#include "AbilitySystem/Player/GA_GuardAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/RetrieveWeaponSockets.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/WeaponAttackDefinition.h"
#include "Data/WeaponGuardAttackDefinition.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_GuardAttack::UGA_GuardAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_GuardAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Parry);   // GA_ParryCounter가 "패리 성공 주체"를 찾는 공통 태그.
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);    // 전투 스탠스/공격 캔슬/피격 취소 정책은 공격군으로 취급한다.
	SetAssetTags(Tags);

	ApplyCommonActionBlocks();
	bBlockedByLocomotionAction = true;

	bUseCombatInputBuffer = true;
	CombatInputPriority = 10;

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	// GuardAttack은 Guard 상태에서 파생되는 액션이다.
	// 발동 시 홀드 Guard를 끊고 방패 공격 몽타주로 전환한다.
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
}

bool UGA_GuardAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(ASC) || ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Cooldown_Player_Parry))
	{
		// GuardAttack은 패리 시도 액션이므로 쿨다운 중 "데미지만 있는 방패 공격"으로 변질시키지 않는다.
		return false;
	}

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(WeaponComp) || !WeaponComp->IsEquipped())
	{
		return false;
	}

	const FRetrieveWeaponDataRow& WeaponData = WeaponComp->GetWeaponDataRef();
	if (WeaponData.WeaponTypeTag != RetrieveGameplayTags::Weapon_Type_SwordShield || WeaponData.AttackComboDefinition.IsNull())
	{
		return false;
	}

	const UWeaponAttackDefinition* AttackDefinition = WeaponData.AttackComboDefinition.LoadSynchronous();
	if (!IsValid(AttackDefinition) || AttackDefinition->GuardAttackDefinition.IsNull())
	{
		// Staff/Bow 등 GuardAttack 미지원 무기는 flag가 아니라 optional data asset null로 걸러진다.
		return false;
	}

	return IsValid(DamageEffectClass);
}

void UGA_GuardAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
	if (!ResolveGuardAttackData())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimMontage* Montage = CachedGuardAttackData.Montage.LoadSynchronous();
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
		this, NAME_None, Montage, 1.f, CachedGuardAttackData.SectionName, true,
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

bool UGA_GuardAttack::ResolveGuardAttackData()
{
	UWeaponAttackDefinition* AttackDefinition = CachedWeaponData.AttackComboDefinition.LoadSynchronous();
	if (!IsValid(AttackDefinition) || AttackDefinition->GuardAttackDefinition.IsNull())
	{
		return false;
	}

	UWeaponGuardAttackDefinition* GuardAttackDefinition = AttackDefinition->GuardAttackDefinition.LoadSynchronous();
	if (!IsValid(GuardAttackDefinition))
	{
		return false;
	}

	const FWeaponGuardAttackData* ResolvedData = GuardAttackDefinition->ResolveGuardAttackVariant(ResolveCurrentElementTag());
	if (!ResolvedData || ResolvedData->Montage.IsNull())
	{
		return false;
	}

	CachedGuardAttackData = *ResolvedData;
	return true;
}

bool UGA_GuardAttack::OpenNotifyParryWindow()
{
	// 이 함수는 AnimNotifyState_ParryWindow에서 호출된다.
	// NotifyState는 Ability 타입을 모르므로, GuardAttack 쪽에서 자기 데이터가 실제로 패리를 허용하는지 확인한다.
	//
	// bCanStartParry 의미:
	// - "이 GuardAttack 액션의 특정 몽타주 구간에서 패리 판정을 시작할 수 있는가"이다.
	// - 적 공격이 패리 가능한지는 Attack.Type.Parryable / Unblockable 쪽이 별도로 결정한다.
	if (!IsActive() || !CachedGuardAttackData.bCanStartParry)
	{
		return false;
	}

	// OpenParryWindow는 State.Player.Parrying을 부여하는 GE를 적용한다.
	// 실제 적 공격 trace가 플레이어에게 들어왔을 때 CombatAttributeSet이 이 태그를 보고 패리 성공 여부를 판단한다.
	if (!OpenParryWindow())
	{
		return false;
	}

	// window가 실제로 열린 경우에만 성공 이벤트를 듣는다.
	// 쿨다운 등으로 Open이 실패했는데 성공 이벤트를 구독하면, 이후 다른 경로의 이벤트를 잘못 받을 수 있다.
	StartListeningForParrySuccess();
	return true;
}

