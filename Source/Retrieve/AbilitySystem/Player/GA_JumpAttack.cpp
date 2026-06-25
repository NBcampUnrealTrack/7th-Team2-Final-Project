#include "AbilitySystem/Player/GA_JumpAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Animation/RetrieveWeaponSockets.h"
#include "Character/RetrieveAlsCharacter.h"
#include "Combat/RetrieveKnockbackLibrary.h"
#include "Components/Player/WeaponComponent.h"
#include "Data/WeaponAttackDefinition.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"

UGA_JumpAttack::UGA_JumpAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_JumpAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Type_Attack);
	SetAssetTags(Tags);

	// 공중 전용 어빌리티
	bBlockActivationWhileAirborne = false;

	// 버퍼 사용 + 공격류 우선순위. 어떤 공격에서 점프공격으로 캔슬할 수 있는지는 몽타주 AllowedCancelIntents가 정한다.
	bUseCombatInputBuffer = true;
	CombatInputPriority = 10;

	// 상태 게이트(사망/피격/다운)때 동작 불가
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);

	// "공격 중" 상태 태그를 재사용
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);

	CancelAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Type_Attack);

	// 공중 공격 중 가드 차단
	BlockAbilitiesWithTag.AddTag(RetrieveGameplayTags::Ability_Player_Guard);
}

bool UGA_JumpAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 실제로 낙하(공중) 중일 때만 발동. IsAvatarAirborne은 bPressedJump도 공중으로 치는데,
	// 점프 입력만 들어가고 실제론 못 뜬 접지 상태에서 발동되면 착지 이벤트(LandedDelegate)가 영영 안 와
	// 다이브 루프 몽타주가 그 자리에서 무한 반복된다. 버퍼가 있어 실제 이륙 직후 발동돼도 늦지 않다.
	const ACharacter* AirborneCharacter = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UCharacterMovementComponent* MoveComp = AirborneCharacter ? AirborneCharacter->GetCharacterMovement() : nullptr;
	if (!MoveComp || !MoveComp->IsFalling())
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
	
	bLandingHandled = false;
	CachedWeaponData = CachedWeaponComponent->GetWeaponDataRef();

	// 콤보 정의에서 현재 원소의 Jump variant 해결 (없으면 기본 variant)
	UWeaponAttackDefinition* ComboDefinition = CachedWeaponData.AttackComboDefinition.LoadSynchronous();
	const FWeaponJumpAttack* ResolvedJump = ComboDefinition ? ComboDefinition->ResolveJumpVariant(ResolveCurrentElementTag()) : nullptr;
	if (!ResolvedJump)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	CachedJumpData = *ResolvedJump;

	UAnimMontage* Montage = CachedJumpData.Montage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ResolveHeightTier();

	ACharacter* Character = Cast<ACharacter>(AvatarActor);

	// 찍기 착지 시 낙법(ALS Rolling on Land) 억제
	if (ARetrieveAlsCharacter* AlsChar = Cast<ARetrieveAlsCharacter>(AvatarActor))
	{
		AlsChar->SetSuppressLandingRoll(true);
	}
	
	if (Character)
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			if (CachedJumpData.DiveGravityScale > 0.f)
			{
				SavedGravityScale = MoveComp->GravityScale;
				MoveComp->GravityScale = CachedJumpData.DiveGravityScale;
				bGravityModified = true;
			}
		}
	}
	
	if (Character)
	{
		Character->LandedDelegate.AddDynamic(this, &ThisClass::HandleLanded);
		BoundLandedCharacter = Character;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.f, CachedJumpData.SectionName, true);
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

	const float Radius = ResolvedAoeRadius;
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

	const float DamageMul = ResolvedDamageMultiplier;

	TArray<ACharacter*> KnockbackTargets;

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
		
		AddCombatTagsToDamageSpec(
			*PerHitSpec.Data.Get(),
			ResolveCurrentElementTag(),
			RetrieveGameplayTags::Attack_Type_Normal,
			FGameplayTag(),
			HitReactTypeToTag(ResolvedHitReactType));

		SourceASC->ApplyGameplayEffectSpecToTarget(*PerHitSpec.Data.Get(), TargetASC);
		HitActors.Add(TargetActor);

		if (ACharacter* HitCharacter = Cast<ACharacter>(TargetActor))
		{
			KnockbackTargets.Add(HitCharacter);
		}

		if (!bChargeBonusGranted)
		{
			const FGameplayTag BonusTag = CachedJumpData.ChargeBonusEventTag;
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

	if (CachedJumpData.bUseLandingKnockback)
	{
		URetrieveKnockbackLibrary::ApplyRadialKnockbackToTargets(Center, Radius, KnockbackTargets, CachedJumpData.LandingKnockback);
	}
}

