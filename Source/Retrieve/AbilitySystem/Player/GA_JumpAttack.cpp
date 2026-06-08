#include "AbilitySystem/Player/GA_JumpAttack.h"

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
#include "Components/WeaponComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"

UGA_JumpAttack::UGA_JumpAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_JumpAttack);
	SetAssetTags(Tags);

	// 공중 전용 어빌리티
	bBlockActivationWhileAirborne = false;

	// 상태 게이트(사망/피격/다운)때 동작 불가
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);

	// "공격 중" 상태 태그를 재사용
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);

	// 공중 공격 중 가드 차단
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
}

bool UGA_JumpAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	if (!IsAvatarAirborne(ActorInfo))
	{
		return false;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(WeaponComp) || !WeaponComp->IsEquipped())
	{
		return false;
	}
	
	if (WeaponComp->GetWeaponDataRef().JumpAttack.Montage.IsNull())
	{
		return false;
	}

	return IsValid(DamageEffectClass);
}

void UGA_JumpAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	CachedWeaponComponent = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(CachedWeaponComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedWeaponData = CachedWeaponComponent->GetWeaponDataRef();

	UAnimMontage* Montage = CachedWeaponData.JumpAttack.Montage.LoadSynchronous();
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

	// 착지 시 몽타주 취소 → 착지 블렌드
	if (ACharacter* Character = Cast<ACharacter>(AvatarActor))
	{
		Character->LandedDelegate.AddDynamic(this, &ThisClass::HandleLanded);
		BoundLandedCharacter = Character;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.f, CachedWeaponData.JumpAttack.SectionName, true);
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

void UGA_JumpAttack::HandleImpactBeginEvent(FGameplayEventData Payload)
{
	PreviousTracePoints.Reset();
	bHasValidPreviousTracePoints = false;
	HitActors.Reset();
}

void UGA_JumpAttack::HandleImpactEvent(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}
	ApplyHitDamage();
}

void UGA_JumpAttack::ApplyHitDamage()
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

	const float DamageMul = CachedWeaponData.JumpAttack.DamageMultiplier;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GA_JumpAttack_Impact), false, AvatarActor);
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
			constexpr float DebugLife = -1.0f;
			DrawDebugLine(World, SegStart, SegEnd, bHit ? FColor::Green : FColor::Red, false, DebugLife, 0, 0.5f);
			DrawDebugSphere(World, SegEnd, TraceRadius, 8, bHit ? FColor::Green : FColor::Red, false, DebugLife);
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
			PerHitContext.AddHitResult(Hit, /*bReset=*/true);

			FGameplayEffectSpecHandle PerHitSpec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), PerHitContext);
			if (!PerHitSpec.IsValid() || !PerHitSpec.Data.IsValid()) continue;

			PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, DamageMul);
			PerHitSpec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Normal);
			
			if (const FGameplayTag ReactTag = HitReactTypeToTag(CachedWeaponData.JumpAttack.HitReactType); ReactTag.IsValid())
			{
				PerHitSpec.Data->AddDynamicAssetTag(ReactTag);
			}

			SourceASC->ApplyGameplayEffectSpecToTarget(*PerHitSpec.Data.Get(), TargetASC);
			HitActors.Add(TargetActor);
			
			if (!bChargeBonusGranted)
			{
				const FGameplayTag BonusTag = CachedWeaponData.JumpAttack.ChargeBonusEventTag;
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

void UGA_JumpAttack::BuildTracePoints(TArray<FVector>& OutPoints) const
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

void UGA_JumpAttack::HandleMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_JumpAttack::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_JumpAttack::HandleMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_JumpAttack::HandleLanded(const FHitResult& Hit)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CurrentMontageStop(0.1f);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_JumpAttack::StopRuntimeTasks()
{
	if (MontageTask)
	{
		MontageTask->EndTask(); MontageTask = nullptr;
	}
	if (ImpactBeginEventTask)
	{
		ImpactBeginEventTask->EndTask(); ImpactBeginEventTask = nullptr;
	}
	if (ImpactEventTask)
	{
		ImpactEventTask->EndTask(); ImpactEventTask = nullptr;
	}
}

void UGA_JumpAttack::UnbindLanded()
{
	if (ACharacter* Character = BoundLandedCharacter.Get())
	{
		Character->LandedDelegate.RemoveDynamic(this, &ThisClass::HandleLanded);
	}
	BoundLandedCharacter = nullptr;
}

void UGA_JumpAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();
	UnbindLanded();

	HitActors.Reset();
	PreviousTracePoints.Reset();
	bHasValidPreviousTracePoints = false;
	bChargeBonusGranted = false;
	CachedWeaponComponent = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_JumpAttack::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	StopRuntimeTasks();
	UnbindLanded();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
