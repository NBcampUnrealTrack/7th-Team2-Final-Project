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
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void UEnemyCombatComponent::Initialize(UDataTable* InPatternTable, const TArray<FName>& InPatternSlots)
{
	PatternTable = InPatternTable;
	PatternSlots = InPatternSlots;
	if (!PatternSlots.IsEmpty())
	{
		BasicAttackRowName = PatternSlots[0];
	}
}

bool UEnemyCombatComponent::RequestBasicAttack(AActor* Target)
{
	if (!Target)
	{
		UE_LOG(LogRetrieveCombat, Error, TEXT("[%s] No Target."), *GetOwner()->GetName());
		return false;
	}
	
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		UE_LOG(LogRetrieveCombat, Error, TEXT("[%s] No ASC."), *GetOwner()->GetName());
		return false;
	}
	
	// 기본 공격 Row 유효성 확인
	if (!PatternTable || BasicAttackRowName.IsNone())
	{
		UE_LOG(LogRetrieveCombat, Warning, TEXT("[%s] No PatternTAble or No BasicAttackRowName. BasicAttackRowName = %s")
			,*GetOwner()->GetName(), *BasicAttackRowName.ToString());
		return false;
	}
	
	const FMonsterPatternRow* Row =
		PatternTable->FindRow<FMonsterPatternRow>(BasicAttackRowName, TEXT(""));
	if (!Row || Row->HitboxBoneName.IsNone())
	{
		UE_LOG(LogRetrieveCombat, Warning, TEXT("[%s] Basic Attack Row not found."),*GetOwner()->GetName());
		return false;
	}
	
	if (!IsCooldownReady(BasicAttackRowName))
	{
		UE_LOG(LogRetrieveCombat, Warning, TEXT("[%s] Basic Attack is Now CoolDown."),*GetOwner()->GetName());
		return false;
	}
	
	ActivePatternRowName = BasicAttackRowName;

	FGameplayEventData EventData;
	EventData.EventTag= RetrieveGameplayTags::GameplayEvent_Enemy_Attack;
	EventData.Target = Target;
	EventData.Instigator = GetOwner();
	EventData.OptionalObject = Row->AttackMontage.LoadSynchronous();

	const int32 TriggeredCount =
		ASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_Enemy_Attack, &EventData);

	
	if (TriggeredCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] No Ability triggered."),*GetOwner()->GetName());
		return false;
	}
	
	StartCooldown(ActivePatternRowName, Row->Cooldown);
	return true;
}

bool UEnemyCombatComponent::RequestPatternByPriority(AActor* Target)
{
	if (!Target)
	{
		return false;
	}

	FName BestPatternRowName = NAME_None;
	const FMonsterPatternRow* BestPattern = FindBestPattern(Target, RetrieveGameplayTags::Ability_Enemy_SpecialAttack, &BestPatternRowName);
	if (!BestPattern)
	{
		UE_LOG(LogRetrieveCombat, Warning,
			TEXT("[%s] No SpecialAttack pattern found. Target=%s"),
			*GetOwner()->GetName(),
			*GetNameSafe(Target));
		
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
	EventData.EventTag = RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack;
	EventData.Target = Target;
	EventData.Instigator = GetOwner();
	
	const int32 TriggeredCount = ASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack, &EventData);
	
	UE_LOG(LogRetrieveCombat, Display,
		TEXT("[%s] SpecialAttack Event TriggeredCount=%d Row=%s Montage=%s Target=%s"),
		*GetOwner()->GetName(),
		TriggeredCount,
		*BestPatternRowName.ToString(),
		*GetNameSafe(EventData.OptionalObject.Get()),
		*GetNameSafe(Target));
	
	if (TriggeredCount <= 0)
	{
		UE_LOG(LogRetrieveCombat, Warning,
		TEXT("[%s] SpecialAttack event did not trigger ability. Row=%s"),
		*GetOwner()->GetName(),
		*BestPatternRowName.ToString());
		return false;
	}
	
	StartCooldown(ActivePatternRowName, BestPattern->Cooldown);
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

bool UEnemyCombatComponent::IsAttackable() const
{
	return IsCooldownReady(BasicAttackRowName);
}

void UEnemyCombatComponent::ActivateHitbox()
{
	if (!ActiveHitboxComp)
	{
		return;
	}
	
	const FMonsterPatternRow* Row = PatternTable
		? PatternTable->FindRow<FMonsterPatternRow>(ActivePatternRowName == NAME_None 
			? BasicAttackRowName : ActivePatternRowName,
			TEXT(""))
		: nullptr;

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
	
	UE_LOG(LogDataTable, Display,
		TEXT("[%s] FindBestPattern Type=%s Result=%s Distance=%.1f"),
		*GetName(), *RequiredPatternType.ToString(),
		*BestRowName.ToString(), FMath::Sqrt(DistanceSq));
	
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

URetrieveAbilitySystemComponent* UEnemyCombatComponent::GetASC() const
{
	URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}
