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
#include "Components/SkeletalMeshComponent.h"
#include "Character/Cosmetics/RetrieveBowLinkedAnimInstance.h"
#include "Character/Cosmetics/RetrieveBowMeshAnimInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
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

	// 링크된 활 레이어를 잡아 캐시(차징/발사 몽타주 소스). GA_StanceTransition과 동일 탐색.
	CachedBowLayer = nullptr;
	if (const ACharacter* Character = Cast<ACharacter>(AvatarActor))
	{
		if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			for (UAnimInstance* Linked : Mesh->GetLinkedAnimInstances())
			{
				if (URetrieveBowLinkedAnimInstance* BowLayer = Cast<URetrieveBowLinkedAnimInstance>(Linked))
				{
					CachedBowLayer = BowLayer;
					break;
				}
			}
		}
	}

	// 활 메시 AnimInstance 캐시(활메시 phase 몽타주 재생 대상). AnimInstanceClass 미설정이면 null → 재생 스킵.
	CachedBowMeshAnim = nullptr;
	if (IsValid(CachedWeaponComponent))
	{
		if (USkeletalMeshComponent* BowSkel = Cast<USkeletalMeshComponent>(CachedWeaponComponent->GetPrimaryEquippedWeaponMesh()))
		{
			CachedBowMeshAnim = Cast<URetrieveBowMeshAnimInstance>(BowSkel->GetAnimInstance());
		}
	}

	CachedElementTag = ResolveCurrentElementTag();
	CachedChargeMultiplier = 1.f;
	CachedEmpowerMultiplier = 1.f;

	StartCharging();
}

void UGA_BowShot::StartCharging()
{
	// 조준(Aiming) 해제 감시 — 장전/차징 전 구간(중간 우클릭 해제 시 캔슬).
	AimTagRemovedTask = UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(
		this, RetrieveGameplayTags::State_Player_Aiming, /*InOptionalExternalTarget=*/nullptr, /*OnlyTriggerOnce=*/true);
	if (IsValid(AimTagRemovedTask))
	{
		AimTagRemovedTask->Removed.AddDynamic(this, &ThisClass::HandleAimTagRemoved);
		AimTagRemovedTask->ReadyForActivation();
	}

	// 미노킹 + 화살 소비형 → 장전(Reload) 먼저 후 차징. 무제한/이미 노킹이면 바로 차징.
	const bool bSkipReload = ArrowItemId.IsNone()
		|| !IsValid(CachedWeaponComponent)
		|| CachedWeaponComponent->IsArrowNocked();
	if (bSkipReload)
	{
		BeginCharge();
	}
	else
	{
		PlayReloadThenBeginCharge();
	}
}

void UGA_BowShot::PlayReloadThenBeginCharge()
{
	// 활 메시 장전 애님(lockstep).
	PlayBowMeshMontage(EBowShotPhase::Reload, MontagePlayRate);

	UAnimMontage* Reload = ResolveShotMontage(EBowShotPhase::Reload);
	if (!IsValid(Reload))
	{
		// 캐릭터 장전 몽타주 없음 → 연출 없이 즉시 노킹 후 차징.
		if (IsValid(CachedWeaponComponent))
		{
			CachedWeaponComponent->SetNockedArrowVisible(true);
		}
		BeginCharge();
		return;
	}

	ReloadMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Reload, MontagePlayRate, NAME_None, /*bStopWhenAbilityEnds=*/true);
	if (!ReloadMontageTask)
	{
		if (IsValid(CachedWeaponComponent))
		{
			CachedWeaponComponent->SetNockedArrowVisible(true);
		}
		BeginCharge();
		return;
	}

	// 장전 완료/인터럽트 → 차징 시작. 중복·종료 콜백은 HandleReloadFinished가 가드.
	ReloadMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleReloadFinished);
	ReloadMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleReloadFinished);
	ReloadMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleReloadFinished);
	ReloadMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleReloadFinished);
	ReloadMontageTask->ReadyForActivation();
}

void UGA_BowShot::HandleReloadFinished()
{
	if (bCharging || !IsActive())
	{
		return; // 이미 차징 시작했거나 어빌리티 종료 중(중복/인터럽트 가드)
	}
	BeginCharge();
}

void UGA_BowShot::BeginCharge()
{
	// 릴리즈 감시 — 장전 이후부터 카운트(차징 시간에 장전 시간 미포함).
	// bTestAlreadyReleased=false: 미래 릴리즈 이벤트만 발화(true면 서버에서 입력 복제 전 '이미 뗌')
	ChargeReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/false);
	if (!IsValid(ChargeReleaseTask))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	ChargeReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleChargeReleased);
	ChargeReleaseTask->ReadyForActivation();

	BroadcastChargeState(ERetrieveBowChargePhase::Started, 0.f);

	bCharging = true;

	// 활 메시 ABP가 읽어 현을 손으로 당긴다. 발사/캔슬 시 StopCharging에서 해제 → 원형 복귀.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Player_BowShot_Drawing);
	}

	// 차징 애님: DrawnStart(1회) 재생 → 블렌드아웃 시 Drawn(loop) 진입. 인트로 미설정이면 바로 홀드.
	if (UAnimMontage* Intro = ResolveShotMontage(EBowShotPhase::DrawnStart))
	{
		ChargeMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Intro, 1.f, NAME_None, /*bStopWhenAbilityEnds=*/true);
		if (ChargeMontageTask)
		{
			ChargeMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::PlayChargeHold);
			ChargeMontageTask->ReadyForActivation();
		}
		else
		{
			PlayChargeHold();
		}
	}
	else
	{
		PlayChargeHold();
	}

	// 활 메시 lockstep: 현/화살 DrawnStart 동시 재생.
	PlayBowMeshMontage(EBowShotPhase::DrawnStart, 1.f);

	// 풀차지 도달 → DrawnShake로 교체.
	if (MaxChargeTime > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(FullChargeTimerHandle, this,
				&UGA_BowShot::HandleFullChargeReached, MaxChargeTime, false);
		}
	}
}

