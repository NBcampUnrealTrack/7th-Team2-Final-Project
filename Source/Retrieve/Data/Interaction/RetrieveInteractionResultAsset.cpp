#include "Data/Interaction/RetrieveInteractionResultAsset.h"

void URetrieveInteractionResultAsset::ApplyResult_Implementation(
	UObject* /*WorldContextObject*/,
	AActor* /*Instigator*/,
	AActor* /*InteractedActor*/) const
{
	// 자식이 override. 베이스는 no-op.
}
