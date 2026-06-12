#include "Components/EnemyCombatComponent.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AIController.h"
#include "Components/SphereComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/PatternCounterComponent.h"
#include "Combat/RetrieveCombatTypes.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

void UEnemyCombatComponent::Initialize(UDataTable* InPatternTable, const TArray<FName>& InPatternSlots)
{
	PatternTable = InPatternTable;
	PatternSlots = InPatternSlots;
}

bool UEnemyCombatComponent::RequestPatternByPriority(AActor* Target, FGameplayTag RequiredPatternType)
{
	if (!Target)
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
	
	UE_LOG(LogRetrieveCombat, Display,
		TEXT("[%s] RequestPatternByPriority Target=%s"),
		*GetOwner()->GetName(),
		*GetNameSafe(Target));
	
	FName BestPatternRowName = NAME_None;
	const FMonsterPatternRow* BestPattern = FindBestPattern(Target, RequiredPatternType, &BestPatternRowName);
	if (!BestPattern)
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[%s] No SpecialAttack pattern found. Target=%s"),
			*GetOwner()->GetName(),
			*GetNameSafe(Target));
		
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
	
	if (UPatternCounterComponent* PatternCounter = GetOwner()->FindComponentByClass<UPatternCounterComponent>())
	{
		PatternCounter->SetActivePatternRow(ActivePatternRowName, PatternTable.Get());
	}

	FGameplayEventData EventData;
	EventData.OptionalObject = BestPattern->AttackMontage.LoadSynchronous();
	
	const FGameplayTag AbilityEventTag = BestPattern->AbilityEventTag.IsValid()
	? BestPattern->AbilityEventTag : DefaultEventTag;
	EventData.EventTag = AbilityEventTag;
	EventData.Target = Target;
	EventData.Instigator = GetOwner();
	
	const int32 TriggeredCount = ASC->HandleGameplayEvent(AbilityEventTag, &EventData);
	
	UE_LOG(LogRetrieveCombat, Display,
		TEXT("[%s] SpecialAttack Event TriggeredCount=%d Row=%s EventTag=%s Montage=%s Target=%s"),
		*GetOwner()->GetName(),
		TriggeredCount,
		*BestPatternRowName.ToString(),
		*AbilityEventTag.ToString(),
		*GetNameSafe(EventData.OptionalObject.Get()),
		*GetNameSafe(Target));
	
	if (TriggeredCount <= 0)
	{
		UE_LOG(LogRetrieveCombat, Warning,
		TEXT("[%s] SpecialAttack event did not trigger ability. Row=%s"),
		*GetOwner()->GetName(),
		*BestPatternRowName.ToString());
		ActivePatternRowName = NAME_None;
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
	return FindBestPattern(Target, PatternType) != nullptr;
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
	ActivePatternRowName = NAME_None;
}

bool UEnemyCombatComponent::IsPatternActive() const
{
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
		MovementLockOriginalMaxWalkSpeed = MoveComp->MaxWalkSpeed;
		MoveComp->StopMovementImmediately();
		MoveComp->MaxWalkSpeed = 0.f;
	}
	else if (MovementLockOriginalMaxWalkSpeed >= 0.f)
	{
		MoveComp->MaxWalkSpeed = MovementLockOriginalMaxWalkSpeed;
		MovementLockOriginalMaxWalkSpeed = -1.f;
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
	
	if (Row->HitboxBoneName.IsNone())
	{
		ActiveHitboxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}
	
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		if (!ActiveHitboxComp->AttachToComponent(
			OwnerChar->GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			Row->HitboxBoneName))
		{
			UE_LOG(LogSkeletalMesh, Error, TEXT("[%s] HitBox attachment failed"), *GetName());
		}
	}
	
	ActiveHitboxComp->SetSphereRadius(Row->HitboxRadius);
	ActiveHitboxComp->SetRelativeLocation(Row->HitboxOffset);
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
	
	const IAbilitySystemInterface* TargetASCActor = 
		Cast<IAbilitySystemInterface>(OtherActor);

	if (!TargetASCActor)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}
	
	const AAIController* OwnerController =
		Cast<AAIController>(OwnerPawn->GetController());

	if (!OwnerController ||
		OwnerController->GetTeamAttitudeTowards(*OtherActor) != ETeamAttitude::Hostile)
	{
		return;
	}
	
	if (HitActors.Contains(OtherActor))
	{
		return;
	}
	
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	
	IAbilitySystemInterface* TargetIF = Cast<IAbilitySystemInterface>(OtherActor);
	UAbilitySystemComponent* TargetASC = TargetIF ? TargetIF->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return;
	}
	
	URetrieveAbilitySystemComponent* SourceASC = GetASC();
	if (!SourceASC || !DamageEffectClass)
	{
		return;
	}
	
	HitActors.Add(OtherActor);

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(GetOwner(), GetOwner());
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
		DamageEffectClass, 1.f, Context);

	if (Spec.IsValid())
	{
		if (PatternTable && !ActivePatternRowName.IsNone())
		{
			if (const FMonsterPatternRow* ReactRow =
				PatternTable->FindRow<FMonsterPatternRow>(ActivePatternRowName, TEXT("OnHitboxOverlap_HitReact")))
			{
				if (const FGameplayTag ReactTag = HitReactTypeToTag(ReactRow->HitReactType); ReactTag.IsValid())
				{
					Spec.Data->AddDynamicAssetTag(ReactTag);
				}
			}
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	
	UE_LOG(LogDamage, Display, TEXT("[%s] Hit "), *GetName())
}

const FMonsterPatternRow* UEnemyCombatComponent::FindBestPattern(AActor* Target, FGameplayTag RequiredPatternType, FName* OutRowName) const
{
	if (!PatternTable || PatternSlots.IsEmpty() || !GetOwner())
	{
		return nullptr;
	}

	const FVector PawnLocation = GetOwner()->GetActorLocation();
	const float DistanceSq = FVector::DistSquared(PawnLocation, Target->GetActorLocation());

	const FMonsterPatternRow* BestRow = nullptr;
	FName BestRowName = NAME_None;
	int32 BestPriority = MIN_int32;

	for (const FName& RowName : PatternSlots)
	{
		const FMonsterPatternRow* Row = PatternTable->FindRow<FMonsterPatternRow>(RowName, TEXT("UEnemyCombatComponent"));
		if (!Row)
		{
			UE_LOG(LogTemp,Error, TEXT("[%s] Can't find patternRow. RowName : %s"), *GetName(), *RowName.ToString());
			continue;
		}
		
		if (RequiredPatternType.IsValid() &&
			!Row->PatternType.MatchesTagExact(RequiredPatternType))
		{
			continue;
		}
		
		if (DistanceSq > FMath::Square(Row->MaxActivationRange))
		{
			continue;
		}
		
		if (Row->MinActivationRange > 0.f &&
			DistanceSq < FMath::Square(Row->MinActivationRange))
		{
			continue;
		}
		
		if (!IsCooldownReady(RowName))
		{
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
