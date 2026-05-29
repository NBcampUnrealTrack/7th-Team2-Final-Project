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
	if (!Target) return false;

	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return false;

	// 기본 공격 Row 유효성 확인
	if (!PatternTable || BasicAttackRowName.IsNone()) return false;
	const FMonsterPatternRow* Row =
		PatternTable->FindRow<FMonsterPatternRow>(BasicAttackRowName, TEXT(""));
	if (!Row || Row->HitboxBoneName.IsNone()) return false;

	ActivePatternRowName = BasicAttackRowName;

	FGameplayEventData EventData;
	EventData.EventTag   = RetrieveGameplayTags::GameplayEvent_Enemy_Attack;
	EventData.Target     = Target;
	EventData.Instigator = GetOwner();

	const int32 TriggeredCount =
		ASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_Enemy_Attack, &EventData);

	return TriggeredCount > 0;
}

bool UEnemyCombatComponent::RequestPatternByPriority(AActor* Target)
{
	if (!Target)
	{
		return false;
	}

	const FMonsterPatternRow* BestPattern = FindBestPattern(Target);
	if (!BestPattern || BestPattern->HitboxBoneName == NAME_None)
	{
		return false;
	}

	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return false;
	}
	
	if (UPatternCounterComponent* PatternCounter = GetOwner()->FindComponentByClass<UPatternCounterComponent>())
	{
		PatternCounter->SetActivePatternRow(ActivePatternRowName, PatternTable.Get());
	}

	FGameplayEventData EventData;
	EventData.EventTag = RetrieveGameplayTags::GameplayEvent_Enemy_Attack;
	EventData.Target   = Target;
	EventData.Instigator = GetOwner();
	
	const int32 TriggeredCount = ASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_Enemy_SpecialAttack, &EventData);

	if (TriggeredCount <= 0)
	{
		return false;
	}
	
	StartCooldown(ActivePatternRowName, BestPattern->Cooldown);
	return true;
}

void UEnemyCombatComponent::StopCurrentPattern()
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	FGameplayTagContainer TagsToCancel(RetrieveGameplayTags::Ability_Enemy_Attack);
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
	return ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Attack);
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

	if (Row && !Row->HitboxBoneName.IsNone())
	{
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
	}
	else if (Row == nullptr)
	{
		UE_LOG(LogDataTable, Error, TEXT("[%s] Row is inValid"), *GetName());
	}
	else if (Row->HitboxBoneName.IsNone())
	{
		UE_LOG(LogDataTable, Error, TEXT("[%s] Bone name is none"), *GetName());
	}
	
	HitActors.Empty();
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

const FMonsterPatternRow* UEnemyCombatComponent::FindBestPattern(AActor* Target)
{
	if (!PatternTable || PatternSlots.IsEmpty() || !GetOwner())
	{
		return nullptr;
	}

	const FVector PawnLocation = GetOwner()->GetActorLocation();
	const float DistanceSq     = FVector::DistSquared(PawnLocation, Target->GetActorLocation());

	const FMonsterPatternRow* BestRow = nullptr;
	int32 BestPriority = MIN_int32;

	for (const FName& RowName : PatternSlots)
	{
		const FMonsterPatternRow* Row = PatternTable->FindRow<FMonsterPatternRow>(RowName, TEXT("UEnemyCombatComponent"));
		if (!Row)
		{
			UE_LOG(LogTemp,Error, TEXT("[%s] Can't find patternRow. RowName : %s"), *GetName(), *RowName.ToString());
			continue;
		}
		
		if (DistanceSq > FMath::Square(Row->ActivationRange))
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
			ActivePatternRowName = RowName;
			BestRow = Row;
		}
	}
	
	UE_LOG(LogDataTable, Display, TEXT("[%s] Set ActivePatternRowName : %s"), *GetName(), *ActivePatternRowName.ToString());
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
