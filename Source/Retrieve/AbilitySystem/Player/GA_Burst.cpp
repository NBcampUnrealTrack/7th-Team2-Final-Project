#include "AbilitySystem/Player/GA_Burst.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/ElementGaugeComponent.h"
#include "Components/PlayerBurstComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "UI/HUD/RetrieveBuffUIBroadcastComponent.h"

UGA_Burst::UGA_Burst()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Burst);
	SetAssetTags(Tags);

	ActivationRequiredTags.AddTag(RetrieveGameplayTags::State_Gauge_Full);

	// 공중/점프 중 버스트 불가
	bBlockActivationWhileAirborne = true;

	// 상태 게이트(사망/피격/회피) 사용 불가
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Bursting);

	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Dash);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
}

void UGA_Burst::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	if (!SkillCombinationTable) return;

	static const FString Context(TEXT("GA_Burst::OnAvatarSet"));
	TArray<FSkillCombination*> Rows;
	SkillCombinationTable->GetAllRows<FSkillCombination>(Context, Rows);
	for (FSkillCombination* Row : Rows)
	{
		if (Row && !Row->AttackMontage.IsNull())
		{
			Row->AttackMontage.LoadSynchronous();
		}
	}
}

void UGA_Burst::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Avatar) || !IsValid(ASC) || !SkillCombinationTable)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UElementGaugeComponent* Gauge = Avatar->FindComponentByClass<UElementGaugeComponent>();
	if (!IsValid(Gauge))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TMap<FGameplayTag, int32> ElementPattern = Gauge->GetCurrentCombination();

	const FSkillCombination* MatchedRow = FindMatchingCombination(ElementPattern);
	if (!MatchedRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Burst] No matching combination found"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[GA_Burst] Skill=%s"), *MatchedRow->DisplayName.ToString());

	// 시전 중 이동/회전 잠금 (스킬 타입별 데이터)
	ApplyCastLockTags(MatchedRow);

	Gauge->ClearSlot();

	UPlayerBurstComponent* BurstComp = Avatar->FindComponentByClass<UPlayerBurstComponent>();
	if (!IsValid(BurstComp))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Burst] PlayerBurstComponent not found on %s"), *Avatar->GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimMontage* Montage = MatchedRow->AttackMontage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Burst] AttackMontage is null for Skill=%s"), *MatchedRow->DisplayName.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FRetrieveElementGaugeBurstPayload BurstPayload;
	BurstPayload.Instigator = Avatar;
	BurstPayload.ElementPattern = ElementPattern;
	if (UWorld* World = Avatar->GetWorld())
	{
		UGameplayMessageSubsystem::Get(World)
			.BroadcastMessage(RetrieveGameplayTags::Channel_ElementGauge_Burst, BurstPayload);
	}

	Gauge->ClearSlot();

	CachedBurstComp = BurstComp;
	BurstComp->BeginBurstSkill(MatchedRow);

	if (MatchedRow->BurstUITag.IsValid())
	{
		if (URetrieveBuffUIBroadcastComponent* BuffUI =
			Avatar->FindComponentByClass<URetrieveBuffUIBroadcastComponent>())
		{
			BuffUI->BroadcastBuffManual(MatchedRow->BurstUITag);
		}
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.f, NAME_None, true);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	MontageTask->ReadyForActivation();
}

void UGA_Burst::HandleMontageCompleted()
{
	UE_LOG(LogTemp, Log, TEXT("[GA_Burst] Montage Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Burst::HandleMontageBlendOut()
{
	UE_LOG(LogTemp, Log, TEXT("[GA_Burst] Montage BlendOut"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Burst::HandleMontageInterrupted()
{
	UE_LOG(LogTemp, Warning, TEXT("[GA_Burst] Montage Interrupted"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Burst::HandleMontageCancelled()
{
	UE_LOG(LogTemp, Warning, TEXT("[GA_Burst] Montage Cancelled"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Burst::CleanupBurst()
{
	RemoveCastLockTags();

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (IsValid(CachedBurstComp))
	{
		CachedBurstComp->EndBurstSkill();
	}
	CachedBurstComp = nullptr;
}

void UGA_Burst::ApplyCastLockTags(const FSkillCombination* Combo)
{
	RemoveCastLockTags();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Combo || !IsValid(ASC))
	{
		return;
	}

	if (Combo->bLockMovementDuringCast)
	{
		AppliedCastLockTags.AddTag(RetrieveGameplayTags::Animation_Lock_Movement);
	}
	if (Combo->bLockRotationDuringCast)
	{
		AppliedCastLockTags.AddTag(RetrieveGameplayTags::Animation_Lock_Rotation);
	}

	for (const FGameplayTag& Tag : AppliedCastLockTags)
	{
		ASC->AddLooseGameplayTag(Tag);
	}
}

void UGA_Burst::RemoveCastLockTags()
{
	if (AppliedCastLockTags.IsEmpty())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		for (const FGameplayTag& Tag : AppliedCastLockTags)
		{
			ASC->RemoveLooseGameplayTag(Tag);
		}
	}
	AppliedCastLockTags.Reset();
}

void UGA_Burst::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 중복 호출 방지: OnBlendOut + OnCompleted 가 둘 다 발동되는 정상 케이스에서
	// EndAbility 가 두 번 불려도 cleanup/로그가 한 번만 실행되도록 가드.
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}

	CleanupBurst();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Burst::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	CleanupBurst();

	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

const FSkillCombination* UGA_Burst::FindMatchingCombination(const TMap<FGameplayTag, int32>& ElementPattern) const
{
	if (!SkillCombinationTable) return nullptr;

	static const FString Context(TEXT("GA_Burst::FindMatchingCombination"));
	TArray<FSkillCombination*> Rows;
	SkillCombinationTable->GetAllRows<FSkillCombination>(Context, Rows);

	for (const FSkillCombination* Row : Rows)
	{
		if (Row && DoesCombinationMatch(Row->ElementPattern, ElementPattern))
		{
			return Row;
		}
	}
	return nullptr;
}

bool UGA_Burst::DoesCombinationMatch(const TMap<FGameplayTag, int32>& TablePattern, const TMap<FGameplayTag, int32>& CurrentPattern)
{
	if (TablePattern.Num() != CurrentPattern.Num()) return false;

	for (const TPair<FGameplayTag, int32>& Pair : TablePattern)
	{
		const int32* ElementCount = CurrentPattern.Find(Pair.Key);
		if (!ElementCount || *ElementCount != Pair.Value)
		{
			return false;
		}
	}
	return true;
}
