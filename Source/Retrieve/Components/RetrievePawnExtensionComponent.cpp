#include "Components/RetrievePawnExtensionComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Character/RetrievePawnData.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Net/UnrealNetwork.h"

const FName URetrievePawnExtensionComponent::NAME_ActorFeatureName("PawnExtension");

URetrievePawnExtensionComponent::URetrievePawnExtensionComponent(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URetrievePawnExtensionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URetrievePawnExtensionComponent, PawnData);
}

void URetrievePawnExtensionComponent::OnRegister()
{
	Super::OnRegister();

	TArray<UActorComponent*> Found;
	GetOwner()->GetComponents(URetrievePawnExtensionComponent::StaticClass(), Found);
	ensureAlwaysMsgf(Found.Num() == 1, TEXT("Only one URetrievePawnExtensionComponent should exist on a pawn."));

	RegisterInitStateFeature();
}

void URetrievePawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);
	ensure(TryToChangeInitState(RetrieveGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

void URetrievePawnExtensionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterInitStateFeature();
	UninitializeAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void URetrievePawnExtensionComponent::SetPawnData(const URetrievePawnData* InPawnData)
{
	APawn* Pawn = GetPawn<APawn>();
	if (!Pawn || !Pawn->HasAuthority())
	{
		return;
	}
	if (PawnData)
	{
		return;
	}

	PawnData = InPawnData;
	Pawn->ForceNetUpdate();
	CheckDefaultInitialization();
}

void URetrievePawnExtensionComponent::OnRep_PawnData()
{
	CheckDefaultInitialization();
}

void URetrievePawnExtensionComponent::InitializeAbilitySystem(URetrieveAbilitySystemComponent* InASC,
                                                              AActor* InOwnerActor)
{
	check(InASC);
	check(InOwnerActor);

	if (AbilitySystemComponent == InASC)
	{
		return;
	}
	if (AbilitySystemComponent)
	{
		UninitializeAbilitySystem();
	}

	APawn* Pawn = GetPawnChecked<APawn>();
	AbilitySystemComponent = InASC;
	AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor, Pawn);

	CheckDefaultInitialization();
}

void URetrievePawnExtensionComponent::UninitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	GrantedHandles.TakeFromAbilitySystem(AbilitySystemComponent);

	if (AbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		AbilitySystemComponent->ClearActorInfo();
	}
	AbilitySystemComponent = nullptr;
}

void URetrievePawnExtensionComponent::HandleControllerChanged()
{
	if (AbilitySystemComponent && AbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		AbilitySystemComponent->RefreshAbilityActorInfo();
	}
	CheckDefaultInitialization();
}

void URetrievePawnExtensionComponent::HandlePlayerStateReplicated()
{
	CheckDefaultInitialization();
}

void URetrievePawnExtensionComponent::SetupPlayerInputComponent()
{
	CheckDefaultInitialization();
}

void URetrievePawnExtensionComponent::OnAbilitySystemInitialized_RegisterAndCall(
	FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!OnAbilitySystemInitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		OnAbilitySystemInitialized.Add(Delegate);
	}

	if (AbilitySystemComponent)
	{
		Delegate.Execute();
	}
}

bool URetrievePawnExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager,
                                                         FGameplayTag CurrentState, FGameplayTag DesiredState) const
{
	check(Manager);
	APawn* Pawn = GetPawn<APawn>();

	if (!CurrentState.IsValid() || DesiredState == RetrieveGameplayTags::InitState_Spawned)
	{
		return Pawn != nullptr;
	}

	if (CurrentState == RetrieveGameplayTags::InitState_Spawned && DesiredState ==
		RetrieveGameplayTags::InitState_DataAvailable)
	{
		if (!PawnData)
		{
			return false;
		}

		if (Pawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			const bool bHasAuthority = Pawn->HasAuthority();
			const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
			if (bHasAuthority || bIsLocallyControlled)
			{
				if (!GetController<AController>())
				{
					return false;
				}
			}
		}
		return true;
	}

	if (CurrentState == RetrieveGameplayTags::InitState_DataAvailable && DesiredState ==
		RetrieveGameplayTags::InitState_DataInitialized)
	{
		if (PawnData && !PawnData->bRequiresAbilitySystem)
		{
			return true;
		}
		return AbilitySystemComponent != nullptr;
	}

	if (CurrentState == RetrieveGameplayTags::InitState_DataInitialized && DesiredState ==
		RetrieveGameplayTags::InitState_GameplayReady)
	{
		return Manager->HaveAllFeaturesReachedInitState(GetOwner(), RetrieveGameplayTags::InitState_DataInitialized);
	}

	return false;
}

void URetrievePawnExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager,
                                                            FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	if (CurrentState == RetrieveGameplayTags::InitState_DataAvailable && DesiredState ==
		RetrieveGameplayTags::InitState_DataInitialized)
	{
		APawn* Pawn = GetPawn<APawn>();
		if (!Pawn || !Pawn->HasAuthority())
		{
			return;
		}
		if (!PawnData)
		{
			return;
		}

		// Lumen 폰은 초기화 핸드셰이크에 참여하지만 ASC가 없음. DataInitialized 이벤트만 발생하면 됨
		if (!PawnData->bRequiresAbilitySystem)
		{
			return;
		}

		if (!AbilitySystemComponent)
		{
			return;
		}

		// 1. AbilitySet 부여. ASC에 UCombatAttributeSet이 스폰됨
		if (PawnData->DefaultAbilitySet)
		{
			PawnData->DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedHandles, GetOwner());
		}

		// 2. PawnData에 행(row)이 지정되어 있다면 DT_CharacterStats에서 MaxHealth를 적용
		// AttributeSet은 기본값으로 생성된 상태이므로, 이 단계에서 해당 값을 데이터로 덮어씀
		ApplyCharacterStatsRow();
		
		// 3. ASC + UCombatAttributeSet + 초기 스탯이 모두 준비됨
		OnAbilitySystemInitialized.Broadcast();
	}
}

void URetrievePawnExtensionComponent::ApplyCharacterStatsRow()
{
	if (!PawnData || !AbilitySystemComponent)
	{
		return;
	}

	if (PawnData->CharacterStatsRow.IsNone() || !PawnData->CharacterStatsTable)
	{
		return;
	}

	const FCharacterStats* Row = PawnData->CharacterStatsTable->FindRow<FCharacterStats>(
		PawnData->CharacterStatsRow, TEXT("URetrievePawnExtensionComponent::ApplyCharacterStatsRow"));

	if (!Row)
	{
		return;
	}

	if (UCombatAttributeSet* AttributeSet = const_cast<UCombatAttributeSet*>(AbilitySystemComponent->GetSet<
		UCombatAttributeSet>()))
	{
		AttributeSet->SetMaxHealth(Row->MaxHealth);
		AttributeSet->SetHealth(Row->MaxHealth);
	}
}

void URetrievePawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName != NAME_ActorFeatureName)
	{
		CheckDefaultInitialization();
	}
}

void URetrievePawnExtensionComponent::CheckDefaultInitialization()
{
	static const TArray<FGameplayTag> StateChain = {
		RetrieveGameplayTags::InitState_Spawned,
		RetrieveGameplayTags::InitState_DataAvailable,
		RetrieveGameplayTags::InitState_DataInitialized,
		RetrieveGameplayTags::InitState_GameplayReady,
	};

	ContinueInitStateChain(StateChain);
}
