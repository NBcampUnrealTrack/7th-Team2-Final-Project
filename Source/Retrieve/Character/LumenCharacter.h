#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveCharacter.h"
#include "LumenCharacter.generated.h"

/**
 * 비전투 동반자 NPC. ASC 및 HealthComponent를 갖지 않습니다. (DA_PawnData_Lumen.bRequiresAbilitySystem = false)
 * 호스트 Pawn을 따라갑니다. (ARetrieveGameState::GetHostPawn으로 결정됨)
 * 상호작용은 Ultimate Interaction Manager를 통해 BP_LumenCharacter에서 작성됩니다.
 */
UCLASS()
class RETRIEVE_API ALumenCharacter : public ARetrieveCharacter
{
	GENERATED_BODY()

public:
	ALumenCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
