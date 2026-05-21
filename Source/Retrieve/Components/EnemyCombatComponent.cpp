#include "Components/EnemyCombatComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/PatternCounterComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Engine/DataTable.h"

void UEnemyCombatComponent::Initialize(UDataTable* InPatternTable, const TArray<FName>& InPatternSlots)
{
	PatternTable = InPatternTable;
	PatternSlots = InPatternSlots;
}

bool UEnemyCombatComponent::RequestPatternByPriority(AActor* Target)
{
	if (!Target)
	{
		return false;
	}

	const FMonsterPatternRow* BestPattern = FindBestPattern(Target);
	if (!BestPattern)
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
	ASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_Enemy_Attack, &EventData);

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

	ActivePatternRowName = NAME_None;
}

bool UEnemyCombatComponent::IsPatternActive() const
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	return ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Attack);
}

const FMonsterPatternRow* UEnemyCombatComponent::FindBestPattern(AActor* Target) const
{
	if (!PatternTable || PatternSlots.IsEmpty() || !GetOwner())
	{
		return nullptr;
	}

	const FVector PawnLocation = GetOwner()->GetActorLocation();
	const float DistanceSq     = FVector::DistSquared(PawnLocation, Target->GetActorLocation());

	const FMonsterPatternRow* BestRow      = nullptr;
	int32                     BestPriority = MIN_int32;
	FName                     BestRowName  = NAME_None;

	for (const FName& RowName : PatternSlots)
	{
		const FMonsterPatternRow* Row = PatternTable->FindRow<FMonsterPatternRow>(RowName, TEXT("UEnemyCombatComponent"));
		if (!Row)
		{
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
			BestRow      = Row;
			BestRowName  = RowName;
		}
	}

	// const_cast: FindBestPattern은 const지만 선택된 패턴 이름을 캐싱해야 함
	if (BestRow)
	{
		const_cast<UEnemyCombatComponent*>(this)->ActivePatternRowName = BestRowName;
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

URetrieveAbilitySystemComponent* UEnemyCombatComponent::GetASC() const
{
	URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}
