#include "Components/RetrieveHeroComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h"
#include "RetrievePawnExtensionComponent.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Character/RetrievePawnData.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Input/RetrieveInputComponent.h"
#include "Input/RetrieveInputConfig.h"

const FName URetrieveHeroComponent::NAME_ActorFeatureName("Hero");
const FName URetrieveHeroComponent::NAME_BindInputsNow("BindInputsNow");

URetrieveHeroComponent::URetrieveHeroComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveHeroComponent::OnRegister()
{
	Super::OnRegister();
	if (!GetPawn<APawn>())
	{
		return;
	}
	RegisterInitStateFeature();
}

void URetrieveHeroComponent::BeginPlay()
{
	Super::BeginPlay();
	BindOnActorInitStateChanged(URetrievePawnExtensionComponent::NAME_ActorFeatureName, FGameplayTag(), false);
	ensure(TryToChangeInitState(RetrieveGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

void URetrieveHeroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterInitStateFeature();
	Super::EndPlay(EndPlayReason);
}

bool URetrieveHeroComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState,
	FGameplayTag DesiredState) const
{
	check(Manager)
		APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return false;
	}

	if (!CurrentState.IsValid() && DesiredState == RetrieveGameplayTags::InitState_Spawned)
	{
		return true;
	}

	if (CurrentState == RetrieveGameplayTags::InitState_Spawned && DesiredState == RetrieveGameplayTags::InitState_DataAvailable)
	{
		if (!Pawn->GetPlayerState())
		{
			return false;
		}
		if (Pawn->IsLocallyControlled())
		{
			const APlayerController* PlayerController = GetController<APlayerController>();
			if (!PlayerController || !PlayerController->GetLocalPlayer())
			{
				return false;
			}
			return true;
		}
	}

	if (CurrentState == RetrieveGameplayTags::InitState_DataAvailable && DesiredState == RetrieveGameplayTags::InitState_DataInitialized)
	{
		return Manager->HasFeatureReachedInitState(Pawn, URetrievePawnExtensionComponent::NAME_ActorFeatureName, RetrieveGameplayTags::InitState_DataInitialized);
	}

	if (CurrentState == RetrieveGameplayTags::InitState_DataInitialized && DesiredState == RetrieveGameplayTags::InitState_GameplayReady)
	{
		return true;
	}

	return false;
}

void URetrieveHeroComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState,
	FGameplayTag DesiredState)
{
	if (CurrentState == RetrieveGameplayTags::InitState_DataAvailable && DesiredState == RetrieveGameplayTags::InitState_DataInitialized)
	{
		APawn* Pawn = GetPawn<APawn>();
		if (!Pawn || !Pawn->IsLocallyControlled())
		{
			return;
		}

		UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(Pawn, NAME_BindInputsNow);
		BindPlayerInputs();
	}
}

void URetrieveHeroComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName == URetrievePawnExtensionComponent::NAME_ActorFeatureName && Params.FeatureState == RetrieveGameplayTags::InitState_DataInitialized)
	{
		CheckDefaultInitialization();
	}
}

void URetrieveHeroComponent::CheckDefaultInitialization()
{
	static const TArray<FGameplayTag> StateChain = {
		RetrieveGameplayTags::InitState_Spawned,
		RetrieveGameplayTags::InitState_DataAvailable,
		RetrieveGameplayTags::InitState_DataInitialized,
		RetrieveGameplayTags::InitState_GameplayReady
	};
	ContinueInitStateChain(StateChain);
}

void URetrieveHeroComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);
	// DataInitialized에 도달하기 전에 SetupPlayerInputComponent가 호출될 수 있음
	// InputComponent를 캐시해두고 PawnExtensionComponent가 준비되면 바인딩
	PendingInputComponent = PlayerInputComponent;
	BindPlayerInputs();
}

void URetrieveHeroComponent::BindPlayerInputs()
{
	if (bInputsBound) return;
	if (!PendingInputComponent) return;

	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn || !Pawn->IsLocallyControlled()) return;

	const APlayerController* PlayerController = GetController<APlayerController>();
	if (!PlayerController) return;

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	if (!LocalPlayer) return;

	URetrievePawnExtensionComponent* PawnExtensionComponent = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn);
	if (!PawnExtensionComponent) return;

	const URetrievePawnData* PawnData = PawnExtensionComponent->GetPawnData();
	if (!PawnData) return;

	const URetrieveInputConfig* InputConfig = PawnData->InputConfig;
	if (!InputConfig) return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem) return;
	Subsystem->ClearAllMappings();

	if (PawnData->DefaultMappingContext)
	{
		Subsystem->AddMappingContext(PawnData->DefaultMappingContext, PawnData->DefaultMappingPriority);
	}

	URetrieveInputComponent* RetrieveIC = Cast<URetrieveInputComponent>(PendingInputComponent);
	if (!RetrieveIC) return;

	TArray<uint32> BindHandles;
	RetrieveIC->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, BindHandles);

	RetrieveIC->BindNativeAction(InputConfig, RetrieveGameplayTags::Input_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move, false);
	RetrieveIC->BindNativeAction(InputConfig, RetrieveGameplayTags::Input_Look, ETriggerEvent::Triggered, this, &ThisClass::Input_Look, false);

	bInputsBound = true;
}

void URetrieveHeroComponent::Input_Move(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;

	const FVector2D Value = InputActionValue.Get<FVector2D>();
	const FRotator MovementRotation(0.0f, Pawn->GetControlRotation().Yaw, 0.0f);

	const FVector ForwardDirection = MovementRotation.RotateVector(FVector::ForwardVector);
	const FVector RightDirection = MovementRotation.RotateVector(FVector::RightVector);

	if (Value.X != 0.0f)
	{
		Pawn->AddMovementInput(RightDirection, Value.X);
	}

	if (Value.Y != 0.0f)
	{
		Pawn->AddMovementInput(ForwardDirection, Value.Y);
	}

	// GA_Dash 등 방향성 어빌리티가 참조할 카메라(컨트롤) Yaw 기반 입력 방향을 캐시
	// 입력이 0이면 영 벡터가 저장되며, 호출부에서 ActorForward로 Fallback 처리
	CachedMoveInputVector = (ForwardDirection * Value.Y + RightDirection * Value.X).GetSafeNormal();
}

void URetrieveHeroComponent::Input_Look(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}
	// 락온 중에는 카메라가 타겟을 자동 추적하므로 마우스 Look 입력 무시
	const IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(Pawn);
	UAbilitySystemComponent* ASC = ASCInterface ? ASCInterface->GetAbilitySystemComponent() : nullptr;
	if (ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::LockOn_Active))
	{
		return;
	}

	const FVector2D Value = InputActionValue.Get<FVector2D>();

	if (Value.X != 0.0f)
	{
		Pawn->AddControllerYawInput(Value.X);
	}

	if (Value.Y != 0.0f)
	{
		Pawn->AddControllerPitchInput(Value.Y);
	}
}

void URetrieveHeroComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	if (InputTag == RetrieveGameplayTags::Ability_Player_Jump)
	{
		if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl))
			{
				return;
			}
		}
	}

	if (URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent())
		{
			ASC->AbilityInputTagPressed(InputTag);
		}
	}
}

void URetrieveHeroComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	if (URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent())
		{
			ASC->AbilityInputTagReleased(InputTag);
		}
	}
}