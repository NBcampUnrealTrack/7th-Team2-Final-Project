#include "AbilitySystem/Player/GA_Burst.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Character/RetrieveAlsCharacter.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/Player/PlayerBurstComponent.h"
#include "Components/Player/WeaponComponent.h"
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
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_ForcedKnockback);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Bursting);

	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Dash);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
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
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(Avatar) || !SkillCombinationTable)
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

	const UWeaponComponent* WeaponComp = Avatar->FindComponentByClass<UWeaponComponent>();
	const FGameplayTag WeaponTypeTag = IsValid(WeaponComp) ? WeaponComp->GetWeaponDataRef().WeaponTypeTag : FGameplayTag();

	// 게이지 슬롯 조합이 아니라 현재 선택된 원소모드로 버스트 스킬을 결정한다.
	const FGameplayTag CurrentElement = ResolveCurrentElementTag();
	const FSkillCombination* MatchedRow = FindBurstForElement(WeaponTypeTag, CurrentElement);

	// 원소는 항상 불/물/바람(보스 해방 시 강화)
	if (!MatchedRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_Burst] No burst row for Weapon=%s Element=%s"), *WeaponTypeTag.ToString(), *CurrentElement.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

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
	BurstPayload.BurstElement = CurrentElement;
	if (UWorld* World = Avatar->GetWorld())
	{
		UGameplayMessageSubsystem& MsgSys = UGameplayMessageSubsystem::Get(World);
		MsgSys.BroadcastMessage(RetrieveGameplayTags::Channel_ElementGauge_Burst, BurstPayload);
	}

	CachedBurstComp = BurstComp;
	BurstComp->BeginBurstSkill(MatchedRow);

	// 착지 슬램 준비(낙법/래그돌 억제 + 섹션점프 + AoE/넉백)
	bLandingHandled = false;
	bLandingImpactEnabled = MatchedRow->bDoLandingImpact;
	CachedLandingSection = MatchedRow->LandingSectionName;
	CachedLandingRadius = MatchedRow->LandingAoeRadius;
	CachedLandingDamageMul = MatchedRow->LandingDamageMultiplier;
	bCachedUseLandingKnockback = MatchedRow->bUseLandingKnockback;
	bCachedExcludeBoss = MatchedRow->bExcludeBossFromKnockback;
	CachedLandingKnockback = MatchedRow->LandingKnockback;
	if (bLandingImpactEnabled)
	{
		if (ARetrieveAlsCharacter* AlsChar = Cast<ARetrieveAlsCharacter>(Avatar))
		{
			AlsChar->SetSuppressLandingRoll(true);
		}
		if (ACharacter* LandChar = Cast<ACharacter>(Avatar))
		{
			LandChar->LandedDelegate.AddDynamic(this, &ThisClass::HandleLanded);
			BoundLandedCharacter = LandChar;
		}
	}

	// 돌진형: 시전 중 적(Pawn) 통과, 벽은 막힘
	if (MatchedRow->AttackType == EAttackExecutionType::Dash)
	{
		SetAvatarPawnCollisionIgnored(true);
	}

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
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Burst::HandleMontageBlendOut()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Burst::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Burst::HandleMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Burst::CleanupBurst()
{
	UnbindLanded();
	SetAvatarPawnCollisionIgnored(false);
	
	if (bLandingImpactEnabled && !bLandingHandled)
	{
		if (ARetrieveAlsCharacter* AlsChar = Cast<ARetrieveAlsCharacter>(GetAvatarActorFromActorInfo()))
		{
			AlsChar->SetSuppressLandingRoll(false);
		}
	}
	bLandingImpactEnabled = false;

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

void UGA_Burst::HandleLanded(const FHitResult& Hit)
{
	if (bLandingHandled)
	{
		return;
	}
	bLandingHandled = true;
	UnbindLanded();

	// 잔여 다이브 속도 제거 → 그 자리에 정지
	if (ACharacter* LandedChar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* MoveComp = LandedChar->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}
	}

	// 착지 섹션으로 점프(설정 시)
	if (!CachedLandingSection.IsNone())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->CurrentMontageJumpToSection(CachedLandingSection);
		}
	}

	// 착지 AoE + 방사 넉백(보스 제외)
	if (CachedLandingRadius > 0.f && IsValid(CachedBurstComp))
	{
		const AActor* Avatar = GetAvatarActorFromActorInfo();
		const FVector Center = IsValid(Avatar) ? Avatar->GetActorLocation() : FVector::ZeroVector;
		CachedBurstComp->ApplyLandingImpact(Center, CachedLandingRadius, CachedLandingDamageMul,
			bCachedUseLandingKnockback, CachedLandingKnockback, bCachedExcludeBoss);
	}
}

void UGA_Burst::UnbindLanded()
{
	if (ACharacter* Character = BoundLandedCharacter.Get())
	{
		Character->LandedDelegate.RemoveDynamic(this, &ThisClass::HandleLanded);
	}
	BoundLandedCharacter = nullptr;
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

const FSkillCombination* UGA_Burst::FindBurstForElement(const FGameplayTag& WeaponType, const FGameplayTag& Element) const
{
	if (!SkillCombinationTable) return nullptr;

	static const FString Context(TEXT("GA_Burst::FindBurstForElement"));
	TArray<FSkillCombination*> Rows;
	SkillCombinationTable->GetAllRows<FSkillCombination>(Context, Rows);

	for (const FSkillCombination* Row : Rows)
	{
		if (Row && Row->WeaponType == WeaponType && Row->BurstElement == Element)
		{
			return Row;
		}
	}
	return nullptr;
}
