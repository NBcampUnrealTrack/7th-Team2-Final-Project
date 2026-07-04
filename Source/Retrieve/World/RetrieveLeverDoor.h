#pragma once

#include "CoreMinimal.h"
#include "World/RetrieveDoorBase.h"
#include "RetrieveLeverDoor.generated.h"

class ARetrieveLeverActor;

UENUM(BlueprintType)
enum class ERetrieveLeverDoorCondition : uint8
{
	AllActivated	UMETA(DisplayName = "모든 레버 ON"),
	Pattern			UMETA(DisplayName = "지정 패턴 일치"),
};

/**
 * 레버/버튼 조합이 조건을 만족하면 열리는 문.
 *
 * ─ BP/레벨 세팅 ──────────────────────────────────────────────────────────────
 *  1. 레벨에 RetrieveLeverActor 여러 개 배치.
 *  2. 이 문의 Levers 배열에 그 레버 액터들을 지정.
 *  3. Condition 선택:
 *     · 모든 레버 ON  — 전부 활성화되면 개방.
 *     · 지정 패턴 일치 — RequiredStates(레버와 같은 순서)와 각 레버 상태가 일치하면 개방.
 *  4. bRecloseWhenUnmet: 조건이 깨지면 다시 닫을지 (false면 한 번 열리면 유지).
 */
UCLASS(Blueprintable)
class RETRIEVE_API ARetrieveLeverDoor : public ARetrieveDoorBase
{
	GENERATED_BODY()

public:
	ARetrieveLeverDoor();

protected:
	virtual void BeginPlay() override;

	/** 레버 상태 변화 수신 → 조건 평가. */
	UFUNCTION()
	void HandleLeverChanged(ARetrieveLeverActor* Lever);

	/** 현재 레버 조합이 조건을 만족하는지. BP에서 조회 가능. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Door|Lever")
	bool IsConditionMet() const;

	/**
	 * 레버가 바뀔 때마다 조건 결과를 BP에 알린다 (문 개폐와 별개).
	 * 성공(bMet=true) 순간 사운드/VFX/연출 등에 사용. 문 여닫힘 자체는 C++가 자동 처리.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Door|Lever")
	void OnConditionEvaluated(bool bMet);

	/** 감시할 레버들 (레벨에 배치한 액터 지정). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Retrieve|Door|Lever")
	TArray<TObjectPtr<ARetrieveLeverActor>> Levers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Door|Lever")
	ERetrieveLeverDoorCondition Condition = ERetrieveLeverDoorCondition::AllActivated;

	/** Condition=Pattern일 때 각 레버의 목표 상태 (Levers와 같은 순서·길이). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Door|Lever",
		meta = (EditCondition = "Condition == ERetrieveLeverDoorCondition::Pattern"))
	TArray<bool> RequiredStates;

	/** 조건이 깨지면 다시 닫을지 (false면 한 번 열리면 유지). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Door|Lever")
	bool bRecloseWhenUnmet = false;
};
