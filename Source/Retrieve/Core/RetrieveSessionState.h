#pragma once

#include "CoreMinimal.h"
#include "RetrieveSessionState.generated.h"

UENUM(BlueprintType)
enum class ERetrieveSessionState : uint8
{
	Loading UMETA(DisplayName = "Loading"),
	MainMenu UMETA(DisplayName = "Main Menu"),
	InGame UMETA(DisplayName = "In Game"),
	Result UMETA(DisplayName = "Result")
};
