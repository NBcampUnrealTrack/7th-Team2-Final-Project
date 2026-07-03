#include "Combat/RetrieveAttackHitLibrary.h"

#include "Interface/RetrieveAttackHitReceiver.h"

bool URetrieveAttackHitLibrary::TryNotifyAttackHitReceiver(
	AActor* Target,
	AActor* Attacker,
	const FHitResult& HitResult,
	FGameplayTag AttackTypeTag,
	FGameplayTag ElementTag)
{
	if (!IsValid(Target) || !IsValid(Attacker))
	{
		return false;
	}

	if (!Target->GetClass()->ImplementsInterface(URetrieveAttackHitReceiver::StaticClass()))
	{
		return false;
	}

	return IRetrieveAttackHitReceiver::Execute_ReceiveRetrieveAttackHit(Target, Attacker, HitResult, AttackTypeTag, ElementTag);
}
