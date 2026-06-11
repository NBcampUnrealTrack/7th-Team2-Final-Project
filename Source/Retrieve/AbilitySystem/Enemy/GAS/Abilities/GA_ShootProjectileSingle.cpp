#include "AbilitySystem/Enemy/GAS/Abilities/GA_ShootProjectileSingle.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Enemy/EnemyProjectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

UGA_ShootProjectileSingle::UGA_ShootProjectileSingle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Hit);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Groggy);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_Attack);
	//ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);

	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UGA_ShootProjectileSingle::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GA_ShootProjectileSingle] CommitAbility failed Owner=%s"),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedTargetActor = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Target.Get()) : nullptr;

	const UAnimMontage* Montage = ResolveMontage(TriggerEventData);
	const bool bHasMontage = Montage != nullptr;
	
	if (bHasMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, const_cast<UAnimMontage*>(Montage), 1.f, NAME_None, true);
		
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UGA_ShootProjectileSingle::OnMontageCompleted);
			MontageTask->OnBlendOut.AddDynamic(this, &UGA_ShootProjectileSingle::OnMontageCompleted);
			MontageTask->OnInterrupted.AddDynamic(this, &UGA_ShootProjectileSingle::OnMontageInterrupted);
			MontageTask->OnCancelled.AddDynamic(this, &UGA_ShootProjectileSingle::OnMontageInterrupted);
			MontageTask->ReadyForActivation();
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SpawnTimerHandle, this, &UGA_ShootProjectileSingle::SpawnProjectile, ProjectileSpawnDelay, false);

		if (!bHasMontage)
		{
			World->GetTimerManager().SetTimer(
				FinishTimerHandle,
				this,
				&UGA_ShootProjectileSingle::FinishAbility,
				ProjectileSpawnDelay + 0.2f,
				false);
		}
	}
}

void UGA_ShootProjectileSingle::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
		World->GetTimerManager().ClearTimer(FinishTimerHandle);
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	CachedTargetActor = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_ShootProjectileSingle::SpawnProjectile()
{
	
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_ShootProjectileSingle] Spawn skipped: no authority"));
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GA_ShootProjectileSingle] Spawn skipped: ProjectileClass is null"));
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!AvatarActor || !World)
	{
		return;
	}

	FVector SpawnLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(SpawnOffset);
	FRotator SpawnRotation = AvatarActor->GetActorRotation();

	if (USkeletalMeshComponent* Mesh = AvatarActor->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (!SpawnSocketName.IsNone() && Mesh->DoesSocketExist(SpawnSocketName))
		{
			SpawnLocation = Mesh->GetSocketLocation(SpawnSocketName);
		}
	}

	FVector Direction = AvatarActor->GetActorForwardVector();
	if (CachedTargetActor)
	{
		FVector AimLocation = CachedTargetActor->GetActorLocation();

		if (const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(CachedTargetActor->GetRootComponent()))
		{
			AimLocation = RootPrimitive->Bounds.Origin;
		}

		Direction = (AimLocation - SpawnLocation).GetSafeNormal();
		SpawnRotation = Direction.Rotation();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEnemyProjectile* Projectile = World->SpawnActor<AEnemyProjectile>(
		ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (Projectile)
	{
		Projectile->Launch(Direction, ProjectileSpeed);
	}
}

void UGA_ShootProjectileSingle::FinishAbility()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

const UAnimMontage* UGA_ShootProjectileSingle::ResolveMontage(const FGameplayEventData* TriggerEventData) const
{
	if (TriggerEventData)
	{
		if (const UAnimMontage* EventMontage = Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()))
		{
			return EventMontage;
		}
	}

	return DefaultMontage;
}

void UGA_ShootProjectileSingle::OnMontageCompleted()
{
	FinishAbility();
}

void UGA_ShootProjectileSingle::OnMontageInterrupted()
{
	UE_LOG(LogTemp, Warning, TEXT("[GA_ShootProjectileSingle] Montage Interrupted"));
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}
