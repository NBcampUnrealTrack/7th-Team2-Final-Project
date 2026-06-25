#include "AbilitySystem/Player/GA_Attack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Animation/RetrieveWeaponSockets.h"
#include "Character/RetrieveAlsCharacter.h"
#include "Components/Combat/CombatStanceComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/WeaponAttackDefinition.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"

UGA_Attack::UGA_Attack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);

	// 공중/점프 중 공격 불가
	bBlockActivationWhileAirborne = true;

	// 구르기/맨틀 등 ALS 액션 중 공격 발동 불가. 단, 캔슬 윈도우가 이 입력을 허용하면 예외(캔슬 우선) — CanActivateAbility 참고.
	bBlockedByLocomotionAction = true;

	// 평타는 버퍼를 쓴다. 우선순위 0(가장 낮음) — 같은 입력에 묶인 Sprint/Heavy/Jump(우선순위 높음)가
	// 문맥상 발동 가능하면 그쪽이 먼저 소비된다.
	bUseCombatInputBuffer = true;
	CombatInputPriority = 0;

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Sprinting);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);
	
	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
}

bool UGA_Attack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
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
	
	if (WeaponComp->GetWeaponDataRef().AttackComboDefinition.IsNull())
	{
		return false;
	}
	
	return IsValid(DamageEffectClass);
}

void UGA_Attack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 납검 상태의 평타는 스윙하지 않고 '발검'만 한다(한 번 뽑기).
	// 발검 트리거를 여기서 직접 호출한다 — CombatStance의 콜백은 PreActivate에서 이 함수보다
	// 먼저 실행되므로 거기서 스탠스를 바꾸면 납검 태그가 지워져 이 게이트가 깨진다(콜백은 평타+납검에 무동작).
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_WeaponSheathed))
		{
			if (AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)
			{
				if (UCombatStanceComponent* Stance = AvatarActor->FindComponentByClass<UCombatStanceComponent>())
				{
					Stance->NotifyCombatActivity(/*bFromAttack=*/false); // 발검 몽타주 + 디케이 타이머
				}
			}
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			return;
		}
	}

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
	if (!ResolveAttackComboVariant())
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

	// 콤보 다음 타는 ASC 리졸버가 매 프레임 버퍼를 보고 TryConsumeBufferedCombatInput()로 위임한다.
	// (윈도우 열림 태스크/버퍼 델리게이트 구독 불필요)
	StartComboStep(0);
}

bool UGA_Attack::ResolveAttackComboVariant()
{
	CachedComboSteps.Reset();

	UWeaponAttackDefinition* ComboDefinition = CachedWeaponData.AttackComboDefinition.LoadSynchronous();
	if (!IsValid(ComboDefinition))
	{
		return false;
	}

	const FGameplayTag ElementTag = ResolveCurrentElementTag();
	CachedElementTag = ElementTag;
	const FAttackComboVariant* Variant = ComboDefinition->ResolveComboVariant(ElementTag);
	if (!Variant)
	{
		return false;
	}
	
	CachedAttackMontage = Variant->Montage.LoadSynchronous();
	if (!IsValid(CachedAttackMontage))
	{
		return false;
	}
	
	CachedComboSteps = Variant->ComboSteps;
	return !CachedComboSteps.IsEmpty();
}

