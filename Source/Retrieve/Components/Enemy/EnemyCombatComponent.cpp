#include "Components/Enemy/EnemyCombatComponent.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AIController.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Components/Enemy/PatternCounterComponent.h"
#include "Combat/RetrieveCombatTypes.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/RetrieveEnemyCharacter.h"

void UEnemyCombatComponent::Initialize(UDataTable* InPatternTable, const TArray<FName>& InPatternSlots)
{
	PatternTable = InPatternTable;
	PatternSlots = InPatternSlots;

}

bool UEnemyCombatComponent::RequestPatternByPriority(AActor* Target, FGameplayTag RequiredPatternType)
{
	if (!IsValid(Target))
	{
		return false;
	}
	
	FGameplayTag DefaultEventTag;
	bool bIsSpecialAttack = false;
	if (RequiredPatternType.MatchesTagExact(RetrieveGameplayTags::Ability_Enemy_Attack))
	{
		DefaultEventTag = RetrieveGameplayTags::GameplayEvent_Enemy_Attack;
	}
	else if (RequiredPatternType.MatchesTagExact(RetrieveGameplayTags::Ability_Enemy_SpecialAttack))
	{
		DefaultEventTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack;
		bIsSpecialAttack = true;
	}
	else
	{
		return false;
	}
	
	FName BestPatternRowName = NAME_None;
	const FMonsterPatternRow* BestPattern = FindBestPattern(Target, RequiredPatternType, &BestPatternRowName);
	if (!BestPattern)
	{
		if (bIsSpecialAttack)
		{
			UE_LOG(LogRetrieveCombat, Verbose,
				TEXT("[%s] No SpecialAttack pattern currently available. Target=%s"),
				*GetOwner()->GetName(),
				*GetNameSafe(Target));
		}
		else
		{
			UE_LOG(LogRetrieveCombat, Warning,
				TEXT("[%s] No attack pattern found. Target=%s"),
				*GetOwner()->GetName(),
				*GetNameSafe(Target));
		}
		
		return false;
	}
	
	/** SpecialGolbalCooldown은 SpecialAttack 시도 시에만 */
	if (bIsSpecialAttack)
	{
		StartSpecialAttackRetryCooldown();
	}
	/** 일반 공격은 HitBox가 기반이기에 체크 필요*/
	else if (BestPattern->HitboxBoneName.IsNone())
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[%s] Attack pattern has no HitboxBoneName. Row=%s"),
			*GetOwner()->GetName(),
			*BestPatternRowName.ToString());
		return false;
	}
	
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return false;
	}

	ActivePatternRowName = BestPatternRowName;
	SetFocusTarget(Target);
	
	if (UPatternCounterComponent* PatternCounter = GetOwner()->FindComponentByClass<UPatternCounterComponent>())
	{
		PatternCounter->SetActivePatternRow(ActivePatternRowName, PatternTable.Get());
	}

	if (!bIsSpecialAttack && BestPattern->AttackMontage.IsNull())
	{
		if (!TryStartSequencePattern(*BestPattern, BestPatternRowName, Target))
		{
			ActivePatternRowName = NAME_None;
			if (UPatternCounterComponent* PatternCounter = GetOwner()->FindComponentByClass<UPatternCounterComponent>())
			{
				PatternCounter->CloseCounterWindow();
			}
			return false;
		}

		StartCooldown(ActivePatternRowName, BestPattern->Cooldown);
		return true;
	}

	UObject* EventAnimation = BestPattern->AttackMontage.LoadSynchronous();
	if (!EventAnimation)
	{
		EventAnimation = BestPattern->AttackSequence.LoadSynchronous();
	}

	FGameplayEventData EventData;
	EventData.OptionalObject = EventAnimation;
	
	const FGameplayTag AbilityEventTag = BestPattern->AbilityEventTag.IsValid()
	? BestPattern->AbilityEventTag : DefaultEventTag;
	EventData.EventTag = AbilityEventTag;
	EventData.Target = Target;
	EventData.Instigator = GetOwner();
	FaceTarget(Target);
	
	int32 TriggeredCount = ASC->HandleGameplayEvent(AbilityEventTag, &EventData);
	if (TriggeredCount <= 0 && bIsSpecialAttack && AbilityEventTag != DefaultEventTag)
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[%s] SpecialAttack event did not trigger ability with row tag. Row=%s Event=%s. Trying default event %s."),
			*GetOwner()->GetName(),
			*BestPatternRowName.ToString(),
			*AbilityEventTag.ToString(),
			*DefaultEventTag.ToString());
		EventData.EventTag = DefaultEventTag;
		TriggeredCount = ASC->HandleGameplayEvent(DefaultEventTag, &EventData);
	}
	
	if (TriggeredCount <= 0)
	{
		UE_LOG(LogRetrieveCombat, Warning,
		TEXT("[%s] %s event did not trigger ability. Row=%s Event=%s OptionalObject=%s PatternType=%s"),
		*GetOwner()->GetName(),
		bIsSpecialAttack ? TEXT("SpecialAttack") : TEXT("Attack"),
		*BestPatternRowName.ToString(),
		*EventData.EventTag.ToString(),
		*GetNameSafe(EventAnimation),
		*BestPattern->PatternType.ToString());
		ActivePatternRowName = NAME_None;
		ClearFocusTarget();
		if (UPatternCounterComponent* PatternCounter = GetOwner()->FindComponentByClass<UPatternCounterComponent>())
		{
			PatternCounter->CloseCounterWindow();
		}
		return false;
	}
	
	StartCooldown(ActivePatternRowName, BestPattern->Cooldown);
	
	if (bIsSpecialAttack)
	{
		LockSpecialAttackEvaluation(SpecialAttackEvaluationLockDuration);
	}
	
	return true;
}

