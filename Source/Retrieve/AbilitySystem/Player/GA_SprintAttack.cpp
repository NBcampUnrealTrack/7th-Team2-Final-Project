#include "AbilitySystem/Player/GA_SprintAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "Animation/RetrieveWeaponSockets.h"
#include "Components/Player/RetrieveHeroComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/AttackComboDefinition.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"

UGA_SprintAttack::UGA_SprintAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_SprintAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);

	ActivationRequiredTags.AddTag(RetrieveGameplayTags::State_Player_Sprinting);

	bBlockActivationWhileAirborne = true;

	// 구르기/맨틀 등 ALS 액션 중 질주공격 발동 불가. 단, 캔슬 윈도우가 허용하면 예외 — CanActivateAbility 참고.
	bBlockedByLocomotionAction = true;

	// 버퍼 사용 + 공격류 우선순위. 어떤 공격에서 질주공격으로 캔슬할 수 있는지는 몽타주 AllowedCancelIntents가 정한다.
	bUseCombatInputBuffer = true;
	CombatInputPriority = 10;

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);

	// TODO(CancelWindow): 캔슬 윈도우 작업에서 SprintAttack이 기존 공격을 끊을 수 있는 타이밍을 게이트한다.
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);

	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
}

bool UGA_SprintAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(WeaponComp) || !WeaponComp->IsEquipped())
	{
		return false;
	}

	const FRetrieveWeaponDataRow& WeaponData = WeaponComp->GetWeaponDataRef();
	if (WeaponData.AttackComboDefinition.IsNull())
	{
		return false;
	}

	const URetrieveHeroComponent* Hero = URetrieveHeroComponent::FindHeroComponent(AvatarActor);
	if (!Hero || Hero->GetCachedMoveInputDirection().IsNearlyZero(0.1f))
	{
		return false;
	}

	const UAttackComboDefinition* ComboDefinition = WeaponData.AttackComboDefinition.LoadSynchronous();
	const FWeaponSprintAttack* ResolvedSprint = ComboDefinition ? ComboDefinition->ResolveSprintVariant(ResolveCurrentElementTag()) : nullptr;
	if (!ResolvedSprint)
	{
		return false;
	}

	if (ResolvedSprint->RequiredSprintDuration > 0.f && Hero->GetTimeSprintingSeconds() < ResolvedSprint->RequiredSprintDuration)
	{
		return false;
	}

	return IsValid(DamageEffectClass);
}

void UGA_SprintAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	CachedWeaponComponent = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(CachedWeaponComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedWeaponData = CachedWeaponComponent->GetWeaponDataRef();

	// 콤보 정의에서 현재 원소의 Sprint variant 해결 (없으면 기본 variant)
	UAttackComboDefinition* ComboDefinition = CachedWeaponData.AttackComboDefinition.LoadSynchronous();
	const FWeaponSprintAttack* ResolvedSprint = ComboDefinition ? ComboDefinition->ResolveSprintVariant(ResolveCurrentElementTag()) : nullptr;
	if (!ResolvedSprint)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	CachedSprintData = *ResolvedSprint;

	// RequiredSprintDuration 이상 스프린트 유지해야 발동 (커밋 전 검사 → 미달 시 코스트 미소모)
	if (CachedSprintData.RequiredSprintDuration > 0.f)
	{
		const URetrieveHeroComponent* Hero = URetrieveHeroComponent::FindHeroComponent(AvatarActor);
		if (!Hero || Hero->GetTimeSprintingSeconds() < CachedSprintData.RequiredSprintDuration)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// SprintAttack 후에 다시 Sprint + 워밍업을 거쳐야 재발동
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Sprinting, 0);
	}

	UAnimMontage* Montage = CachedSprintData.Montage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ImpactBeginEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RetrieveGameplayTags::GameplayEvent_Attack_Impact_Begin, nullptr, false, true);
	if (ImpactBeginEventTask)
	{
		ImpactBeginEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleImpactBeginEvent);
		ImpactBeginEventTask->ReadyForActivation();
	}

	ImpactEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RetrieveGameplayTags::GameplayEvent_Attack_Impact, nullptr, false, true);
	if (ImpactEventTask)
	{
		ImpactEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleImpactEvent);
		ImpactEventTask->ReadyForActivation();
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, 1.f, CachedSprintData.SectionName, true,
		1.f, 0.f, /*bAllowInterruptAfterBlendOut=*/true);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	MontageTask->ReadyForActivation();
}