void UGA_JumpAttack::ResolveHeightTier()
{
	const FWeaponJumpAttack& Data = CachedJumpData;
	
	ResolvedDamageMultiplier = Data.DamageMultiplier;
	ResolvedHitReactType = Data.HitReactType;
	ResolvedAoeRadius = Data.LandingAoeRadius;

	if (Data.HeightTiers.IsEmpty())
	{
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!IsValid(AvatarActor) || !IsValid(World))
	{
		return;
	}
	
	float HalfHeight = 0.f;
	if (const ACharacter* Char = Cast<ACharacter>(AvatarActor))
	{
		if (const UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
		{
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}
	const FVector Start = AvatarActor->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	const FVector End = Start - FVector(0.f, 0.f, 100000.f);

	float MeasuredHeight = TNumericLimits<float>::Max();
	FHitResult GroundHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GA_JumpAttack_HeightProbe), false, AvatarActor);
	if (World->LineTraceSingleByChannel(GroundHit, Start, End, ECC_Visibility, Params))
	{
		MeasuredHeight = FMath::Max(0.f, static_cast<float>((Start - GroundHit.ImpactPoint).Size()));
	}

	// MinHeight <= 측정높이 인 구간 중 MinHeight가 가장 큰 구간 선택
	const FJumpAttackHeightTier* Best = nullptr;
	for (const FJumpAttackHeightTier& Tier : Data.HeightTiers)
	{
		if (MeasuredHeight >= Tier.MinHeight && (!Best || Tier.MinHeight > Best->MinHeight))
		{
			Best = &Tier;
		}
	}

	if (Best)
	{
		ResolvedDamageMultiplier = Best->DamageMultiplier;
		ResolvedHitReactType = Best->HitReactType;
		ResolvedAoeRadius = (Best->AoeRadiusOverride > 0.f) ? Best->AoeRadiusOverride : Data.LandingAoeRadius;
	}

	if (bDebugDrawTrace)
	{
		UE_LOG(LogRetrieveCombat, Log, TEXT("[JumpAttack] Height=%.0f -> DamageMul=%.2f, Radius=%.0f"),
			MeasuredHeight, ResolvedDamageMultiplier, ResolvedAoeRadius);
	}
}

void UGA_JumpAttack::RestoreGravityScale()
{
	if (bGravityModified)
	{
		if (const ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			if (UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
			{
				MoveComp->GravityScale = SavedGravityScale;
			}
		}
		bGravityModified = false;
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
	if (bLandingHandled)
	{
		return;
	}
	bLandingHandled = true;
	
	UnbindLanded();
	ApplyLandingAoe();
	RestoreGravityScale();
	
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (!CachedJumpData.LandingSectionName.IsNone())
		{
			ASC->CurrentMontageJumpToSection(CachedJumpData.LandingSectionName);
			return;
		}
		
		ASC->CurrentMontageStop(0.1f); // Landing Section 없을 시 Fallback(Montage Stop)
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
	
	if (bGravityModified)
	{
		if (const ACharacter* Char = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			if (UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
			{
				MoveComp->GravityScale = SavedGravityScale;
			}
		}
		bGravityModified = false;
	}

	HitActors.Reset();
	bChargeBonusGranted = false;
	bLandingHandled = false;
	CachedWeaponComponent = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_JumpAttack::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	StopRuntimeTasks();
	UnbindLanded();
	RestoreGravityScale();
	bLandingHandled = false;
	
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
