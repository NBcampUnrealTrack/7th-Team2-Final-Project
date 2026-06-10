#include "AbilitySystem/Player/GA_JumpAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
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

	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	
	if (Character && CachedWeaponData.JumpAttack.DiveDownSpeed > 0.f)
	{
		Character->LaunchCharacter(FVector(0.f, 0.f, -CachedWeaponData.JumpAttack.DiveDownSpeed),
			/*bXYOverride=*/false, /*bZOverride=*/true);
	}

	if (Character)
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

void UGA_JumpAttack::ApplyLandingAoe()
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

	const float Radius = CachedWeaponData.JumpAttack.LandingAoeRadius;
	if (Radius <= 0.f)
	{
		return;
	}
	
	FVector Center = AvatarActor->GetActorLocation();
	const FName Socket = CachedWeaponData.TraceSocketName.IsNone() ? RetrieveWeaponSockets::Weapon_R : CachedWeaponData.TraceSocketName;
	if (IsValid(CachedWeaponComponent))
	{
		if (UMeshComponent* TraceMesh = CachedWeaponComponent->GetWeaponMeshForTrace(Socket, Socket))
		{
			if (TraceMesh->DoesSocketExist(Socket))
			{
				Center = TraceMesh->GetSocketLocation(Socket);
			}
		}
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GA_JumpAttack_LandingAoe), false, AvatarActor);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	if (bDebugDrawTrace)
	{
		DrawDebugSphere(World, Center, Radius, 16, Overlaps.Num() > 0 ? FColor::Green : FColor::Orange, false, 2.f);
	}

	if (Overlaps.IsEmpty())
	{
		return;
	}

	const float DamageMul = CachedWeaponData.JumpAttack.DamageMultiplier;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* TargetActor = Overlap.GetActor();
		if (!IsValid(TargetActor) || TargetActor == AvatarActor) continue;
		if (HitActors.Contains(TargetActor)) continue;

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!TargetASC) continue;

		FGameplayEffectContextHandle PerHitContext = SourceASC->MakeEffectContext();
		PerHitContext.AddInstigator(AvatarActor, AvatarActor);
		PerHitContext.AddSourceObject(this);

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
	ApplyLandingAoe();

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
