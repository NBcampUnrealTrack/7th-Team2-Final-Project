#include "AbilitySystem/Player/GA_BowShot.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/MeshComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystem/Player/StaffProjectile.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "TimerManager.h"

UGA_BowShot::UGA_BowShot()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_BowShot);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);

	bBlockActivationWhileAirborne = true;

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_ForcedKnockback);

	// 차징 중 조준(회전)을 허용해야 하므로 Animation.Lock.Rotation은 걸지 않는다.
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);
}

bool UGA_BowShot::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(WeaponComp) || !WeaponComp->IsEquipped())
	{
		return false;
	}

	if (WeaponComp->GetWeaponDataRef().WeaponTypeTag != RetrieveGameplayTags::Weapon_Type_Bow)
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(ASC) || !ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Aiming))
	{
		return false;
	}

	if (ProjectileClass == nullptr)
	{
		return false;
	}

	// 화살 재고(설정 시). None이면 무제한.
	if (!ArrowItemId.IsNone())
	{
		const UInventoryComponent* Inventory = AvatarActor->FindComponentByClass<UInventoryComponent>();
		if (!IsValid(Inventory))
		{
			UE_LOG(LogRetrieveCombat, Warning,
				TEXT("[GA_BowShot] ArrowItemId=%s 설정됐으나 InventoryComponent 없음 → 발사 차단"),
				*ArrowItemId.ToString());
			return false;
		}
		if (!Inventory->HasItem(ArrowItemId, GetArrowCost()))
		{
			return false;
		}
	}

	return true;
}

void UGA_BowShot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	CachedWeaponComponent = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	CachedElementTag = ResolveCurrentElementTag();
	CachedChargeMultiplier = 1.f;
	CachedEmpowerMultiplier = 1.f;

	StartCharging();
}

void UGA_BowShot::StartCharging()
{
	ChargeReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/false);
	if (!IsValid(ChargeReleaseTask))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	ChargeReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleChargeReleased);
	ChargeReleaseTask->ReadyForActivation();

	// 조준(Aiming) 태그가 사라지면(우클릭 해제) 즉시 캔슬.
	AimTagRemovedTask = UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(
		this, RetrieveGameplayTags::State_Player_Aiming, /*InOptionalExternalTarget=*/nullptr, /*OnlyTriggerOnce=*/true);
	if (IsValid(AimTagRemovedTask))
	{
		AimTagRemovedTask->Removed.AddDynamic(this, &ThisClass::HandleAimTagRemoved);
		AimTagRemovedTask->ReadyForActivation();
	}

	BroadcastChargeState(ERetrieveBowChargePhase::Started, 0.f);
}

void UGA_BowShot::HandleChargeReleased(float TimeHeld)
{
	// Aim 해제 감시 종료(더 이상 캔슬 트리거 불필요).
	if (AimTagRemovedTask)
	{
		AimTagRemovedTask->EndTask();
		AimTagRemovedTask = nullptr;
	}

	const float Clamped = FMath::Clamp(TimeHeld, 0.f, MaxChargeTime);

	// 최소 차징 미달 → 발사 취소(화살/강화 미소비). 0이면 항상 발사.
	if (Clamped < MinChargeTimeToFire)
	{
		BroadcastChargeState(ERetrieveBowChargePhase::Cancelled, 0.f);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	CachedChargeMultiplier = ComputeChargeDamageMultiplier(Clamped);

	// 강화 화살 소비 + 화살 재고 차감 — 권한 측 1회.
	CachedEmpowerMultiplier = 1.f;
	if (HasAuthority(&GetCurrentActivationInfoRef()))
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_BowShot_Empowered))
			{
				CachedEmpowerMultiplier = EmpoweredDamageMultiplier;
				ASC->RemoveLooseGameplayTag(RetrieveGameplayTags::State_Player_BowShot_Empowered);
				// TODO: 강화 실제 효과(추가 투사체/관통/원소 부여 등) 확장 지점.
			}
		}

		if (!ArrowItemId.IsNone())
		{
			if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
			{
				if (UInventoryComponent* Inventory = AvatarActor->FindComponentByClass<UInventoryComponent>())
				{
					Inventory->RemoveItem(ArrowItemId, ArrowItemCategoryTag, GetArrowCost());
				}
			}
		}
	}

	const float Ratio = (MaxChargeTime > 0.f) ? FMath::Clamp(Clamped / MaxChargeTime, 0.f, 1.f) : 1.f;
	CachedChargeRatio = Ratio;
	BroadcastChargeState(ERetrieveBowChargePhase::Released, Ratio);

	ScheduleProjectiles();
	PlayFireMontageThenEnd();
}

void UGA_BowShot::HandleAimTagRemoved()
{
	// 조준 해제 -> 발사 없이 즉시 캔슬.
	if (ChargeReleaseTask)
	{
		ChargeReleaseTask->EndTask();
		ChargeReleaseTask = nullptr;
	}

	BroadcastChargeState(ERetrieveBowChargePhase::Cancelled, 0.f);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UGA_BowShot::ScheduleProjectiles()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<float> Delays = FireDelays;
	if (Delays.IsEmpty())
	{
		Delays.Add(0.f);
	}

	for (const float FireDelay : Delays)
	{
		const float ClampedDelay = FMath::Max(0.f, FireDelay);
		if (ClampedDelay <= 0.f)
		{
			SpawnProjectile();
			continue;
		}

		FTimerHandle SpawnTimerHandle;
		World->GetTimerManager().SetTimer(SpawnTimerHandle, this, &UGA_BowShot::SpawnProjectile, ClampedDelay, false);
		SpawnTimerHandles.Add(SpawnTimerHandle);
	}
}

