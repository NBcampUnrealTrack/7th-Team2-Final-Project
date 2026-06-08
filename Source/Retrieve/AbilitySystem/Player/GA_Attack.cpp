#include "AbilitySystem/Player/GA_Attack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Animation/RetrieveWeaponSockets.h"
#include "Components/WeaponComponent.h"
#include "Data/AttackComboDefinition.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "Player/RetrievePlayerState.h"

UGA_Attack::UGA_Attack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	SetAssetTags(Tags);

	// 공중/점프 중 공격 불가
	bBlockActivationWhileAirborne = true;
	
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);

	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);
	
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

	StartComboStep(0);
}

bool UGA_Attack::ResolveAttackComboVariant()
{
	CachedComboSteps.Reset();

	UAttackComboDefinition* ComboDefinition = CachedWeaponData.AttackComboDefinition.LoadSynchronous();
	if (!IsValid(ComboDefinition))
	{
		return false;
	}

	const FGameplayTag ElementTag = ResolveCurrentElementTag();
	CachedElementTag = ElementTag;
	const FAttackComboVariant* Variant = ComboDefinition->ResolveVariant(ElementTag);
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

FGameplayTag UGA_Attack::ResolveCurrentElementTag() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const ARetrievePlayerState* RetrievePlayerState = AvatarPawn ? AvatarPawn->GetPlayerState<ARetrievePlayerState>() : nullptr;
	return RetrievePlayerState ? RetrievePlayerState->GetCurrentElementTag() : FGameplayTag();
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
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	CurrentComboIndex = StepIndex;          // 재생 시점에 인덱스 직접 확정 (ImpactBegin 커밋 불필요)
	PendingComboIndex = INDEX_NONE;
	bPendingElementRestart = false;
	bComboChargeBonusGranted = false;

	HitActorsThisStep.Reset();
	PreviousTracePoints.Reset();
	bHasValidPreviousTracePoints = false;

	const FWeaponComboStep& StepData = CachedComboSteps[StepIndex];

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, CachedAttackMontage, GetMontagePlayRate(), StepData.SectionName, true);
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

	StartListeningComboInput();
}

void UGA_Attack::StartListeningComboInput()
{
	if (InputPressTask)
	{
		InputPressTask->EndTask();
		InputPressTask = nullptr;
	}

	InputPressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	if (InputPressTask)
	{
		InputPressTask->OnPress.AddDynamic(this, &ThisClass::HandleInputPressed);
		InputPressTask->ReadyForActivation();
	}
}

void UGA_Attack::HandleInputPressed(float TimeWaited)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		return;
	}
	
	if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Combo_Open))
	{
		if (ResolveCurrentElementTag() == CachedElementTag)
		{
			const int32 NextStep = CurrentComboIndex + 1;
			if (CachedComboSteps.IsValidIndex(NextStep))
			{
				PendingComboIndex = NextStep;   // 예약만 — 현재 섹션은 끊지 않음. OnBlendOut에서 소비(크로스블렌드)
				bPendingElementRestart = false;
			}
			// 마지막 타면 무시 — 현재 몽타주 완주 후 종료
		}
		else
		{
			bPendingElementRestart = true;  // 다른 원소 → 완주 후 재시작
			PendingComboIndex = INDEX_NONE;
		}
	}

	StartListeningComboInput();
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

			// Attack.Type (방어 처리 결정)
			PerHitSpec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Normal);

			// HitReact.Type (피격 반응 결정)
			const ERetrieveHitReactType ReactType = CachedComboSteps.IsValidIndex(CurrentComboIndex)
				? CachedComboSteps[CurrentComboIndex].HitReactType
				: ERetrieveHitReactType::Flinch;
			if (const FGameplayTag ReactTag = HitReactTypeToTag(ReactType); ReactTag.IsValid())
			{
				PerHitSpec.Data->AddDynamicAssetTag(ReactTag);
			}

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
	// 섹션 자연 종료의 블렌드아웃 시작 시점. 예약된 다음 타가 있으면 여기서 재생 → 공격끼리 크로스블렌드.
	// 예약이 없으면 아무것도 안 함 → 그대로 블렌드아웃 진행 → OnCompleted가 종료를 처리.
	// (외부 중단은 OnBlendOut이 아니라 OnInterrupted로 분기되므로 여기서 콤보로 오인하지 않음)
	const int32 Next = PendingComboIndex;
	PendingComboIndex = INDEX_NONE;
	if (CachedComboSteps.IsValidIndex(Next))
	{
		StartComboStep(Next);
	}
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
	if (bPendingElementRestart)
	{
		bPendingElementRestart = false;
		if (ResolveAttackComboVariant())   // 현재 원소 다시 읽어 새 몽타주+스텝+CachedElementTag 갱신
		{
			StartComboStep(0);             // 새 원소 1타부터
			return;
		}
	}
	
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
	if (InputPressTask)
	{
		InputPressTask->EndTask(); InputPressTask = nullptr;
	}
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

void UGA_Attack::CleanupComboTag() const
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Combo_Open, 0);
	}
}

void UGA_Attack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();
	CleanupComboTag();

	CurrentComboIndex = INDEX_NONE;
	PendingComboIndex = INDEX_NONE;
	bPendingElementRestart = false;
	
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
	CleanupComboTag();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