bool UEnemyCombatComponent::HasAvailablePatternByType(AActor* Target, FGameplayTag PatternType) const
{
	if (!IsValid(Target))
	{
		return false;
	}

	return FindBestPattern(Target, PatternType) != nullptr;
}

bool UEnemyCombatComponent::HasPatternInRangeByTypeIgnoringCooldown(AActor* Target, FGameplayTag PatternType) const
{
	if (!IsValid(Target))
	{
		return false;
	}

	return FindBestPattern(Target, PatternType, nullptr, true) != nullptr;
}

void UEnemyCombatComponent::StopCurrentPattern()
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	FGameplayTagContainer TagsToCancel(RetrieveGameplayTags::Ability_Enemy_Attack);
	TagsToCancel.AddTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
	ASC->CancelAbilities(&TagsToCancel);

	if (UPatternCounterComponent* PatternCounter = GetOwner()->FindComponentByClass<UPatternCounterComponent>())
	{
		PatternCounter->CloseCounterWindow();
	}

	DeactivateHitbox();
	FinishSequencePattern();
	ActivePatternRowName = NAME_None;
	ClearFocusTarget();
}

bool UEnemyCombatComponent::IsPatternActive() const
{
	if (bSequencePatternActive)
	{
		return true;
	}

	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return false;
	}
	// 과한 검증 코드로 생각돼 주석처리 후 간단화 만일 이후에 과련 이슈가 있다면 해제
	// if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Attack)
	// 	|| ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_SpecialAttack))
	// {
	// 	return true;
	// }
	//
	// auto HasActiveAbilityWithTag = [ASC](const FGameplayTag& AbilityTag)
	// {
	// 	FGameplayTagContainer AbilityTags;
	// 	AbilityTags.AddTag(AbilityTag);
	//
	// 	TArray<FGameplayAbilitySpec*> MatchingSpecs;
	// 	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(
	// 		AbilityTags, MatchingSpecs, false);
	//
	// 	for (const FGameplayAbilitySpec* Spec : MatchingSpecs)
	// 	{
	// 		if (Spec && Spec->IsActive())
	// 		{
	// 			return true;
	// 		}
	// 	}
	//
	// 	return false;
	// };
	//
	// return HasActiveAbilityWithTag(RetrieveGameplayTags::Ability_Enemy_Attack)
	// 	|| HasActiveAbilityWithTag(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
	return ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Attack)
		|| ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_SpecialAttack);
}