void UGA_GuardAttack::CloseNotifyParryWindow()
{
	// NotifyState End에서 닫히는 정상 실패/만료도 "패리 시도 1회"로 본다.
	// 그래서 window가 실제로 열린 적이 있으면 cooldown을 적용한다.
	//
	// 성공 경로:
	// - UGA_ParryBase::HandleParrySuccess가 이미 CloseParryWindow + ApplyParryCooldown + StopParrySuccessTask를 수행한다.
	// - 그 뒤 NotifyEnd가 다시 들어와도 bHadParryWindow가 false가 되므로 cooldown이 중복 적용되지 않는다.
	const bool bHadParryWindow = bParryWindowOpened || ParryWindowHandle.IsValid();

	CloseParryWindow();

	if (bHadParryWindow)
	{
		ApplyParryCooldown();
	}

	StopParrySuccessTask();
}

void UGA_GuardAttack::HandleImpactBeginEvent(FGameplayEventData /*Payload*/)
{
	PreviousTracePoints.Reset();
	bHasValidPreviousTracePoints = false;
	HitActors.Reset();
}

void UGA_GuardAttack::HandleImpactEvent(FGameplayEventData /*Payload*/)
{
	if (!IsActive())
	{
		return;
	}

	ApplyHitDamage();
}

void UGA_GuardAttack::ApplyHitDamage()
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

	TArray<FVector> CurrentPoints;
	BuildTracePoints(CurrentPoints);
	if (CurrentPoints.IsEmpty())
	{
		return;
	}

	SweepAndApplyDamage(CurrentPoints);
}

void UGA_GuardAttack::BuildTracePoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();

	switch (CachedGuardAttackData.AttackSource)
	{
	case ERetrieveAttackSource::Shield:
		BuildShieldTracePoints(OutPoints);
		break;

	case ERetrieveAttackSource::Body:
		if (const AActor* AvatarActor = GetAvatarActorFromActorInfo())
		{
			OutPoints.Add(AvatarActor->GetActorLocation());
		}
		break;

	case ERetrieveAttackSource::Weapon:
	default:
		BuildWeaponTracePoints(OutPoints);
		break;
	}

	if (OutPoints.IsEmpty())
	{
		if (const AActor* AvatarActor = GetAvatarActorFromActorInfo())
		{
			OutPoints.Add(AvatarActor->GetActorLocation());
		}
	}
}

void UGA_GuardAttack::BuildWeaponTracePoints(TArray<FVector>& OutPoints) const
{
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
		for (int32 Index = 0; Index < SegmentCount; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(SegmentCount - 1);
			OutPoints.Add(FMath::Lerp(StartLoc, EndLoc, Alpha));
		}
		return;
	}

	const FName SingleSocket = CachedWeaponData.TraceSocketName.IsNone() ? RetrieveWeaponSockets::Weapon_R : CachedWeaponData.TraceSocketName;
	if (IsValid(TraceMesh) && TraceMesh->DoesSocketExist(SingleSocket))
	{
		OutPoints.Add(TraceMesh->GetSocketLocation(SingleSocket));
	}
}

