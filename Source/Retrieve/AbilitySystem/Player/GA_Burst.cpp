#include "AbilitySystem/Player/GA_Burst.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Character/RetrieveAlsCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/Player/PlayerBurstComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "TimerManager.h"
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

	// 착지 처리 바인딩 조건: 슬램(AoE/넉백)은 bDoLandingImpact가, 착지 정지는 다이브 페이즈가 요구.
	// 슬램 파트는 캐시값(Section/Radius)으로 자체 게이팅되어, 다이브만인 WindBurst는 정지만·슬램 no-op.
	bLandingHandled = false;
	bLandingImpactEnabled = MatchedRow->bDoLandingImpact || BurstComp->WantsDiveLanding();
	CachedLandingSection = MatchedRow->LandingSectionName;
	CachedLandingRadius = MatchedRow->LandingAoeRadius;
	CachedLandingDamageMul = MatchedRow->LandingDamageMultiplier;
	bCachedUseLandingKnockback = MatchedRow->bUseLandingKnockback;
	bCachedExcludeBoss = MatchedRow->bExcludeBossFromKnockback;
	CachedLandingKnockback = MatchedRow->LandingKnockback;
	CachedLandingVFX = MatchedRow->LandingVFX;
	CachedLandingVFXScale = MatchedRow->LandingVFXScale;
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

		// 안전 타임아웃: 끝내 착지 못 하면(낭떠러지 + 루프 몽타주는 완료 이벤트도 없음) 무한 대기 방지.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				LandingWaitTimeoutHandle,
				FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					bWaitingForLanding = false;
					EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
				}),
				LandingSafetyTimeout, false);
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
	FinishOrWaitForLanding();
}

void UGA_Burst::HandleMontageBlendOut()
{
	FinishOrWaitForLanding();
}

bool UGA_Burst::ShouldWaitForDiveLanding() const
{
	// 다이브가 이미 발사됐는데 아직 착지 처리가 안 됐다 = 몽타주가 물리 착지보다 먼저 끝난 상황.
	return bLandingImpactEnabled
		&& !bLandingHandled
		&& IsValid(CachedBurstComp)
		&& CachedBurstComp->WantsDiveLanding()
		&& CachedBurstComp->HasDiveLaunched();
}

void UGA_Burst::FinishOrWaitForLanding()
{
	// 몽타주가 물리 착지보다 먼저 끝났으면, 실제 착지(HandleLanded)까지 EndAbility를 미룬다.
	if (ShouldWaitForDiveLanding())
	{
		bWaitingForLanding = true;
		return;
	}

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

	bWaitingForLanding = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LandingWaitTimeoutHandle);
	}
	
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

	// 잔여 속도 제거(중간/진짜 착지 매번). StopMovementImmediately는 Velocity만 0으로 만들어,
	// 엔진 ProcessLanded가 같은 프레임에 굴리는 StartNewPhysics가 잔존 Acceleration으로 재가속 → 미끄러짐.
	// StopActiveMovement로 Acceleration까지 비운다.
	if (ACharacter* LandedChar = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* MoveComp = LandedChar->GetCharacterMovement())
		{
			MoveComp->StopActiveMovement();      // Acceleration / RequestedVelocity 제거
			MoveComp->StopMovementImmediately(); // Velocity 제거
		}
	}

	// 다이브가 예정됐는데 아직 안 나갔으면 상승 아크의 '중간 착지'다. 여기서 확정하면 진짜 다이브 착지가
	// 무시되므로(섹션점프·AoE 조기 발동) 미룬다. 속도 정리는 위에서 이미 했다.
	if (IsValid(CachedBurstComp) && CachedBurstComp->WantsDiveLanding() && !CachedBurstComp->HasDiveLaunched())
	{
		return;
	}

	bLandingHandled = true;
	UnbindLanded();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LandingWaitTimeoutHandle);
	}

	// 착지 섹션으로 점프(설정 시). 다이브 낙하 섹션을 착지까지 루프시켜 두면 지형 높이와 무관하게 여기서 전환.
	if (!CachedLandingSection.IsNone())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->CurrentMontageJumpToSection(CachedLandingSection);
		}
	}

	// 착지 VFX는 몽타주 노티(고정 타임라인 → 공중 재생) 대신 실제 착지에 스폰.
	SpawnLandingVFX();

	// 착지 AoE + 방사 넉백(보스 제외)
	if (CachedLandingRadius > 0.f && IsValid(CachedBurstComp))
	{
		const AActor* Avatar = GetAvatarActorFromActorInfo();
		const FVector Center = IsValid(Avatar) ? Avatar->GetActorLocation() : FVector::ZeroVector;
		CachedBurstComp->ApplyLandingImpact(Center, CachedLandingRadius, CachedLandingDamageMul,
			bCachedUseLandingKnockback, CachedLandingKnockback, bCachedExcludeBoss);
	}

	// 몽타주가 먼저 끝나 대기 중이었다면, 착지를 처리했으니 이제 종료.
	if (bWaitingForLanding)
	{
		bWaitingForLanding = false;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGA_Burst::SpawnLandingVFX()
{
	UNiagaraSystem* VFX = CachedLandingVFX.LoadSynchronous();
	if (!VFX)
	{
		return;
	}

	const ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = Char ? Char->GetWorld() : nullptr;
	if (!IsValid(Char) || !IsValid(World))
	{
		return;
	}

	// 발밑(캡슐 하단) 기준으로 스폰 → 지면에 밀착.
	FVector FeetLocation = Char->GetActorLocation();
	if (const UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
	{
		FeetLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, VFX, FeetLocation, FRotator::ZeroRotator,
		FVector(CachedLandingVFXScale), /*bAutoDestroy=*/true, /*bAutoActivate=*/true,
		ENCPoolMethod::AutoRelease);
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
