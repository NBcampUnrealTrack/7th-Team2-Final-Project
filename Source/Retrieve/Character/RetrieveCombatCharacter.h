#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveCharacter.h"
#include "RetrieveCombatCharacter.generated.h"

/**
 * 전투 가능한 폰(소버린, 적, 보스)에 UHealthComponent를 추가하는 베이스 클래스.
 * Lumen은 비전투형 캐릭터이므로 ARetrieveCharacter를 직접 상속합니다.
 */
UCLASS()
class RETRIEVE_API ARetrieveCombatCharacter : public ARetrieveCharacter
{
	GENERATED_BODY()
	
public:
	ARetrieveCombatCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
protected:
	virtual void BeginPlay() override;
};