bool UEnemyCombatComponent::IsAttackable(AActor* Target) const
{
	if (!IsValid(Target))
	{
		return false;
	}

	return FindBestPattern(Target, RetrieveGameplayTags::Ability_Enemy_Attack) != nullptr;
}

bool UEnemyCombatComponent::IsSpecialAttackEvaluationLocked() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < SpecialAttackEvaluationLockUntilTime;
}

bool UEnemyCombatComponent::IsSpecialAttackRetryCooldownReady() const
{
	const UWorld* World = GetWorld();
	return !World || World->GetTimeSeconds() >= SpecialAttackRetryCooldownUntilTime;
}

void UEnemyCombatComponent::StartSpecialAttackRetryCooldown()
{
	if (SpecialAttackRetryCooldownDuration <= 0.f)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		SpecialAttackRetryCooldownUntilTime =
			World->GetTimeSeconds() + SpecialAttackRetryCooldownDuration;
	}
}

void UEnemyCombatComponent::SuppressSpecialAttackEvaluation(float Duration)
{
	if (Duration <= 0.f)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		const float UntilTime = World->GetTimeSeconds() + Duration;
		SpecialAttackEvaluationLockUntilTime = FMath::Max(SpecialAttackEvaluationLockUntilTime, UntilTime);
		SpecialAttackRetryCooldownUntilTime = FMath::Max(SpecialAttackRetryCooldownUntilTime, UntilTime);
	}
}

void UEnemyCombatComponent::SetMovementLockedByAttack(bool bLocked)
{
	if (bMovementLockedByAttack == bLocked)
	{
		return;
	}

	bMovementLockedByAttack = bLocked;

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	if (bLocked)
	{
		MoveComp->StopMovementImmediately();
	}

	if (ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(OwnerCharacter))
	{
		EnemyCharacter->RefreshMoveSpeedFromAttribute();
	}
	else if (bLocked)
	{
		MoveComp->MaxWalkSpeed = 0.f;
	}
}

void UEnemyCombatComponent::SetFocusTarget(AActor* Target)
{
	FocusTarget = Target;
}

AActor* UEnemyCombatComponent::GetFocusTarget() const
{
	return FocusTarget.Get();
}

void UEnemyCombatComponent::ClearFocusTarget()
{
	FocusTarget.Reset();
}

void UEnemyCombatComponent::FaceFocusTarget(float DeltaTime, float InterpSpeed, bool bYawOnly)
{
	FaceActor(GetFocusTarget(), DeltaTime, InterpSpeed, bYawOnly);
}

void UEnemyCombatComponent::FaceActor(AActor* Target, float DeltaTime, float InterpSpeed, bool bYawOnly)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !IsValid(Target) || DeltaTime <= 0.f)
	{
		return;
	}

	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	FVector Direction = TargetLocation - OwnerLocation;
	if (bYawOnly)
	{
		Direction.Z = 0.f;
	}

	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FRotator TargetRotation = Direction.Rotation();
	const FRotator CurrentRotation = OwnerActor->GetActorRotation();
	const float SafeInterpSpeed = FMath::Max(0.f, InterpSpeed);
	FRotator NewRotation = SafeInterpSpeed > 0.f
		? FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, SafeInterpSpeed)
		: TargetRotation;

	if (bYawOnly)
	{
		NewRotation.Pitch = CurrentRotation.Pitch;
		NewRotation.Roll = CurrentRotation.Roll;
	}

	OwnerActor->SetActorRotation(NewRotation);
}

float UEnemyCombatComponent::GetAttackSpeedMultiplier() const
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return 1.f;
	}

	return FMath::Max(0.1f, ASC->GetNumericAttribute(UCombatAttributeSet::GetAttackSpeedMultiplierAttribute()));
}

float UEnemyCombatComponent::GetAttackMontagePlayRate(float BasePlayRate) const
{
	return FMath::Max(0.01f, BasePlayRate) * GetAttackSpeedMultiplier();
}

float UEnemyCombatComponent::GetAttackDelay(float BaseDelay, float MinDelay) const
{
	return FMath::Max(MinDelay, FMath::Max(0.f, BaseDelay) / GetAttackSpeedMultiplier());
}

