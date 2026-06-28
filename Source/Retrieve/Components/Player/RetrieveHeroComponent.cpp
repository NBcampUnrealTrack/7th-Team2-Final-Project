#include "Components/Player/RetrieveHeroComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h"
#include "../Pawn/RetrievePawnExtensionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/Pawn/RetrieveCameraBoom.h"
#include "Components/Water/SwimDetectionComponent.h"
#include "Character/RetrieveAlsCharacter.h"
#include "Character/RetrievePawnData.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
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

	const UInputAction* QuickSlot1Action =
		InputConfig->FindNativeInputActionForTag(RetrieveGameplayTags::Input_UseItem_Slot1, false);
	const UInputAction* QuickSlot2Action =
		InputConfig->FindNativeInputActionForTag(RetrieveGameplayTags::Input_UseItem_Slot2, false);

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
	RetrieveIC->BindNativeAction(InputConfig, RetrieveGameplayTags::Input_Zoom, ETriggerEvent::Triggered, this, &ThisClass::Input_Zoom, false);
	if (QuickSlot1Action)
	{
		RetrieveIC->BindAction(
			QuickSlot1Action,
			ETriggerEvent::Started,
			this,
			&ThisClass::Input_UseConsumableSlot,
			UInventoryComponent::QuickSlotPrimaryKey);
	}
	if (QuickSlot2Action)
	{
		RetrieveIC->BindAction(
			QuickSlot2Action,
			ETriggerEvent::Started,
			this,
			&ThisClass::Input_UseConsumableSlot,
			UInventoryComponent::QuickSlotSecondaryKey);
	}
	

	// Sprint (Hold) — Started: 임계 도달 시점 / Completed: 뗌 시점
	RetrieveIC->BindNativeAction(InputConfig, RetrieveGameplayTags::Input_Sprint, ETriggerEvent::Started,   this, &ThisClass::Input_SprintPressed,  false);
	RetrieveIC->BindNativeAction(InputConfig, RetrieveGameplayTags::Input_Sprint, ETriggerEvent::Completed, this, &ThisClass::Input_SprintReleased, false);

	// Crouch (Toggle) — Started에서 한 번만 발동
	RetrieveIC->BindNativeAction(InputConfig, RetrieveGameplayTags::Input_Crouch, ETriggerEvent::Started, this, &ThisClass::Input_CrouchPressed, false);
	RetrieveIC->BindNativeAction(InputConfig, RetrieveGameplayTags::Input_Crouch, ETriggerEvent::Completed, this, &ThisClass::Input_CrouchReleased, false);

	// Dash (Tap) — Tap trigger의 Triggered에 바인딩. 시간 내 떼면 1회 발동, 길게 누르면 발동 안 함(Sprint Hold가 작동).
	RetrieveIC->BindNativeAction(InputConfig, RetrieveGameplayTags::Input_Dash, ETriggerEvent::Triggered, this, &ThisClass::Input_DashRequest, false);

	bInputsBound = true;
}

void URetrieveHeroComponent::Input_Move(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;

	const FVector2D Value = InputActionValue.Get<FVector2D>();

	// 수영 분기: 수중=3D(pitch 포함) / 표면=평면
	if (UAbilitySystemComponent* SwimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
	{
		if (SwimASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming))
		{
			const FRotator ControlRot = Pawn->GetControlRotation();
			const bool bUnderwater = SwimASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming_UnderWater);
			const FVector Fwd   = bUnderwater ? ControlRot.Vector()
			                                  : FRotator(0.f, ControlRot.Yaw, 0.f).Vector();
			const FVector Right = FRotator(0.f, ControlRot.Yaw, 0.f).RotateVector(FVector::RightVector);
			Pawn->AddMovementInput(Fwd,   Value.Y);
			Pawn->AddMovementInput(Right, Value.X);
			CachedMoveInputVector = (Fwd * Value.Y + Right * Value.X).GetSafeNormal();
			return;
		}
	}

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

