

#include "GA_Necromancer_SummonUndeads.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Enemy/NecroUndeadComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

UGA_Necromancer_SummonUndeads::UGA_Necromancer_SummonUndeads(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack_SummonUndeads;
	TriggerData.TriggerSource =	EGameplayAbilityTriggerSource::GameplayEvent;

	AbilityTriggers.Add(TriggerData);
}

void UGA_Necromancer_SummonUndeads::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (IsValid(FirstMinionClass) == false && IsValid(SecondMinionClass) == false)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NecromancerSummon] No minion classes assigned."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bSummonExecuted = false;
	bMontageFinished = false;

	const float FinalPlayRate =	GetAttackMontagePlayRate(MontagePlayRate);

	ActiveMontage = ResolveSummonMontage(TriggerEventData);

	if (IsValid(ActiveMontage) == false)
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[NecromancerSummon] Invalid summon montage. Owner=%s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()));

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UWorld* World = GetWorld();
	if (IsValid(World) == false)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		ActiveMontage,
		FinalPlayRate,
		NAME_None,
		true);

	if (IsValid(MontageTask) == false)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);

	MontageTask->ReadyForActivation();

	UE_LOG(LogRetrieveCombat, Warning,
		TEXT("[NecromancerSummon] Activated Owner=%s Delay=%.2f Montage=%s First=%s Second=%s Authority=%d"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		SummonDelay,
		*GetNameSafe(ActiveMontage),
		*GetNameSafe(FirstMinionClass),
		*GetNameSafe(SecondMinionClass),
		IsValid(GetAvatarActorFromActorInfo()) ? GetAvatarActorFromActorInfo()->HasAuthority() : false);

	FTimerDelegate SummonDelegate;
	SummonDelegate.BindUObject(this, &ThisClass::ExecuteSummon);

	if (SummonDelay <= 0.f)
	{
		ExecuteSummon();
	}
	else
	{
		World->GetTimerManager().SetTimer(
			SummonTimerHandle,
			SummonDelegate,
			SummonDelay,
			false);
	}
}

