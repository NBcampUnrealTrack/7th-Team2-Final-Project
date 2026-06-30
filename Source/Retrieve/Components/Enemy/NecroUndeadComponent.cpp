

#include "NecroUndeadComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UNecroUndeadComponent::UNecroUndeadComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNecroUndeadComponent::RegisterUndead(APawn* Undead)
{
	if (IsValid(Undead) == false)
	{
		return;
	}
	Undeads.Add(Undead);
}

int32 UNecroUndeadComponent::GetLiveUndeadCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<APawn>& WeakPawn : Undeads)
	{
		const APawn* Pawn = WeakPawn.Get();
		if (IsValid(Pawn) == false)
		{
			continue;
		}

		const URetrieveHealthComponent* Health = Pawn->FindComponentByClass<URetrieveHealthComponent>();
		if (IsValid(Health) == false || Health->IsDeadOrDying() == false)
		{
			++Count;
		}
	}
	return Count;
}

void UNecroUndeadComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (IsValid(Owner) == false)
	{
		return;
	}

	URetrieveHealthComponent* Health = Owner->FindComponentByClass<URetrieveHealthComponent>();
	if (IsValid(Health) == false)
	{
		return;
	}

	Health->OnDeathStarted.AddDynamic(this, &UNecroUndeadComponent::HandleOwnerDeathStarted);
}

void UNecroUndeadComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AActor* Owner = GetOwner();
	if (IsValid(Owner) == false)
	{
		Super::EndPlay(EndPlayReason);
		return;
	}

	URetrieveHealthComponent* Health = Owner->FindComponentByClass<URetrieveHealthComponent>();
	if (IsValid(Health) == false)
	{
		Super::EndPlay(EndPlayReason);
		return;
	}

	Health->OnDeathStarted.RemoveDynamic(this, &UNecroUndeadComponent::HandleOwnerDeathStarted);

	Super::EndPlay(EndPlayReason);
}

void UNecroUndeadComponent::HandleOwnerDeathStarted(AActor* OwningActor)
{
	if (bDeathHandled)
	{
		return;
	}

	const AActor* Owner = GetOwner();
	if (IsValid(Owner) == false || Owner->HasAuthority() == false)
	{
		return;
	}

	bDeathHandled = true;
	DetonateAllUndaeds();
}

void UNecroUndeadComponent::CompactUndeads()
{
	Undeads.RemoveAll([](const TWeakObjectPtr<APawn>& WeakPawn)
	{
		const APawn* Pawn = WeakPawn.Get();
		if (IsValid(Pawn) == false)
		{
			return true;
		}

		const URetrieveHealthComponent* Health = Pawn->FindComponentByClass<URetrieveHealthComponent>();
		return Health && Health->IsDeadOrDying();
	});
}

void UNecroUndeadComponent::DetonateAllUndaeds()
{
	CompactUndeads();

	// 처치자 확보
	AActor* Killer = nullptr;
	const AActor* Owner = GetOwner();
	const URetrieveHealthComponent* Health = IsValid(Owner) ? Owner->FindComponentByClass<URetrieveHealthComponent>() : nullptr;

	if (IsValid(Health))
	{
		Killer = Health->GetLastDamageInstigator();
		if (IsValid(Killer) == false)
		{
			Killer = Health->GetLastDamageCauser();
		}
	}

	// 플레이어가 처치한게 아니라면 제외
	const APawn* KillerPawn = Cast<APawn>(Killer);
	const AController* KillerController = Cast<AController>(Killer);
	const bool bKillerIsPlayer = (IsValid(KillerPawn) && KillerPawn->IsPlayerControlled()) || 
		(IsValid(KillerController) && KillerController->IsPlayerController());

	if (bKillerIsPlayer == false)
	{
		Killer = nullptr;
	}

	for (const TWeakObjectPtr<APawn>& WeakPawn : Undeads)
	{
		APawn* Undead = WeakPawn.Get();
		if (IsValid(Undead) == false)
		{
			continue;
		}

		FGameplayEventData EventData;
		EventData.Instigator = Killer;
		EventData.Target = Undead;

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Undead,
			RetrieveGameplayTags::GameplayEvent_Enemy_SelfDestruct,
			EventData);
	}
	Undeads.Reset();
}
