

#include "GA_Undead_SelfDestruct.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "CollisionShape.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Enemy/EnemyAIController.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

UGA_Undead_SelfDestruct::UGA_Undead_SelfDestruct(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SelfDestruct);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SelfDestructing);

	// 자폭은 어떤 상태에서도 발동. 점화되면 못 멈춘다.
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SelfDestruct;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_Undead_SelfDestruct::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (IsValid(Pawn) == false || Pawn->HasAuthority() == false)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UEnemyCombatComponent* Combat = Pawn->FindComponentByClass<UEnemyCombatComponent>();

	// 타깃: 네크로맨서 처치자 -> 공격 포커스 -> 플레이어 폴백
	AActor* Target = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Instigator.Get()) : nullptr;
	if (AController* AsController = Cast<AController>(Target))
	{
		Target = AsController->GetPawn();
	}
	if (IsValid(Target) == false && IsValid(Combat))
	{
		Target = Combat->GetFocusTarget();
	}
	if (IsValid(Target) == false)
	{
		Target = UGameplayStatics::GetPlayerPawn(GetWorld(), 0); // 폴백: 플레이어
	}
	ChaseTarget = Target;

	// 공격/패턴 정지 + StateTree 양보
	if (Combat)
	{
		Combat->StopCurrentPattern();
	}
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(Pawn->GetController()))
	{
		AIC->Deactivate(); // StateTree 정지 — GA가 이동 독점
	}

	ApplyChaseSpeed();

	ChaseElapsed = 0.f;
	bFuseStarted = false;
	bDetonated = false;

	UE_LOG(LogRetrieveCombat, Warning, TEXT("[Undead_SelfDestruct] Activated Owner=%s Target=%s"),
		*GetNameSafe(Pawn), *GetNameSafe(ChaseTarget.Get()));

	BeginChase();
}

void UGA_Undead_SelfDestruct::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChasePollTimer);
		World->GetTimerManager().ClearTimer(HitboxOffTimer);
	}

	if (IsValid(MontageTask))
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Undead_SelfDestruct::BeginChase()
{
	APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (IsValid(Pawn) == false)
	{
		BeginFuse();
		return;
	}

	// 이동 목표가 무빙 액터여도 PathFollowing이 추적한다 → 1회 호출
	if (AAIController* AIC = Cast<AAIController>(Pawn->GetController()))
	{
		if (ChaseTarget.IsValid())
		{
			AIC->MoveToActor(ChaseTarget.Get(), FuseTriggerRadius, /*bStopOnOverlap*/true, /*bUsePathfinding*/true);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ChasePollTimer, this, &ThisClass::TickChase, ChasePollInterval, true);
	}
}

void UGA_Undead_SelfDestruct::TickChase()
{
	if (bFuseStarted)
	{
		return;
	}

	ChaseElapsed += ChasePollInterval;

	APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	AActor* Target = ChaseTarget.Get();

	// 폰 무효 / 타깃 소실 / 타임아웃 → 제자리 자폭
	if (IsValid(Pawn) == false || IsValid(Target) == false || ChaseElapsed >= MaxChaseTime)
	{
		BeginFuse();
		return;
	}

	const float Dist = FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation());
	if (Dist <= FuseTriggerRadius)
	{
		BeginFuse();
	}
}

void UGA_Undead_SelfDestruct::BeginFuse()
{
	if (bFuseStarted)
	{
		return;
	}
	bFuseStarted = true;

	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(ChasePollTimer);
	}

	APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (AAIController* AIC = (IsValid(Pawn) ? Cast<AAIController>(Pawn->GetController()) : nullptr))
	{
		AIC->StopMovement();
	}

	UAnimMontage* Montage = FuseMontage.IsNull() ? nullptr : FuseMontage.LoadSynchronous();
	if (IsValid(Montage) == false)
	{
		// 퓨즈 몽타주 없으면 즉시 폭발
		Detonate();
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, GetAttackMontagePlayRate(1.f), NAME_None, /*bStopWhenAbilityEnds*/true);

	if (IsValid(MontageTask) == false)
	{
		Detonate();
		return;
	}

	// 자폭은 못 멈춘다 → 완료/중단/취소 모두 폭발로 귀결
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnFuseFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnFuseFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnFuseFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnFuseFinished);
	MontageTask->ReadyForActivation();
}

void UGA_Undead_SelfDestruct::OnFuseFinished()
{
	Detonate();
}

void UGA_Undead_SelfDestruct::Detonate()
{
	if (bDetonated)
	{
		return;
	}
	bDetonated = true;

	APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();

	if (IsValid(Pawn) && IsValid(SourceASC) && ExplosionDamageEffect && IsValid(World))
	{
		// 자기 사망 전에 잔여 HP로 계수 계산
		float HpRatio = 1.f;
		if (const URetrieveHealthComponent* Health = Pawn->FindComponentByClass<URetrieveHealthComponent>())
		{
			const float MaxHp = Health->GetMaxHealth();
			HpRatio = (MaxHp > 0.f) ? FMath::Clamp(Health->GetHealth() / MaxHp, 0.f, 1.f) : 0.f;
		}
		const float FinalMul = FMath::Max(0.f, BaseDamageMul * FMath::Lerp(MulAtZeroHp, MulAtFullHp, HpRatio));

		// 2) 광역 오버랩 (Pawn 위치 기준 구체)
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(UndeadSelfDestruct), false, Pawn);
		World->OverlapMultiByChannel(
			Overlaps, Pawn->GetActorLocation(), FQuat::Identity,
			ECC_Pawn, FCollisionShape::MakeSphere(ExplosionRadius), Params);

		TSet<AActor*> Damaged;
		for (const FOverlapResult& O : Overlaps)
		{
			AActor* Other = O.GetActor();
			if (IsValid(Other) == false || Other == Pawn || Damaged.Contains(Other))
			{
				continue;
			}

			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Other);
			if (TargetASC == nullptr)
			{
				continue;
			}    // ASC 있는 대상만(플레이어 등)
			Damaged.Add(Other);

			FGameplayEffectContextHandle Ctx = SourceASC->MakeEffectContext();
			Ctx.AddInstigator(Pawn, Pawn);
			FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(ExplosionDamageEffect, 1.f, Ctx);
			if (Spec.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, FinalMul);
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}
	}
	KillSelf();

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Undead_SelfDestruct::KillSelf()
{
	if (SelfKillEffectClass == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (IsValid(ASC) == false)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(SelfKillEffectClass, 1.f, Context);
	if (Spec.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UGA_Undead_SelfDestruct::ApplyChaseSpeed()
{
	APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (IsValid(Pawn) == false || ChaseSpeedMultiplier <= 1.f)
	{
		return;
	}

	if (UCharacterMovementComponent* Move = Pawn->FindComponentByClass<UCharacterMovementComponent>())
	{
		OriginalMaxWalkSpeed = Move->MaxWalkSpeed;
		Move->MaxWalkSpeed *= ChaseSpeedMultiplier;
	}
}