void UGA_BowShot::SpawnProjectile()
{
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!ProjectileClass || !IsValid(AvatarActor) || !IsValid(World))
	{
		return;
	}

	FRetrieveProjectileSpawnParams Params;
	Params.ProjectileClass = ProjectileClass;
	// 차징 비율로 무차징~풀차징 속도 보간. 느린 화살일수록 중력 낙차가 커진다.
	Params.Speed = FMath::Lerp(MinChargeProjectileSpeed, ProjectileSpeed, CachedChargeRatio);
	Params.DamageMultiplier = DamageMultiplier * CachedChargeMultiplier * CachedEmpowerMultiplier;
	Params.HitReactType = HitReactType;
	Params.SpawnSocketName = SpawnSocketName;
	Params.SpawnOffset = SpawnOffset;
	Params.AttackTypeTag = RetrieveGameplayTags::Attack_Type_Normal;
	Params.ElementTag = CachedElementTag;
	Params.ElementStatusEffect = ElementStatusEffects.FindRef(CachedElementTag);
	Params.ChargeBonusEventTag = ChargeBonusEventTag;

	// 크로스헤어 밑 조준 지점으로 발사 + 중력 낙차(설정 시).
	Params.AimPointLocation = ComputeAimPoint();
	Params.bHasAimPoint = true;
	if (ArrowGravityScale > 0.f)
	{
		Params.GravityScaleOverride = ArrowGravityScale;
	}

	UMeshComponent* WeaponMesh = IsValid(CachedWeaponComponent) ? CachedWeaponComponent->GetPrimaryEquippedWeaponMesh() : nullptr;
	AStaffProjectile::SpawnConfigured(World, AvatarActor, GetAbilitySystemComponentFromActorInfo(), WeaponMesh, /*AimTarget=*/nullptr, Params);
}

void UGA_BowShot::PlayFireMontageThenEnd()
{
	UAnimMontage* Montage = FireMontage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds=*/true);
	if (!MontageTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageFinished);
	MontageTask->ReadyForActivation();
}

void UGA_BowShot::HandleMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
}

float UGA_BowShot::ComputeChargeDamageMultiplier(float TimeHeld) const
{
	if (MaxChargeTime <= 0.f)
	{
		return MaxChargeDamageMultiplier;
	}
	const float Alpha = FMath::Clamp(TimeHeld / MaxChargeTime, 0.f, 1.f);
	// Alpha=0(조기 발사)이라도 0이 아닌 MinChargeDamageMultiplier로 발사.
	return FMath::Lerp(MinChargeDamageMultiplier, MaxChargeDamageMultiplier, Alpha);
}

FVector UGA_BowShot::ComputeAimPoint() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	FVector ViewLoc = FVector::ZeroVector;
	FRotator ViewRot = FRotator::ZeroRotator;
	if (IsValid(AvatarActor))
	{
		// 폴백: 액터 시점(눈 위치 + 컨트롤 회전).
		AvatarActor->GetActorEyesViewPoint(ViewLoc, ViewRot);
	}

	// 크로스헤어(화면 중앙) = 카메라 광선이므로 카메라 POV 우선.
	if (const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo())
	{
		if (const APlayerController* PC = Cast<APlayerController>(Info->PlayerController.Get()))
		{
			if (const APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
			{
				ViewLoc = CamMgr->GetCameraLocation();
				ViewRot = CamMgr->GetCameraRotation();
			}
		}
	}

	// 차징 비율로 영점(임계) 거리 결정 → 크로스헤어 방향 그 거리의 지점을 목표로 한다.
	// 표면을 트레이스하지 않는다: 먼 표면을 겨냥하면 도달하려고 과하게 로브하기 때문.
	// 이 고정 임계거리에 탄도해를 풀면 그 전까진 거의 평행, 넘어가면 급락한다(소총 영점 감각).
	const float CriticalRange = FMath::Lerp(MinCriticalRange, MaxCriticalRange, CachedChargeRatio);
	return ViewLoc + ViewRot.Vector() * CriticalRange;
}

int32 UGA_BowShot::GetArrowCost() const
{
	// 투사체 1개당 화살 1개. FireDelays 비어 있으면 단발(1).
	return FMath::Max(1, FireDelays.Num());
}

void UGA_BowShot::BroadcastChargeState(ERetrieveBowChargePhase Phase, float ChargeRatio) const
{
	// 레티클은 로컬 표현이므로 로컬 컨트롤 클라이언트에서만 발행.
	if (!CurrentActorInfo || !CurrentActorInfo->IsLocallyControlled())
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = IsValid(AvatarActor) ? AvatarActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	FRetrieveBowChargePayload Payload;
	Payload.Instigator = AvatarActor;
	Payload.Phase = Phase;
	Payload.MaxChargeTime = MaxChargeTime;
	Payload.ChargeRatio = ChargeRatio;

	UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Bow_Charge, Payload);
}

void UGA_BowShot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& SpawnTimerHandle : SpawnTimerHandles)
		{
			World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		}
	}
	SpawnTimerHandles.Reset();

	if (ChargeReleaseTask)
	{
		ChargeReleaseTask->EndTask();
		ChargeReleaseTask = nullptr;
	}
	if (AimTagRemovedTask)
	{
		AimTagRemovedTask->EndTask();
		AimTagRemovedTask = nullptr;
	}
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	CachedWeaponComponent = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}