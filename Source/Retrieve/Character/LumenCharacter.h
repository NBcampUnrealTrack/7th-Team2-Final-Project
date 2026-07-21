#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "Character/RetrieveCharacter.h"
#include "LumenCharacter.generated.h"

class ULumenFollowComponent;
class URetrieveDialogueComponent;
struct FRetrieveQuestStepPayload;

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

	// TODO: RetrtieveDialogueComponent로 이동 예정
	/** 대화 뷰에 표시될 이름입니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	FText DisplayName;

	/** DT_Dialogue 행의 SpeakerTag(Speaker.Lumen)와 일치합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	FGameplayTag SpeakerTag;

	/** 뷰가 열릴 때 토픽 목록이 나타나기 전에 한 번에 하나씩 재생되는 인사말 라인. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Dialogue")
	TArray<FText> DefaultGreetingLines;

	void SetRetired(bool bInRetired, const FTransform* ParkXf = nullptr);
	bool IsRetired() const { return bRetired; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleQuestStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message);
	void SyncRetiredFromQuestLedger();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Lumen")
	TObjectPtr<ULumenFollowComponent> FollowComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Lumen")
	TObjectPtr<URetrieveDialogueComponent> DialogueComponent;

	FGameplayMessageListenerHandle StepChangedListener;

	// TODO(coop): 호스트 로컬 상태. 추후 복제 필요.
	bool bRetired = false;
};
