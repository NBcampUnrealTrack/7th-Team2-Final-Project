#include "Components/Pawn/RetrievePawnExtensionComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Character/RetrievePawnData.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Core/RetrieveGameState.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayEffect.h"
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
	URetrieveAbilitySystemComponent* ASC = AbilitySystemComponent;
	AActor* Owner = GetOwner();

	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = nullptr;
		return;
	}

	// 재진입 방어: 정리 중 다시 들어오면 이미 nullptr로 보이게 한다.
	AbilitySystemComponent = nullptr;

	GrantedHandles.TakeFromAbilitySystem(ASC);

	if (IsValid(ASC) && ASC->GetAvatarActor_Direct() == Owner)
	{
		ASC->ClearActorInfo();
	}
}

void URetrievePawnExtensionComponent::HandleControllerChanged()
{
	if (AbilitySystemComponent && AbilitySystemComponent->GetAvatarActor_Direct() == GetOwner())
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

	UCombatAttributeSet* AttributeSet =
		const_cast<UCombatAttributeSet*>(AbilitySystemComponent->GetSet<UCombatAttributeSet>());

	if (!AttributeSet)
	{
		return;
	}

	// 스탯의 단일 출처 = FCharacterStats. 행이 지정/존재하지 않으면 구조체 기본값으로 초기화
	FCharacterStats DefaultStats;
	const FCharacterStats* Row = &DefaultStats;
	if (!PawnData->CharacterStatsRow.IsNone() && PawnData->CharacterStatsTable)
	{
		if (PawnData->bScalesWithWorldLevel)
		{
			// 스케일 폰: CharacterStatsTable을 FLeveledCharacterStats 테이블로 간주하고 월드 레벨로 인덱싱
			if (const FLeveledCharacterStats* Leveled = PawnData->CharacterStatsTable->FindRow<FLeveledCharacterStats>(
				PawnData->CharacterStatsRow, TEXT("URetrievePawnExtensionComponent::ApplyCharacterStatsRow(Leveled)")))
			{
				if (Leveled->ByLevel.Num() > 0)
				{
					const int32 WorldLevel = ARetrieveGameState::GetWorldLevelFor(this);
					const int32 Index = FMath::Clamp(WorldLevel - 1, 0, Leveled->ByLevel.Num() - 1);
					Row = &Leveled->ByLevel[Index];
				}
			}
		}
		else if (const FCharacterStats* Found = PawnData->CharacterStatsTable->FindRow<FCharacterStats>(
			PawnData->CharacterStatsRow, TEXT("URetrievePawnExtensionComponent::ApplyCharacterStatsRow")))
		{
			Row = Found;
		}
	}

	// GE_InitStats가 지정된 경우: SetByCaller로 전 어트리뷰트 주입
	if (PawnData->InitStatsEffect)
	{
		FGameplayEffectSpecHandle SpecHandle =
			AbilitySystemComponent->MakeOutgoingSpec(PawnData->InitStatsEffect, 1.f, AbilitySystemComponent->MakeEffectContext());

		if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
		{
			FGameplayEffectSpec& Spec = *SpecHandle.Data.Get();
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_MaxHealth, Row->MaxHealth);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_Health, Row->MaxHealth);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_AttackPower, Row->AttackPower);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_Defense, Row->Defense);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_MoveSpeed, Row->MoveSpeed);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_IncomingDamageMultiplier, Row->IncomingDamageMultiplier);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_GuardDamageReduction, Row->GuardDamageReduction);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_AttackSpeedMultiplier, Row->AttackSpeedMultiplier);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_NormalAttackMul, Row->NormalAttackDamageMultiplier);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_HeavyAttackMul, Row->HeavyAttackDamageMultiplier);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_ElementalMul, Row->ElementalDamageMultiplier);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_CritChance, Row->CriticalChance);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_CritDamageMul, Row->CriticalDamageMultiplier);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_LifeSteal, Row->LifeStealRatio);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_ElementChargeGainMul, Row->ElementChargeGainMultiplier);
			Spec.SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Init_OutgoingMul, Row->OutgoingDamageMultiplier);

			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(Spec);
		}
	}
	else
	{
		// GE 미할당 폴백: 데이터 값으로 전 어트리뷰트 직접 세팅
		AttributeSet->SetMaxHealth(Row->MaxHealth);
		AttributeSet->SetHealth(Row->MaxHealth);
		AttributeSet->SetAttackPower(Row->AttackPower);
		AttributeSet->SetDefense(Row->Defense);
		AttributeSet->SetMoveSpeed(Row->MoveSpeed);
		AttributeSet->SetIncomingDamageMultiplier(Row->IncomingDamageMultiplier);
		AttributeSet->SetGuardDamageReduction(Row->GuardDamageReduction);
		AttributeSet->SetAttackSpeedMultiplier(Row->AttackSpeedMultiplier);
		AttributeSet->SetNormalAttackDamageMultiplier(Row->NormalAttackDamageMultiplier);
		AttributeSet->SetHeavyAttackDamageMultiplier(Row->HeavyAttackDamageMultiplier);
		AttributeSet->SetElementalDamageMultiplier(Row->ElementalDamageMultiplier);
		AttributeSet->SetCriticalChance(Row->CriticalChance);
		AttributeSet->SetCriticalDamageMultiplier(Row->CriticalDamageMultiplier);
		AttributeSet->SetLifeStealRatio(Row->LifeStealRatio);
		AttributeSet->SetElementChargeGainMultiplier(Row->ElementChargeGainMultiplier);
		AttributeSet->SetOutgoingDamageMultiplier(Row->OutgoingDamageMultiplier);
		// 스태미너는 UStaminaComponent가 URetrieveStaminaSettings로 초기화한다(여기서 하지 않음).
	}

	// PIE 검증을 위한 핵심 초기화 로그 출력
	UE_LOG(LogTemp, Log, TEXT("[StatsInit] Pawn=%s Row=%s HP=%.1f/%.1f ATK=%.1f Stamina=%.1f/%.1f"),
		*GetNameSafe(GetOwner()),
		*PawnData->CharacterStatsRow.ToString(),
		AttributeSet->GetHealth(), AttributeSet->GetMaxHealth(),
		AttributeSet->GetAttackPower(),
		AttributeSet->GetStamina(), AttributeSet->GetMaxStamina());
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