void UGA_Attack::StartComboStep(int32 StepIndex)
{
	// 재생 일원화: 모든 타가 동일 경로로 자기 섹션을 새로 재생한다 (예약 방식 제거).
	if (!CachedComboSteps.IsValidIndex(StepIndex) || !IsValid(CachedAttackMontage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 직전 타 태스크를 먼저 정리 — 새 재생이 일으키는 OnInterrupted가 EndAbility로 어빌리티를 취소하지 않도록.
	// 캔슬/콤보 윈도우 태그는 여기서 강제로 0으로 만들지 않는다. 직전 섹션의 ANS End와
	// 새 섹션의 ANS Begin이 ref-count로 알아서 짝맞춤한다(겹침 허용). 강제 리셋하면 직전 End가
	// 한 틱 늦게 떨어질 때 새 윈도우 카운트까지 깎여 전환이 간헐적으로 실패한다. 종료 정리는 EndAbility에서만.
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	CurrentComboIndex = StepIndex;          // 재생 시점에 인덱스 직접 확정 (ImpactBegin 커밋 불필요)
	bComboChargeBonusGranted = false;

	HitActorsThisStep.Reset();
	PreviousTracePoints.Reset();
	bHasValidPreviousTracePoints = false;

	const FWeaponComboStep& StepData = CachedComboSteps[StepIndex];

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, CachedAttackMontage, GetMontagePlayRate(), StepData.SectionName, true, 1.f, 0.f, /*bAllowInterruptAfterBlendOut=*/true);
	if (!MontageTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	MontageTask->ReadyForActivation();
}

bool UGA_Attack::TryConsumeBufferedCombatInput(const FRetrieveBufferedCombatInput& BufferedInput)
{
	// 리졸버가 '활성 중인 나'의 버퍼 입력을 넘겨준 경우다. 여기서는 콤보 다음 타(내부 전환)만 처리한다.
	// 원소 전환/외부 공격(Sprint/Heavy/Jump)은 별개 어빌리티라 리졸버가 직접 발동한다 — 여기서 다루지 않음.
	if (BufferedInput.IntentTag != RetrieveGameplayTags::Ability_Player_Attack)
	{
		return false;
	}

	// 콤보 윈도우가 열려 있어야 다음 타로 넘어간다.
	// (기존 ComboWindow = State.Combo.Open, 또는 평타를 허용하는 AttackCancelWindow 둘 다 인정)
	const URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (!IsValid(RetrieveASC))
	{
		return false;
	}
	// 자기 콤보 다음 타는 '공격 캔슬 윈도우가 열려 있으면' 진행한다.
	// (AllowedCancelIntents에 평타를 따로 넣을 필요 없음 — 그 목록은 교차 캔슬용)
	// State.Combo.Open(레거시 콤보 윈도우)도 그대로 인정.
	const bool bWindowOpen =
		RetrieveASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Combo_Open)
		|| RetrieveASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Attack_CancelOpen);
	if (!bWindowOpen)
	{
		return false;
	}

	// 콤보 도중 원소가 바뀌었으면(다른 SetElement 발동됨) 새 원소 변형으로 0타부터 다시 시작.
	if (ResolveCurrentElementTag() != CachedElementTag)
	{
		if (!ResolveAttackComboVariant())
		{
			return false;
		}
		StartComboStep(0);
		return true;
	}

	// 같은 원소면 다음 타로. 마지막 타면 소비하지 않고 몽타주가 자연 종료되게 둔다.
	const int32 NextStep = CurrentComboIndex + 1;
	if (!CachedComboSteps.IsValidIndex(NextStep))
	{
		return false;
	}

	StartComboStep(NextStep);
	return true;
}

void UGA_Attack::HandleImpactBeginEvent(FGameplayEventData Payload)
{
	// 인덱스는 재생 시점(StartComboStep)에 확정됨. 여기서는 이번 스윙의 트레이스/히트 상태만 초기화.
	PreviousTracePoints.Reset();
	bHasValidPreviousTracePoints = false;
	HitActorsThisStep.Reset();
}

void UGA_Attack::HandleImpactEvent(FGameplayEventData Payload)
{
	if (!IsActive() || CurrentComboIndex == INDEX_NONE)
	{
		return;
	}
	ApplyStepDamage();
}