void UGA_Necromancer_SummonUndeads::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	UE_LOG(LogRetrieveCombat, Warning,
		TEXT("[NecromancerSummon] EndAbility Owner=%s Cancelled=%d Executed=%d MontageFinished=%d"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		bWasCancelled,
		bSummonExecuted,
		bMontageFinished);

	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(SummonTimerHandle);
	}

	if (IsValid(MontageTask))
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	ActiveMontage = nullptr;
	bSummonExecuted = false;
	bMontageFinished = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Necromancer_SummonUndeads::OnMontageCompleted()
{
	UE_LOG(LogRetrieveCombat, Warning,
		TEXT("[NecromancerSummon] Montage Completed Owner=%s Executed=%d"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		bSummonExecuted);

	bMontageFinished = true;
	TryFinishAbility();
}

void UGA_Necromancer_SummonUndeads::OnMontageInterrupted()
{
	UE_LOG(LogRetrieveCombat, Warning,
		TEXT("[NecromancerSummon] Montage Interrupted Owner=%s Executed=%d"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		bSummonExecuted);

	FinishAbility(true);
}

UAnimMontage* UGA_Necromancer_SummonUndeads::ResolveSummonMontage(const FGameplayEventData* TriggerEventData) const
{
	if (TriggerEventData)
	{
		if (const UAnimMontage* EventMontage = Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()))
		{
			return const_cast<UAnimMontage*>(EventMontage);
		}
	}

	const FMonsterPatternRow* PatternRow = GetActivePatternRow();
	if (!PatternRow || PatternRow->AttackMontage.IsNull())
	{
		return nullptr;
	}

	return PatternRow->AttackMontage.LoadSynchronous();
}

void UGA_Necromancer_SummonUndeads::ExecuteSummon()
{
	UE_LOG(LogRetrieveCombat, Warning,
		TEXT("[NecromancerSummon] ExecuteSummon Enter Owner=%s Authority=%d Executed=%d MontageFinished=%d"),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		IsValid(GetAvatarActorFromActorInfo()) ? GetAvatarActorFromActorInfo()->HasAuthority() : false,
		bSummonExecuted,
		bMontageFinished);

	if (bSummonExecuted)
	{
		return;
	}

	bSummonExecuted = true;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (IsValid(Avatar) == false || Avatar->HasAuthority() == false)
	{
		TryFinishAbility();
		return;
	}

	CompactSpawnedUndeads();

	const int32 CurrentCount = SpawnedUndeads.Num();
	const int32 RemainingSlots = FMath::Max(0, MaxMinionCount - CurrentCount);

	if (RemainingSlots <= 0)
	{
		UE_LOG(LogRetrieveCombat, Log,
			TEXT("[NecromancerSummon] Max minion count reached. Owner=%s Count=%d/%d"),
			*GetNameSafe(Avatar),
			CurrentCount,
			MaxMinionCount);

		TryFinishAbility();
		return;
	}

	int32 SpawnedCount = 0;

	UNecroUndeadComponent* UndeadComp = Avatar->FindComponentByClass<UNecroUndeadComponent>();
	if (IsValid(UndeadComp) == false)
	{
		return;
	}

	if (RemainingSlots >= 1)
	{
		if (APawn* SpawnedPawn = SpawnMinion(FirstMinionClass, -SpawnSideDistance))
		{
			SpawnedUndeads.Add(SpawnedPawn);
			UndeadComp->RegisterUndead(SpawnedPawn);
			++SpawnedCount;
		}
	}

	if (RemainingSlots >= 2)
	{
		if (APawn* SpawnedPawn = SpawnMinion(SecondMinionClass, SpawnSideDistance))
		{
			SpawnedUndeads.Add(SpawnedPawn);
			UndeadComp->RegisterUndead(SpawnedPawn);
			++SpawnedCount;
		}
	}

	UE_LOG(LogRetrieveCombat, Log, TEXT("[NecromancerSummon] Summon executed. Owner=%s Spawned=%d Count=%d/%d"),
		*GetNameSafe(Avatar),
		SpawnedCount,
		SpawnedUndeads.Num(),
		MaxMinionCount);

	TryFinishAbility();
}

void UGA_Necromancer_SummonUndeads::CompactSpawnedUndeads()
{
	SpawnedUndeads.RemoveAll([](const TWeakObjectPtr<APawn>& WeakPawn)
		{
			APawn* Pawn = WeakPawn.Get();
			if (IsValid(Pawn) == false)
			{
				return true;
			}

			const URetrieveHealthComponent* HealthComponent = Pawn->FindComponentByClass<URetrieveHealthComponent>();

			return HealthComponent && HealthComponent->IsDeadOrDying();
		});
}

APawn* UGA_Necromancer_SummonUndeads::SpawnMinion(TSubclassOf<APawn> MinionClass, float SideOffset)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();

	if (IsValid(Avatar) == false || IsValid(World) == false || IsValid(MinionClass) == false)
	{
		return nullptr;
	}

	const FVector Forward =	Avatar->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = Avatar->GetActorRightVector().GetSafeNormal2D();
	FVector SpawnLocation =	Avatar->GetActorLocation() + Forward * SpawnForwardDistance	+ Right * SideOffset;

	if (bProjectSpawnToNavigation)
	{
		if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(World))
		{
			FNavLocation ProjectedLocation;
			if (NavSystem->ProjectPointToNavigation(
				SpawnLocation,
				ProjectedLocation,
				NavProjectionExtent))
			{
				SpawnLocation = ProjectedLocation.Location;
			}
		}
	}

	const FRotator SpawnRotation = Avatar->GetActorRotation();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Avatar;
	SpawnParams.Instigator = Cast<APawn>(Avatar);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* SpawnedPawn = World->SpawnActor<APawn>(
		MinionClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams);

	if (IsValid(SpawnedPawn) == false)
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[NecromancerSummon] Spawn failed. Owner=%s Class=%s"),
			*GetNameSafe(Avatar),
			*GetNameSafe(MinionClass));

		return nullptr;
	}

	if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(SpawnedPawn))
	{
		Enemy->SetRespawnable(false);
	}

	if (!SpawnedPawn->GetController())
	{
		SpawnedPawn->SpawnDefaultController();
	}

	return SpawnedPawn;
}

void UGA_Necromancer_SummonUndeads::FinishAbility(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

void UGA_Necromancer_SummonUndeads::TryFinishAbility()
{
	if (bSummonExecuted && bMontageFinished)
	{
		FinishAbility(false);
	}
}