void UGA_SprintAttack::HandleImpactBeginEvent(FGameplayEventData Payload)
{
	PreviousTracePoints.Reset();
	bHasValidPreviousTracePoints = false;
	HitActors.Reset();
}

void UGA_SprintAttack::HandleImpactEvent(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}
	ApplyHitDamage();
}

void UGA_SprintAttack::ApplyHitDamage()
{
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(SourceASC) || !IsValid(AvatarActor) || !IsValid(DamageEffectClass))
	{
		return;
	}

	UWorld* World = AvatarActor->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	TArray<FVector> CurrentPoints;
	BuildTracePoints(CurrentPoints);
	if (CurrentPoints.IsEmpty())
	{
		return;
	}

	const float DamageMul = CachedSprintData.DamageMultiplier;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GA_SprintAttack_Impact), false, AvatarActor);
	const float TraceRadius = CachedWeaponData.TraceRadius;
	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(TraceRadius);

	const bool bHasPrev = bHasValidPreviousTracePoints && PreviousTracePoints.Num() == CurrentPoints.Num();

	TArray<FHitResult> AllHits;

	auto SweepSegment = [&](const FVector& SegStart, const FVector& SegEnd)
	{
		TArray<FHitResult> SegmentHits;
		const bool bHit = World->SweepMultiByObjectType(SegmentHits, SegStart, SegEnd, FQuat::Identity, ObjectQueryParams, TraceShape, QueryParams);
		if (bDebugDrawTrace)
		{
			DrawDebugLine(World, SegStart, SegEnd, bHit ? FColor::Green : FColor::Red, false, -1.f, 0, 0.5f);
			DrawDebugSphere(World, SegEnd, TraceRadius, 8, bHit ? FColor::Green : FColor::Red, false, -1.f);
		}
		if (bHit)
		{
			AllHits.Append(MoveTemp(SegmentHits));
		}
	};

	for (int32 i = 0; i + 1 < CurrentPoints.Num(); ++i)
	{
		SweepSegment(CurrentPoints[i], CurrentPoints[i + 1]);
	}

	if (bHasPrev)
	{
		for (int32 i = 0; i < CurrentPoints.Num(); ++i)
		{
			SweepSegment(PreviousTracePoints[i], CurrentPoints[i]);
		}
	}
	else if (CurrentPoints.Num() == 1)
	{
		SweepSegment(CurrentPoints[0], CurrentPoints[0]);
	}

	PreviousTracePoints = CurrentPoints;
	bHasValidPreviousTracePoints = true;

	if (AllHits.IsEmpty())
	{
		return;
	}

	for (const FHitResult& Hit : AllHits)
	{
		AActor* TargetActor = Hit.GetActor();
		if (!IsValid(TargetActor) || TargetActor == AvatarActor) continue;

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (HitActors.Contains(TargetActor)) continue;

		if (TargetASC)
		{
			FGameplayEffectContextHandle PerHitContext = SourceASC->MakeEffectContext();
			PerHitContext.AddInstigator(AvatarActor, AvatarActor);
			PerHitContext.AddSourceObject(this);
			PerHitContext.AddHitResult(Hit, true);

			FGameplayEffectSpecHandle PerHitSpec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), PerHitContext);
			if (!PerHitSpec.IsValid() || !PerHitSpec.Data.IsValid()) continue;

			PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, DamageMul);
			if (CachedSprintData.KnockbackStrength > 0.f)
			{
				PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_Strength, CachedSprintData.KnockbackStrength);
				PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_UpwardStrength, CachedSprintData.KnockbackUpwardStrength);
			}
			PerHitSpec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Normal);

			if (const FGameplayTag ReactTag = HitReactTypeToTag(CachedSprintData.HitReactType); ReactTag.IsValid())
			{
				PerHitSpec.Data->AddDynamicAssetTag(ReactTag);
			}
			
			AddCombatTagsToDamageSpec(
				*PerHitSpec.Data.Get(),
				ResolveCurrentElementTag(),
				RetrieveGameplayTags::Attack_Type_Normal,
				FGameplayTag(),
				HitReactTypeToTag(CachedSprintData.HitReactType));

			SourceASC->ApplyGameplayEffectSpecToTarget(*PerHitSpec.Data.Get(), TargetASC);
			HitActors.Add(TargetActor);

			if (!bChargeBonusGranted)
			{
				const FGameplayTag BonusTag = CachedSprintData.ChargeBonusEventTag;
				if (BonusTag.IsValid())
				{
					FGameplayEventData BonusEvent;
					BonusEvent.Instigator = AvatarActor;
					BonusEvent.Target = TargetActor;
					BonusEvent.EventTag = BonusTag;
					UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(AvatarActor, BonusTag, BonusEvent);
					bChargeBonusGranted = true;
				}
			}
		}
	}
}