bool UEnemyCombatComponent::TryStartSequencePattern(const FMonsterPatternRow& PatternRow, FName PatternRowName, AActor* Target)
{
	UAnimSequenceBase* AttackSequence = PatternRow.AttackSequence.LoadSynchronous();
	if (!AttackSequence)
	{
		return false;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return false;
	}
	FaceTarget(Target);

	USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	SetSequenceAttackTag(true);

	UAnimMontage* DynamicMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
		AttackSequence,
		TEXT("DefaultSlot"),
		0.1f,
		0.15f,
		1.f,
		1);
	if (!DynamicMontage)
	{
		SetSequenceAttackTag(false);
		return false;
	}

	bSequencePatternActive = true;
	HitActors.Empty();
	SetMovementLockedByAttack(true);

	const float HitboxStartTime = PatternRow.HitboxWindowStartTime;
	const float HitboxDuration  = PatternRow.HitboxWindowDuration;
	const float HitboxEndTime   = HitboxStartTime + HitboxDuration;
	const float FinishTime = FMath::Max(AttackSequence->GetPlayLength(), HitboxEndTime) + 0.05f;

	World->GetTimerManager().SetTimer(
		SequenceHitboxStartTimerHandle,
		this,
		&UEnemyCombatComponent::ActivateHitbox,
		HitboxStartTime,
		false);

	World->GetTimerManager().SetTimer(
		SequenceHitboxEndTimerHandle,
		this,
		&UEnemyCombatComponent::DeactivateHitbox,
		HitboxEndTime,
		false);

	World->GetTimerManager().SetTimer(
		SequenceFinishTimerHandle,
		this,
		&UEnemyCombatComponent::FinishSequencePattern,
		FinishTime,
		false);

	return true;
}

void UEnemyCombatComponent::FaceTarget(AActor* Target) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !Target)
	{
		return;
	}

	FVector Direction = Target->GetActorLocation() - OwnerActor->GetActorLocation();
	Direction.Z = 0.f;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	OwnerActor->SetActorRotation(Direction.Rotation());
}

void UEnemyCombatComponent::FinishSequencePattern()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(SequenceHitboxStartTimerHandle);
		World->GetTimerManager().ClearTimer(SequenceHitboxEndTimerHandle);
		World->GetTimerManager().ClearTimer(SequenceFinishTimerHandle);
	}

	if (bSequencePatternActive)
	{
		DeactivateHitbox();
		SetMovementLockedByAttack(false);
		bSequencePatternActive = false;
		ActivePatternRowName = NAME_None;
	}

	SetSequenceAttackTag(false);
}

void UEnemyCombatComponent::SetSequenceAttackTag(bool bEnable)
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		bSequenceAttackTagApplied = false;
		return;
	}

	const FGameplayTag AttackTag = RetrieveGameplayTags::State_Enemy_Attack;
	if (bEnable)
	{
		if (!ASC->HasMatchingGameplayTag(AttackTag))
		{
			ASC->AddLooseGameplayTag(AttackTag);
			bSequenceAttackTagApplied = true;
		}
		return;
	}

	if (bSequenceAttackTagApplied)
	{
		ASC->RemoveLooseGameplayTag(AttackTag);
		bSequenceAttackTagApplied = false;
	}
}

