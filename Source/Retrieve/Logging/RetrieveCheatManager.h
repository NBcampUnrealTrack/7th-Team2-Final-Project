#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "RetrieveCheatManager.generated.h"

class UAbilitySystemComponent;
class ARetrieveGameState;
class UPatternCounterComponent;

/**
 * 
 */
UCLASS()
class RETRIEVE_API URetrieveCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveKillPlayer();

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveDamagePlayer(float Amount);

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveSetHealth(float Value);
	
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveTestGuardHit(bool bHeavy);

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveTestHitReact(int32 Strength);

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveQuestComplete(const FString& StepTagName);

	/** 바람 원소 해금 (ElementUnlockComponent.UnlockElement(Element.Wind)). WorldState에 영속 저장됨. */
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveUnlockWind();

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveLumenToggleWait();

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveLumenRecall();

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveToggleCombatTag();

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveOpenCounterWindow(float Duration = 0.f);

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveTryCounter();
	
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveTestCounter();

	/** 패리 없이 GA_ParryCounter 검증용 (CanCounter 태그를 임시 부여 후 발동) */
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveTestParryCounter();
	
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveBark(const FString& RowName);

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveBarkKey(const FString& KeyTagName);

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveCinematic(int32 bActive);

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveSystemMessage(const FString& SystemMessage);

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveSystemMessageById(const FString& RowName);

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveSystemMessageKey(const FString& KeyTagName);
	
	/** (원시 전송) bCompleted=0 -> "QUEST STARTED", !=0 -> "QUEST COMPLETED" */
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveQuestNotification(const FString& QuestName, int32 bCompleted);
	
	/** 새 게임 오프닝 타임라인을 즉시 재실행합니다(호스트 전용). */
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveReplayOpening();

private:
	UAbilitySystemComponent* GetLocalPlayerASC() const;
	UPatternCounterComponent* GetLockedOnPatternCounter() const;
	ARetrieveGameState* GetRetrieveGameState() const;
};
