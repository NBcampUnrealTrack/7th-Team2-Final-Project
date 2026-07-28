#include "Quest/RetrieveQuestLinkComponent.h"

URetrieveQuestLinkComponent::URetrieveQuestLinkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FName URetrieveQuestLinkComponent::ResolveRoleName(FName NpcRoleName) const
{
	switch (Role)
	{
	case ERetrieveQuestLinkRole::NPC:
		return NpcRoleName.IsNone() ? FName(TEXT("NPC")) : NpcRoleName;
	case ERetrieveQuestLinkRole::QuestItem:
		return *FString::Printf(TEXT("QuestItem_%d"), FMath::Max(0, RoleIndex));
	case ERetrieveQuestLinkRole::Destination:
		return *FString::Printf(TEXT("Destination_%d"), FMath::Max(0, RoleIndex));
	case ERetrieveQuestLinkRole::Merchant:
		return FName(TEXT("Merchant"));
	case ERetrieveQuestLinkRole::Custom:
		return CustomRole;
	default:
		return NAME_None;
	}
}