void UEnemyCombatComponent::ActivateHitbox()
{
	if (!ActiveHitboxComp)
	{
		return;
	}
	
	if (ActivePatternRowName.IsNone())
	{
		return;
	}
	
	const FMonsterPatternRow* Row = PatternTable->FindRow<FMonsterPatternRow>(ActivePatternRowName, TEXT(""));
	HitActors.Empty();
	
	if (!Row)
	{
		UE_LOG(LogDataTable, Error, TEXT("[%s] Row is inValid"), *GetName());
		ActiveHitboxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}
	
	const FName HitboxBoneName = Row->HitboxBoneName;
	const float HitboxRadius   = Row->HitboxRadius;
	const FVector HitboxOffset = Row->HitboxOffset;

	if (HitboxBoneName.IsNone())
	{
		ActiveHitboxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}
	
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		const bool bAttachedToBone = !HitboxBoneName.IsNone() && OwnerChar->GetMesh()
			&& OwnerChar->GetMesh()->DoesSocketExist(HitboxBoneName)
			&& ActiveHitboxComp->AttachToComponent(
			OwnerChar->GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			HitboxBoneName);

		if (!bAttachedToBone)
		{
			ActiveHitboxComp->AttachToComponent(
				OwnerChar->GetRootComponent(),
				FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			const FVector FallbackOffset = HitboxOffset.IsNearlyZero()
				? FVector(FMath::Max(120.f, HitboxRadius * 0.8f), 0.f, 40.f)
				: HitboxOffset;
			ActiveHitboxComp->SetRelativeLocation(FallbackOffset);
			UE_LOG(LogSkeletalMesh, Warning, TEXT("[%s] HitBox bone/socket unavailable. Falling back to forward capsule space. Bone=%s Row=%s"),
				*GetName(),
				*HitboxBoneName.ToString(),
				*ActivePatternRowName.ToString());
		}
		else
		{
			ActiveHitboxComp->SetRelativeLocation(HitboxOffset);
		}
	}
	
	ActiveHitboxComp->SetSphereRadius(HitboxRadius);
	ActiveHitboxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ActiveHitboxComp->UpdateOverlaps();

	TArray<AActor*> OverlappingActors;
	ActiveHitboxComp->GetOverlappingActors(OverlappingActors);
	for (AActor* OverlappingActor : OverlappingActors)
	{
		ApplyHitToActor(OverlappingActor, FHitResult());
	}
}

void UEnemyCombatComponent::ActivateHitbox(FName InBoneName, FVector InOffset, float InRadius)
{
	if (!ActiveHitboxComp)
	{
		return;
	}
	HitActors.Empty();
	
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		if (!ActiveHitboxComp->AttachToComponent(
			OwnerChar->GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			InBoneName))
		{
			UE_LOG(LogSkeletalMesh, Error, TEXT("[%s] HitBox attachment failed"), *GetName());
		}
	}
	
	ActiveHitboxComp->SetSphereRadius(InRadius);
	ActiveHitboxComp->SetRelativeLocation(InOffset);
	ActiveHitboxComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void UEnemyCombatComponent::DeactivateHitbox()
{
	if (!ActiveHitboxComp)
	{
		return;
	}
	
	ActiveHitboxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitActors.Empty();
}

void UEnemyCombatComponent::SetActiveHitbox(USphereComponent* NewHitbox)
{
	if (ActiveHitboxComp)
	{
		ActiveHitboxComp->OnComponentBeginOverlap.RemoveDynamic(
			this, &UEnemyCombatComponent::OnHitboxOverlap);
	}

	ActiveHitboxComp = NewHitbox;

	if (ActiveHitboxComp)
	{
		ActiveHitboxComp->OnComponentBeginOverlap.AddDynamic(
			this, &UEnemyCombatComponent::OnHitboxOverlap);
		ActiveHitboxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void UEnemyCombatComponent::OnHitboxOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == GetOwner())
	{
		return;
	}
	ApplyHitToActor(OtherActor, SweepResult);
}

bool UEnemyCombatComponent::ApplyHitToActor(AActor* OtherActor, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == GetOwner())
	{
		return false;
	}
	
	const IAbilitySystemInterface* TargetASCActor = 
		Cast<IAbilitySystemInterface>(OtherActor);

	if (!TargetASCActor)
	{
		return false;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return false;
	}
	
	const AAIController* OwnerController =
		Cast<AAIController>(OwnerPawn->GetController());

	if (!OwnerController ||
		OwnerController->GetTeamAttitudeTowards(*OtherActor) != ETeamAttitude::Hostile)
	{
		return false;
	}
	
	if (HitActors.Contains(OtherActor))
	{
		return false;
	}
	
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}
	
	IAbilitySystemInterface* TargetIF = Cast<IAbilitySystemInterface>(OtherActor);
	UAbilitySystemComponent* TargetASC = TargetIF ? TargetIF->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return false;
	}
	
	URetrieveAbilitySystemComponent* SourceASC = GetASC();
	if (!SourceASC || !DamageEffectClass)
	{
		return false;
	}
	
	HitActors.Add(OtherActor);

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(GetOwner(), GetOwner());
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
		DamageEffectClass, 1.f, Context);

	const FMonsterPatternRow* ActivePatternRow = nullptr;
	if (Spec.IsValid())
	{
		if (PatternTable && !ActivePatternRowName.IsNone())
		{
			ActivePatternRow = PatternTable->FindRow<FMonsterPatternRow>(
				ActivePatternRowName, TEXT("OnHitboxOverlap_HitReact"));
			if (ActivePatternRow)
			{
				if (const FGameplayTag ReactTag = HitReactTypeToTag(ActivePatternRow->HitReactType); ReactTag.IsValid())
				{
					Spec.Data->AddDynamicAssetTag(ReactTag);
				}

				// 넉백 강도를 GE에 실어 보내면 BroadcastHitEvent가 공격자(적)→피격자로 자동 적용한다.
				const FMonsterLaunchKnockbackConfig& KbCfg = ActivePatternRow->LaunchKnockbackConfig;
				if (KbCfg.bUseLaunchKnockback)
				{
					Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_Strength, KbCfg.KnockbackStrength);
					Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Knockback_UpwardStrength, KbCfg.KnockbackUpwardStrength);
				}

				if (ActivePatternRow->bCanBeParried)
				{
					Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Parryable);
				}
			}
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	return Spec.IsValid();
}

