#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "BarkSubsystem.generated.h"

class UBarkSpeakerComponent;
struct FBarkRow;
struct FRetrieveBarkPayload;
struct FRetrieveQuestStepPayload;
struct FRetrieveCinematicStatePayload;
struct FRetrieveDialogueChangedPayload;

/**
 * Bark 시스템의 중심. 누가, 언제 말할지를 정하는 월드 단위 관리자.
 * 다음 세 가지 일을 합니다:
 *   (1) 모든 스피커(UBarkSpeakerComponent)의 명단을 관리하고(등록/해제),
 *   (2) 주기적으로 근처의 한가한 스피커를 찾아 평상시 잡담을 시키고,
 *   (3) 세 가지 발동 경로(AmbientRandom / OnQuestStep / Manual)를 알맞은 대사로 연결합니다.
 *
 * UWorldSubsystem으로 둔 이유: 근접 판정 + 여러 액터 조율이 필요하기 때문.
 * 퀘스트 진행 상태는 절대 바꾸지 않습니다. CompletedSteps를 읽기만 합니다 (진행 권한은 호스트 소유).
 */
UCLASS()
class RETRIEVE_API UBarkSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ---- 스피커 명단 관리 (UBarkSpeakerComponent가 자기 BeginPlay/EndPlay에서 호출) ----
	void RegisterSpeaker(UBarkSpeakerComponent* Speaker);
	void UnregisterSpeaker(UBarkSpeakerComponent* Speaker);

	// ---- Manual 트리거 (볼륨 / 패널 훅 / 채널 리스너 / 치트 등 외부에서 호출) ----
	
	/** DT_Bark 행 이름으로 Manual 행을 직접 발동.
	 * 자격(Required/Forbidden/PlayOnce)은 검사하지만 Trigger 종류는 따지지 않습니다. 주로 치트 및 테스트용. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Bark")
	void RequestBarkById(FName RowName);

	/** KeyTag가 일치하는 첫 번째 (자격 있는) Manual 행을 발동합니다. 행 이름 대신 태그로 대사를 부를 때 씁니다.
	 *  e.g. ABarkTriggerVolume이 자기 BarkKeyTag로 호출 -> 해당 태그를 가진 Manual 행이 나옴. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Bark")
	void RequestBarkByKey(FGameplayTag KeyTag);

	// ---- UWorldSubsystem ----
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	void HandleStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message);
	void HandleCinematicChanged(FGameplayTag Channel, const FRetrieveCinematicStatePayload& Message);
	void HandleDialogueChanged(FGameplayTag Channel, const FRetrieveDialogueChangedPayload& Message);

	void ScanAmbient();

	bool IsRowEligible(FName RowName, const FBarkRow& Row) const;
	bool IsSpeakerInRange(const UBarkSpeakerComponent* Speaker) const;
	bool IsLocalPlayerInCombat() const;
	void FireRow(FName RowName, const FBarkRow& Row, UBarkSpeakerComponent* ViaSpeaker);
	FRetrieveBarkPayload BuildPayload(const FBarkRow& Row);
	FRetrieveBarkPayload BuildPayloadForLine(const FBarkRow& Row, const FText& Line) const;
	void BroadcastBarkLocal(const FRetrieveBarkPayload& Payload) const;

	/** OnQuestStep 대사를 (지연 후) 실제로 발동. bSequentialLines면 Lines를 순서대로 전부, 아니면 기존처럼 한 줄만 큐잉합니다. */
	void FireStepBark(FName RowName);

	const UDataTable* GetBarkTable() const;
	bool IsStepCompleted(FGameplayTag StepTag) const;

private:
	/** 등록된 모든 스피커 명단. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UBarkSpeakerComponent>> Speakers;

	/** 스피커별 다음 발화 쿨다운(초, World time). */
	TMap<TWeakObjectPtr<UBarkSpeakerComponent>, double> SpeakerReadyTime;

	/** 이미 나온 bPlayOnce 행들(행 이름), 한 번만 나와야 하는 대사의 재발동을 막습니다. 게임 재시작 시 초기화 됩니다. */
	TSet<FName> PlayedOnceRows;

	/** 직전에 발동한 행. 같은 대사가 연달아 나오지 않도록 후보에서 한 번 빼는 데 씁니다. */
	FName LastFiredRow = NAME_None;
	FString LastLine;

	double NextAmbientAllowedTime = 0.0;

	// 억제 상태: 시네마틱이나 대화 중에는 앰비언트를 생성하지 않습니다. (둘 중 하나라도 켜지면 억제)
	bool bCinematicActive = false;
	bool bDialogueActive = false;
	bool IsSuppressed() const { return bCinematicActive || bDialogueActive; }

	FGameplayMessageListenerHandle StepChangedHandle;
	FGameplayMessageListenerHandle CinematicHandle;
	FGameplayMessageListenerHandle DialogueHandle;

	FTimerHandle ScanTimerHandle;

	/** OnQuestStep 대사의 지연 발동용 타이머들 (Deinitialize에서 정리). */
	TArray<FTimerHandle> StepBarkTimers;

	/** 발화 후보를 찾는 스캔 주기 */
	float ScanInterval = 2.5f;
	/** 한 줄이 떠 있는 시간보다 훨씬 길게 잡아 "앞 자막이 사라진 뒤 다음 자막이 나오도록" 보장하세요. */
	float AmbientCooldown = 15.f;
};