void URetrieveHeroComponent::Input_Zoom(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	if (IsValid(Pawn) == false)
	{
		return;
	}

	const float Value = InputActionValue.Get<float>();

	URetrieveCameraBoom* Boom = Pawn->FindComponentByClass<URetrieveCameraBoom>();
	if (IsValid(Boom) == false)
	{
		return;
	}
	Boom->AddZoomInput(Value);
}

void URetrieveHeroComponent::Input_UseConsumableSlot(int32 SlotKey)
{
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		if (UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>())
		{
			Inventory->UseConsumableSlot(SlotKey);
		}
	}
}

void URetrieveHeroComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn)
	{
		return;
	}

	// 수영 중 Jump = 상승 트림 (Jump GA 스킵)
	if (InputTag == RetrieveGameplayTags::Ability_Player_Jump)
	{
		if (UAbilitySystemComponent* SwimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
		{
			if (SwimASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming_UnderWater))
			{
				if (USwimDetectionComponent* Swim = Pawn->FindComponentByClass<USwimDetectionComponent>())
				{
					Swim->SetSwimVerticalInput(1.f);
				}
				return;
			}
		}
	}

	// 수면 수영 중 Jump = climb-out (수영=EmptyTag라 인자없는 StartMantling 무반응 → 전용 경로)
	if (InputTag == RetrieveGameplayTags::Ability_Player_Jump)
	{
		if (UAbilitySystemComponent* SwimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
		{
			if (SwimASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming)
				&& !SwimASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming_UnderWater))
			{
				if (ARetrieveAlsCharacter* AlsChar = Cast<ARetrieveAlsCharacter>(Pawn))
				{
					AlsChar->TryMantleFromWater();
				}
				return;
			}
		}
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

	// 좌클릭(Attack) + Shift = 대시어택 Chord. 평타 인텐트 대신 SprintAttack 인텐트로 치환해
	// 평타 폴백을 막는다(대시 실패 시 평타가 나가던 문제 제거). Shift는 Sprint 키와 공유.
	if (InputTag == RetrieveGameplayTags::Ability_Player_Attack)
	{
		// 공중에선 chord를 끈다 — Shift+LMB가 attack을 SprintAttack으로 가로채면 점프어택이 굶고
		// 공중 발동 불가한 SprintAttack이 버퍼에 남아 착지 때 뭉개진다. 대시/방패는 지상 무브.
		const ACharacter* Char = Cast<ACharacter>(Pawn);
		const bool bAirborne = Char && Char->GetCharacterMovement() && Char->GetCharacterMovement()->IsFalling();

		// Guard 중 Attack 입력은 일반 평타가 아니라 GuardAttack으로 치환한다.
		// 단, 공중에서는 GuardAttack을 허용하지 않는다. 공중 Attack은 기존 JumpAttack/Attack 흐름이 처리한다.
		if (!bAirborne)
		{
			if (URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn))
			{
				if (URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent())
				{
					// 패링 성공 후 Attack 입력은 평타/GuardAttack이 아니라 ParryCounter 선택으로 소비한다.
					// State.Player.CanCounter는 CounterWindowEffect가 부여하는 "카운터 가능" 상태다.
					const bool bCanCounter = ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_CanCounter);
					const bool bHasParryCounter = ASC->HasActivatableAbilityWithInputTag(RetrieveGameplayTags::Ability_Player_ParryCounter);
					if (bCanCounter && bHasParryCounter)
					{
						ASC->AbilityInputTagPressed(RetrieveGameplayTags::Ability_Player_ParryCounter);
						return;
					}

					const bool bGuarding = ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Guarding);
					const bool bHasGuardAttack = ASC->HasActivatableAbilityWithInputTag(RetrieveGameplayTags::Ability_Player_GuardAttack);
					if (bGuarding && bHasGuardAttack)
					{
						ASC->AbilityInputTagPressed(RetrieveGameplayTags::Ability_Player_GuardAttack);
						return;
					}
				}
			}
		}

		const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
		if (!bAirborne && PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift)))
		{
			// 대시어택(SprintAttack 인텐트)을 실제로 가진 경우에만 치환. 미보유 클래스(메이지 등)는
			// 평타 경로로 그대로 흘려보낸다 — Shift+LMB 입력이 먹통 되는 것 방지.
			if (URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn))
			{
				if (URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent();
					ASC && ASC->HasActivatableAbilityWithInputTag(RetrieveGameplayTags::Ability_Player_SprintAttack))
				{
					ASC->AbilityInputTagPressed(RetrieveGameplayTags::Ability_Player_SprintAttack);
					return;
				}
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

	// 수영 중 Jump 뗌 = 상승 트림 종료
	if (InputTag == RetrieveGameplayTags::Ability_Player_Jump)
	{
		if (UAbilitySystemComponent* SwimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
		{
			if (SwimASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming))
			{
				if (USwimDetectionComponent* Swim = Pawn->FindComponentByClass<USwimDetectionComponent>())
				{
					Swim->SetSwimVerticalInput(0.f);
				}
				return;
			}
		}
	}

	if (URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent())
		{
			ASC->AbilityInputTagReleased(InputTag);
		}
	}
}