const FMonsterPatternRow* UEnemyCombatComponent::FindBestPattern(AActor* Target, FGameplayTag RequiredPatternType, FName* OutRowName, bool bIgnoreCooldown) const
{
	AActor* OwnerActor = GetOwner();
	if (!PatternTable || PatternSlots.IsEmpty() || !IsValid(OwnerActor) || !IsValid(Target))
	{
		if (OutRowName)
		{
			*OutRowName = NAME_None;
		}
		return nullptr;
	}

	const FVector PawnLocation = OwnerActor->GetActorLocation();
	const ARetrieveEnemyCharacter* OwnerEnemy = Cast<ARetrieveEnemyCharacter>(OwnerActor);
	const ACharacter* OwnerChar = Cast<ACharacter>(OwnerActor);
	const UCharacterMovementComponent* OwnerMoveComp = OwnerChar ? OwnerChar->GetCharacterMovement() : nullptr;
	// 에픽 전용: 공중 비행 중(부양)에는 수평 거리만 사용하여 고도 차이로 사거리가 초과되는 문제 방지.
	// (일반/보스는 비행하지 않으므로 항상 3D 거리 — 원본 동작)
	const bool bOwnerFlying = OwnerEnemy
		&& OwnerEnemy->ShouldUse2DPatternRangeWhileFlying()
		&& OwnerMoveComp
		&& OwnerMoveComp->MovementMode == MOVE_Flying;
	const float DistanceSq = bOwnerFlying
		? FVector::DistSquared2D(PawnLocation, Target->GetActorLocation())
		: FVector::DistSquared(PawnLocation, Target->GetActorLocation());
	const float Distance3D = FVector::Distance(PawnLocation, Target->GetActorLocation());
	const float Distance2D = FVector::Dist2D(PawnLocation, Target->GetActorLocation());
	const bool bLogPatternSelection =
		(OwnerEnemy && OwnerEnemy->ShouldUsePatternRangeForNormalAttack())
		|| RequiredPatternType.MatchesTagExact(RetrieveGameplayTags::Ability_Enemy_SpecialAttack);

	const FMonsterPatternRow* BestRow = nullptr;
	FName BestRowName = NAME_None;
	int32 BestPriority = MIN_int32;

	for (const FName& RowName : PatternSlots)
	{
		const FMonsterPatternRow* Row = PatternTable->FindRow<FMonsterPatternRow>(RowName, TEXT("UEnemyCombatComponent"), false);
		if (!Row)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] Can't find patternRow. RowName : %s"), *GetName(), *RowName.ToString());
			continue;
		}
		
		if (RequiredPatternType.IsValid() &&
			!Row->PatternType.MatchesTagExact(RequiredPatternType))
		{
			if (bLogPatternSelection)
			{
				UE_LOG(LogRetrieveCombat, Verbose,
					TEXT("[FindBestPattern] Skip type. Owner=%s Row=%s Required=%s RowType=%s"),
					*GetNameSafe(OwnerActor),
					*RowName.ToString(),
					*RequiredPatternType.ToString(),
					*Row->PatternType.ToString());
			}
			continue;
		}
		
		// 에픽 전용: MaxActivationRange == 0 → 사거리 제한 없음으로 해석.
		// 일반/보스는 원본 strict 동작 유지(MaxActivationRange 값을 그대로 사거리로 적용).
		const bool bEnforceMaxRange =
			!OwnerEnemy
			|| !OwnerEnemy->ShouldTreatZeroPatternMaxRangeAsUnlimited()
			|| Row->MaxActivationRange > 0.f;
		if (bEnforceMaxRange && DistanceSq > FMath::Square(Row->MaxActivationRange))
		{
			if (bLogPatternSelection)
			{
				UE_LOG(LogRetrieveCombat, Warning,
					TEXT("[FindBestPattern] Skip max range. Owner=%s Row=%s Required=%s Dist3D=%.1f Dist2D=%.1f Max=%.1f Flying=%d"),
					*GetNameSafe(OwnerActor),
					*RowName.ToString(),
					*RequiredPatternType.ToString(),
					Distance3D,
					Distance2D,
					Row->MaxActivationRange,
					bOwnerFlying ? 1 : 0);
			}
			continue;
		}
		
		if (Row->MinActivationRange > 0.f &&
			DistanceSq < FMath::Square(Row->MinActivationRange))
		{
			if (bLogPatternSelection)
			{
				UE_LOG(LogRetrieveCombat, Warning,
					TEXT("[FindBestPattern] Skip min range. Owner=%s Row=%s Required=%s Dist3D=%.1f Dist2D=%.1f Min=%.1f Flying=%d"),
					*GetNameSafe(OwnerActor),
					*RowName.ToString(),
					*RequiredPatternType.ToString(),
					Distance3D,
					Distance2D,
					Row->MinActivationRange,
					bOwnerFlying ? 1 : 0);
			}
			continue;
		}
		
		if (!bIgnoreCooldown && !IsCooldownReady(RowName))
		{
			if (bLogPatternSelection)
			{
				const float* Expiry = CooldownExpiry.Find(RowName);
				const float Remaining = Expiry && GetWorld()
					? FMath::Max(0.f, *Expiry - GetWorld()->GetTimeSeconds())
					: -1.f;
				UE_LOG(LogRetrieveCombat, Warning,
					TEXT("[FindBestPattern] Skip cooldown. Owner=%s Row=%s Required=%s Remaining=%.2f"),
					*GetNameSafe(OwnerActor),
					*RowName.ToString(),
					*RequiredPatternType.ToString(),
					Remaining);
			}
			continue;
		}
		
		if (Row->Priority > BestPriority)
		{
			BestPriority = Row->Priority;
			BestRow = Row;
			BestRowName = RowName;
		}
	}
	
	if (OutRowName)
	{
		*OutRowName = BestRowName;
	}

	if (bLogPatternSelection && !BestRow)
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[FindBestPattern] No pattern selected. Owner=%s Required=%s Slots=%d Dist3D=%.1f Dist2D=%.1f Flying=%d"),
			*GetNameSafe(OwnerActor),
			*RequiredPatternType.ToString(),
			PatternSlots.Num(),
			Distance3D,
			Distance2D,
			bOwnerFlying ? 1 : 0);
	}
	
	return BestRow;
}

bool UEnemyCombatComponent::IsCooldownReady(FName RowName) const
{
	const float* Expiry = CooldownExpiry.Find(RowName);
	if (!Expiry)
	{
		return true;
	}
	return GetWorld()->GetTimeSeconds() >= *Expiry;
}

void UEnemyCombatComponent::StartCooldown(FName RowName, float Duration)
{
	CooldownExpiry.Add(RowName, GetWorld()->GetTimeSeconds() + Duration);
}

void UEnemyCombatComponent::LockSpecialAttackEvaluation(float Duration)
{
	if (Duration <= 0.f)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		SpecialAttackEvaluationLockUntilTime = World->GetTimeSeconds() + Duration;
	}
}

URetrieveAbilitySystemComponent* UEnemyCombatComponent::GetASC() const
{
	URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}
