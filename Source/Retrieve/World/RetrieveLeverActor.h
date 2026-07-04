#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RetrieveLeverActor.generated.h"

class URetrieveInteractionResponseComponent;
class ARetrieveLeverActor;

/** 레버 상태가 바뀔 때 발동. 조건 문(RetrieveLeverDoor)이 바인딩한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRetrieveOnLeverChanged, ARetrieveLeverActor*, Lever);

/**
 * 상호작용으로 켜고/끄는 레버·버튼. 조건 문이 이 상태(bActivated)를 감시한다.
 *
 * ─ BP 세팅 (InteractDoor와 동일 패턴) ────────────────────────────────────────
 *  1. 이 클래스로 BP 생성 + 레버/버튼 메시를 SceneRoot에 부착.
 *  2. 플러그인 Manager_InteractionTarget 컴포넌트 부착 → 변수명 "InteractionTarget".
 *  3. InteractionResponse에 TypeAsset 지정 + FinishMethod 재사용(반복 상호작용).
 *  4. OnLeverStateChangedBP 이벤트에 레버 당김/버튼 발광 연출.
 */
UCLASS(Blueprintable)
class RETRIEVE_API ARetrieveLeverActor : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveLeverActor();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Lever")
	bool IsActivated() const { return bActivated; }

	/** 레버 상태 변화 시 (호스트 토글 또는 클라 OnRep). 문이 바인딩. */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Lever")
	FRetrieveOnLeverChanged OnLeverChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	/** InteractionResponse.OnApplied 바인딩 — 상호작용 시 레버 토글. */
	UFUNCTION()
	void HandleInteracted(AActor* InteractionInstigator);

	UFUNCTION()
	void OnRep_bActivated();

	/** 상태를 비주얼·델리게이트에 반영. */
	void ApplyLeverState(bool bInstant);

	/** 상태 변화 비주얼 (레버 당김/버튼 발광). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Lever")
	void OnLeverStateChangedBP(bool bNowActivated, bool bInstant);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Lever")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Lever")
	TObjectPtr<URetrieveInteractionResponseComponent> InteractionResponse;

	/** true면 토글(켜고 끄기 가능). false면 한 번 켜지면 유지(래치 버튼). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Lever")
	bool bToggle = true;

	/** 시작 시 활성 상태로 둘지. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Lever")
	bool bStartActivated = false;

	UPROPERTY(ReplicatedUsing = OnRep_bActivated, BlueprintReadOnly, Category = "Retrieve|Lever")
	bool bActivated = false;
};