void UGA_Attack::ApplyStepDamage()
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
	
	const float DamageMul = CachedComboSteps.IsValidIndex(CurrentComboIndex)
		? CachedComboSteps[CurrentComboIndex].DamageMultiplier : 1.0f;
	
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GA_Attack_Impact), false, AvatarActor);
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
		const bool bAlreadyHitThisStep = HitActorsThisStep.Contains(TargetActor);

		// 진단용: 콤보 스텝별로 어디서 데미지가 떨어지는지 추적(피격 상태 태그 + 중복 여부)
		if (bDebugDrawTrace)
		{
			UE_LOG(LogRetrieveCombat, Warning,
				TEXT("[GA_Attack] Step=%d Target=%s ASC=%s AlreadyHitThisStep=%d | Staggered=%d Hit=%d Groggy=%d Dead=%d"),
				CurrentComboIndex, *GetNameSafe(TargetActor), TargetASC ? TEXT("Y") : TEXT("N"),
				bAlreadyHitThisStep ? 1 : 0,
				(TargetASC && TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Staggered)) ? 1 : 0,
				(TargetASC && TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Hit)) ? 1 : 0,
				(TargetASC && TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Groggy)) ? 1 : 0,
				(TargetASC && TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Dead)) ? 1 : 0);
		}

		if (bAlreadyHitThisStep) continue;

		if (TargetASC)
		{
			FGameplayEffectContextHandle PerHitContext = SourceASC->MakeEffectContext();
			PerHitContext.AddInstigator(AvatarActor, AvatarActor);
			PerHitContext.AddSourceObject(this);
			
			PerHitContext.AddHitResult(Hit, /*bReset=*/true);

			FGameplayEffectSpecHandle PerHitSpec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), PerHitContext);
			if (!PerHitSpec.IsValid() || !PerHitSpec.Data.IsValid()) continue;

			PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, DamageMul);

			// 넉백 강도(콤보 스텝별). BroadcastHitEvent가 공격자→피격자로 자동 적용.
			if (CachedComboSteps.IsValidIndex(CurrentComboIndex) && CachedComboSteps[CurrentComboIndex].KnockbackStrength > 0.f)
			{
				const FWeaponComboStep& KbStep = CachedComboSteps[CurrentComboIndex];
				PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_Strength, KbStep.KnockbackStrength);
				PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_UpwardStrength, KbStep.KnockbackUpwardStrength);
			}

			// Attack.Type (방어 처리 결정)
			PerHitSpec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Normal);

			// HitReact.Type (피격 반응 결정)
			const ERetrieveHitReactType ReactType = CachedComboSteps.IsValidIndex(CurrentComboIndex)
				? CachedComboSteps[CurrentComboIndex].HitReactType
				: ERetrieveHitReactType::Flinch;
			AddCombatTagsToDamageSpec(
				*PerHitSpec.Data.Get(),
				ResolveCurrentElementTag(),
				RetrieveGameplayTags::Attack_Type_Normal,
				FGameplayTag(),
				HitReactTypeToTag(ReactType));

			SourceASC->ApplyGameplayEffectSpecToTarget(*PerHitSpec.Data.Get(), TargetASC);
			HitActorsThisStep.Add(TargetActor);
			
			if (!bComboChargeBonusGranted)
			{
				const FGameplayTag BonusTag = CachedComboSteps.IsValidIndex(CurrentComboIndex)
					? CachedComboSteps[CurrentComboIndex].ChargeBonusEventTag : FGameplayTag();
				if (BonusTag.IsValid())
				{
					FGameplayEventData BonusEvent;
					BonusEvent.Instigator = AvatarActor;
					BonusEvent.Target = TargetActor;
					BonusEvent.EventTag = BonusTag;
					UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(AvatarActor, BonusTag, BonusEvent);
					bComboChargeBonusGranted = true;
				}
			}
		}
	}
}

void UGA_Attack::BuildTracePoints(TArray<FVector>& OutPoints) const
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

void UGA_Attack::HandleMontageBlendOut()
{
	// 콤보 전환은 ComboWindow 열림/입력 프레임에서 즉시 처리한다.
	// 콤보 전환은 리졸버가 처리한다. BlendOut은 콤보에 관여하지 않으며 섹션 자연 종료는 HandleMontageCompleted가 맡는다.
}

float UGA_Attack::GetMontagePlayRate() const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (const UCombatAttributeSet* Combat = ASC->GetSet<UCombatAttributeSet>())
		{
			return FMath::Max(0.1f, Combat->GetAttackSpeedMultiplier());
		}
	}
	return 1.f;
}

void UGA_Attack::HandleMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Attack::HandleMontageInterrupted()
{	
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
void UGA_Attack::HandleMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Attack::StopRuntimeTasks()
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

void UGA_Attack::CleanupAttackWindowTags() const
{
	if (URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		RetrieveASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Combo_Open, 0);
		RetrieveASC->ClearAttackCancelWindows(RetrieveGameplayTags::State_Attack_CancelOpen);
	}
	else if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Combo_Open, 0);
		ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Attack_CancelOpen, 0);
	}
}

void UGA_Attack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();
	CleanupAttackWindowTags();

	// 공격 종료 후 루트모션 잔류/후방 속도로 ALS가 캐릭터를 도는 것 방지. 이동 입력 시 자동 해제.
	if (ARetrieveAlsCharacter* AlsCharacter = Cast<ARetrieveAlsCharacter>(GetAvatarActorFromActorInfo()))
	{
		AlsCharacter->SetHoldFacing(true);
	}

	CurrentComboIndex = INDEX_NONE;

	HitActorsThisStep.Reset();
	PreviousTracePoints.Reset();
	bHasValidPreviousTracePoints = false;
	CachedWeaponComponent = nullptr;
	CachedComboSteps.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Attack::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	StopRuntimeTasks();
	CleanupAttackWindowTags();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