// --- ALS 연결 액션 입력 ---
// Sprint/Crouch는 GAS 상태 태그만 부여/해제. ALS API는 캐릭터의 콜백이 자동 동기화.
// Roll은 즉시성 액션이라 캐릭터 wrapper를 직접 호출 (LooseTag는 ALS LocomotionAction 변화 시 캐릭터가 자동 처리).

void URetrieveHeroComponent::Input_SprintPressed(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;

	if (URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent())
		{
			if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming_UnderWater)) { return; } // 수중 = Sprint 불가(표면 자유형만)
			ASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Player_Sprinting);
			SprintStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
		}
	}
}

void URetrieveHeroComponent::Input_SprintReleased(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;

	if (URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent())
		{
			// Sprinting은 on/off 상태값. SprintAttack 발동 등으로 이미 0일 수 있으므로
			// RemoveLooseGameplayTag(decrement)가 아니라 SetCount(0)으로 비운다(언더플로 워닝 방지, 다른 클리어 지점과 일관).
			ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Sprinting, 0);
			SprintStartTimeSeconds = -1.0;
		}
	}
}

float URetrieveHeroComponent::GetTimeSprintingSeconds() const
{
	if (SprintStartTimeSeconds < 0.0 || !GetWorld())
	{
		return 0.f;
	}
	return static_cast<float>(GetWorld()->GetTimeSeconds() - SprintStartTimeSeconds);
}

void URetrieveHeroComponent::Input_CrouchPressed(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;

	// 수영 중 Crouch = 하강 트림 (Crouching 스탠스 스킵)
	if (UAbilitySystemComponent* SwimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
	{
		if (SwimASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming))
		{
			if (USwimDetectionComponent* Swim = Pawn->FindComponentByClass<USwimDetectionComponent>())
			{
				Swim->SetSwimVerticalInput(-1.f);
			}
			return;
		}
	}

	if (URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Player_Crouching);
		}
	}
}

void URetrieveHeroComponent::Input_CrouchReleased(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;

	// 수영 중 Crouch 뗌 = 하강 트림 종료
	if (UAbilitySystemComponent* SwimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
	{
		if (SwimASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming))
		{
			if (USwimDetectionComponent* Swim = Pawn->FindComponentByClass<USwimDetectionComponent>())
			{
				Swim->SetSwimVerticalInput(0.f);
			}
			return;
		}
	}

	if (URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(RetrieveGameplayTags::State_Player_Crouching);
		}
	}
}

void URetrieveHeroComponent::Input_DashRequest(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn) return;

	// Tap trigger의 Triggered가 키를 짧게 떼는 순간 1회 호출 → GA_Dash 활성화.
	// 길게 누르면 Tap이 시간 초과로 발동 안 함 → IA_Sprint의 Hold trigger가 작동.
	if (URetrievePawnExtensionComponent* PawnExt = URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (URetrieveAbilitySystemComponent* ASC = PawnExt->GetRetrieveAbilitySystemComponent())
		{
			ASC->AbilityInputTagPressed(RetrieveGameplayTags::Ability_Player_Dash);
		}
	}
}