void UGA_GuardAttack::BuildShieldTracePoints(TArray<FVector>& OutPoints) const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (IsValid(CharacterMesh) && CharacterMesh->DoesSocketExist(RetrieveWeaponSockets::Shield))
	{
		OutPoints.Add(CharacterMesh->GetSocketLocation(RetrieveWeaponSockets::Shield));
		return;
	}

	UMeshComponent* WeaponMesh = IsValid(CachedWeaponComponent) ? CachedWeaponComponent->GetPrimaryEquippedWeaponMesh() : nullptr;
	if (IsValid(WeaponMesh) && WeaponMesh->DoesSocketExist(RetrieveWeaponSockets::Shield))
	{
		OutPoints.Add(WeaponMesh->GetSocketLocation(RetrieveWeaponSockets::Shield));
	}
}

void UGA_GuardAttack::SweepAndApplyDamage(const TArray<FVector>& CurrentPoints)
{
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!IsValid(SourceASC) || !IsValid(AvatarActor) || !IsValid(World))
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GA_GuardAttack_Impact), false, AvatarActor);
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

	for (int32 Index = 0; Index + 1 < CurrentPoints.Num(); ++Index)
	{
		SweepSegment(CurrentPoints[Index], CurrentPoints[Index + 1]);
	}

	if (bHasPrev)
	{
		for (int32 Index = 0; Index < CurrentPoints.Num(); ++Index)
		{
			SweepSegment(PreviousTracePoints[Index], CurrentPoints[Index]);
		}
	}
	else if (CurrentPoints.Num() == 1)
	{
		SweepSegment(CurrentPoints[0], CurrentPoints[0]);
	}

	PreviousTracePoints = CurrentPoints;
	bHasValidPreviousTracePoints = true;

	for (const FHitResult& Hit : AllHits)
	{
		AActor* TargetActor = Hit.GetActor();
		if (!IsValid(TargetActor) || TargetActor == AvatarActor || HitActors.Contains(TargetActor))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!IsValid(TargetASC))
		{
			continue;
		}

		FGameplayEffectContextHandle PerHitContext = SourceASC->MakeEffectContext();
		PerHitContext.AddInstigator(AvatarActor, AvatarActor);
		PerHitContext.AddSourceObject(this);
		PerHitContext.AddHitResult(Hit, true);

		FGameplayEffectSpecHandle PerHitSpec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), PerHitContext);
		if (!PerHitSpec.IsValid() || !PerHitSpec.Data.IsValid())
		{
			continue;
		}

		PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, CachedGuardAttackData.DamageMultiplier);
		if (CachedGuardAttackData.KnockbackStrength > 0.f)
		{
			PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_Strength, CachedGuardAttackData.KnockbackStrength);
			PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_UpwardStrength, CachedGuardAttackData.KnockbackUpwardStrength);
		}

		AddCombatTagsToDamageSpec(
			*PerHitSpec.Data.Get(),
			ResolveCurrentElementTag(),
			RetrieveGameplayTags::Attack_Type_Normal,
			FGameplayTag(),
			HitReactTypeToTag(CachedGuardAttackData.HitReactType));

		SourceASC->ApplyGameplayEffectSpecToTarget(*PerHitSpec.Data.Get(), TargetASC);
		HitActors.Add(TargetActor);
	}
}

void UGA_GuardAttack::HandleMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_GuardAttack::HandleMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_GuardAttack::HandleMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_GuardAttack::StopRuntimeTasks()
{
	if (MontageTask) { MontageTask->EndTask(); MontageTask = nullptr; }
	if (ImpactBeginEventTask) { ImpactBeginEventTask->EndTask(); ImpactBeginEventTask = nullptr; }
	if (ImpactEventTask) { ImpactEventTask->EndTask(); ImpactEventTask = nullptr; }
}

void UGA_GuardAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();

	HitActors.Reset();
	PreviousTracePoints.Reset();
	bHasValidPreviousTracePoints = false;
	CachedWeaponComponent = nullptr;
	CachedWeaponData = FRetrieveWeaponDataRow();
	CachedGuardAttackData = FWeaponGuardAttackData();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_GuardAttack::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	StopRuntimeTasks();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
