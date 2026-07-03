#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Character/RetrieveCharacter.h"
#include "RetrieveNPCCharacter.generated.h"

class URetrieveDialogueComponent;

/**
 * 비전투 NPC(상점 등) 공용 베이스 클래스. ASC, HealthComponent, 전투/장비/카메라 컴포넌트를 갖지 않습니다.
 * 스켈레탈 메시는 GetMesh() 하나만 사용합니다. (ASovereignCharacter의 VisualMesh 이중 메시 구조 미포함)
 */
UCLASS()
class RETRIEVE_API ARetrieveNPCCharacter : public ARetrieveCharacter
{
	GENERATED_BODY()

public:
	ARetrieveNPCCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// TODO: RetrieveDialogueComponent로 이동 예정
	/** 대화 뷰에 표시될 이름입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	FText DisplayName;

	/** DT_Dialogue 행의 SpeakerTag와 일치합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	FGameplayTag SpeakerTag;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|NPC")
	TObjectPtr<URetrieveDialogueComponent> DialogueComponent;
};