void UGA_BowShot::HandleChargeReleased(float TimeHeld)
{
	// Aim 해제 감시 종료(더 이상 캔슬 트리거 불필요).
	if (AimTagRemovedTask)
	{
		AimTagRemovedTask->EndTask();
		AimTagRemovedTask = nullptr;
	}

	// 차징 애님/풀차지 타이머 정지(발사 몽타주로 전환).
	StopCharging();

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

	StopCharging();

	BroadcastChargeState(ERetrieveBowChargePhase::Cancelled, 0.f);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
}

void UGA_BowShot::PlayChargeHold()
{
	if (!bCharging)
	{
		return; // 릴리즈/조준해제 후 늦게 온 인트로 콜백 무시
	}
	StartChargeLoopMontage(ResolveShotMontage(EBowShotPhase::Drawn));
	PlayBowMeshMontage(EBowShotPhase::Drawn, 1.f);
}

void UGA_BowShot::HandleFullChargeReached()
{
	if (!bCharging)
	{
		return;
	}
	StartChargeLoopMontage(ResolveShotMontage(EBowShotPhase::DrawnShake));
	PlayBowMeshMontage(EBowShotPhase::DrawnShake, 1.f);
}

void UGA_BowShot::StartChargeLoopMontage(UAnimMontage* Montage)
{
	if (!IsValid(Montage))
	{
		return; // 몽타주 미설정 → 애님 없이 차징 지속(UI/타이머 유효)
	}

	if (ChargeMontageTask)
	{
		ChargeMontageTask->EndTask();
		ChargeMontageTask = nullptr;
	}

	ChargeMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, 1.f, NAME_None, /*bStopWhenAbilityEnds=*/true);
	if (ChargeMontageTask)
	{
		ChargeMontageTask->ReadyForActivation();
	}
}

void UGA_BowShot::StopCharging()
{
	bCharging = false;

	// 현 당김 해제 → 활 ABP가 원형 rest로 복귀(발사 트왕) + 화살 본 숨김.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(RetrieveGameplayTags::State_Player_BowShot_Drawing);

		// 차징 몽타주(DrawnShake 등) 명시적 정지. ChargeMontageTask->EndTask()는 '어빌리티 종료' 시에만
		// 몽타주를 멈추므로(bStopWhenAbilityEnds && AbilityEnded), 발사 몽타주로 덮이지 않는 캔슬 경로에선
		// 여기서 직접 멈추지 않으면 셰이크 포즈가 남는다.
		ASC->CurrentMontageStop();
	}

	if (ChargeMontageTask)
	{
		ChargeMontageTask->EndTask();
		ChargeMontageTask = nullptr;
	}

	if (ReloadMontageTask)
	{
		ReloadMontageTask->EndTask();
		ReloadMontageTask = nullptr;
	}

	// 활 메시 lockstep 몽타주도 정지(차징 루프 해제).
	if (URetrieveBowMeshAnimInstance* BowAnim = CachedBowMeshAnim.Get())
	{
		BowAnim->Montage_Stop(0.1f);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FullChargeTimerHandle);
	}
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
	// 발사 후 잔탄 여부로 재장전/전탄소진 분기. 무제한(ArrowItemId=None)이면 항상 재장전.
	// 발사 전 잔량 기준(GetArrowCost()+1: 이번 발사분보다 더 있나) — 로컬 컨트롤 표현 일치.
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const UInventoryComponent* Inventory = IsValid(AvatarActor) ? AvatarActor->FindComponentByClass<UInventoryComponent>() : nullptr;
	const bool bWillHaveAmmo = ArrowItemId.IsNone()
		|| (Inventory && Inventory->HasItem(ArrowItemId, GetArrowCost() + 1));

	const EBowShotPhase FirePhase = bWillHaveAmmo ? EBowShotPhase::FireReload : EBowShotPhase::FireIdle;
	UAnimMontage* Montage = ResolveShotMontage(FirePhase);
	PlayBowMeshMontage(FirePhase, MontagePlayRate); // 활 메시 lockstep 발사

	// 발사 몽타주 미설정 → 애님 없이 종료(투사체는 이미 스케줄됨).
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

UAnimMontage* UGA_BowShot::ResolveShotMontage(EBowShotPhase Phase) const
{
	const URetrieveBowLinkedAnimInstance* Layer = CachedBowLayer.Get();
	return IsValid(Layer) ? Layer->ShotMontages.Resolve(Phase, IsOwnerCrouched()) : nullptr;
}

void UGA_BowShot::PlayBowMeshMontage(EBowShotPhase Phase, float PlayRate)
{
	URetrieveBowMeshAnimInstance* Anim = CachedBowMeshAnim.Get();
	if (!IsValid(Anim))
	{
		return; // 활 메시 AnimInstance 미설정 → 스킵(캐릭터 몽타주만 재생)
	}

	// 루프(Drawn/DrawnShake)는 몽타주 애셋 자체 루프 섹션에 맡긴다(캐릭터와 동일 — 코드 강제 루프 없음).
	if (UAnimMontage* Montage = Anim->ShotMontages.Resolve(Phase, IsOwnerCrouched()))
	{
		Anim->Montage_Play(Montage, PlayRate);
	}
}

bool UGA_BowShot::IsOwnerCrouched() const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	return Character && Character->bIsCrouched;
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

	StopCharging(); // 차징 몽타주 태스크 종료 + 풀차지 타이머 정리

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