void UGA_SprintAttack::BuildTracePoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();

	const FName StartSocket = CachedWeaponData.TraceStartSocketName;
	const FName EndSocket = CachedWeaponData.TraceEndSocketName;

	UMeshComponent* TraceMesh = IsValid(CachedWeaponComponent)
		? CachedWeaponComponent->GetWeaponMeshForTrace(StartSocket, EndSocket)
		: nullptr;

	if (IsValid(TraceMesh) && !StartSocket.IsNone() && !EndSocket.IsNone()
		&& TraceMesh->DoesSocketExist(StartSocket) && TraceMesh->DoesSocketExist(EndSocket))
	{
		const FVector StartLoc = TraceMesh->GetSocketLocation(StartSocket);
		const FVector EndLoc = TraceMesh->GetSocketLocation(EndSocket);
		const int32 SegmentCount = FMath::Max(2, CachedWeaponData.TraceSegmentCount);

		OutPoints.Reserve(SegmentCount);
		for (int32 i = 0; i < SegmentCount; ++i)
		{
			const float Alpha = static_cast<float>(i) / static_cast<float>(SegmentCount - 1);
			OutPoints.Add(FMath::Lerp(StartLoc, EndLoc, Alpha));
		}
		return;
	}

	const FName SingleSocket = CachedWeaponData.TraceSocketName.IsNone() ? RetrieveWeaponSockets::Weapon_R : CachedWeaponData.TraceSocketName;
	if (IsValid(TraceMesh) && TraceMesh->DoesSocketExist(SingleSocket))
	{
		OutPoints.Add(TraceMesh->GetSocketLocation(SingleSocket));
		return;
	}

	if (const AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		OutPoints.Add(AvatarActor->GetActorLocation());
	}
}

void UGA_SprintAttack::HandleMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_SprintAttack::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_SprintAttack::HandleMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_SprintAttack::StopRuntimeTasks()
{
	if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
	if (ImpactBeginEventTask) { ImpactBeginEventTask->EndTask(); ImpactBeginEventTask = nullptr; }
	if (ImpactEventTask) { ImpactEventTask->EndTask(); ImpactEventTask = nullptr; }
}

void UGA_SprintAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();

	HitActors.Reset();
	PreviousTracePoints.Reset();
	bHasValidPreviousTracePoints = false;
	bChargeBonusGranted = false;
	CachedWeaponComponent = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_SprintAttack::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	StopRuntimeTasks();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
