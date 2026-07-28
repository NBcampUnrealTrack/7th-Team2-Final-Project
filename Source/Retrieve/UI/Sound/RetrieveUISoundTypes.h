#pragma once

#include "CoreMinimal.h"
#include "RetrieveUISoundTypes.generated.h"

UENUM(BlueprintType)
enum class ERetrieveUISoundEvent : uint8
{
	Hover       UMETA(DisplayName = "Hover"),
	Unhover     UMETA(DisplayName = "Unhover"),
	Press       UMETA(DisplayName = "Press"),
	Release     UMETA(DisplayName = "Release (Confirmed Click)"),
	PanelOpen   UMETA(DisplayName = "Panel Open"),
	PanelClose  UMETA(DisplayName = "Panel Close"),
	TabSwitch   UMETA(DisplayName = "Tab Switch"),
};
