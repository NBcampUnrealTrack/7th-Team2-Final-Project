#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RetrieveDoorBase.generated.h"

/**
 * 레벨에 배치하는 문의 공용 베이스.
 *
 * ─ 콜리전 ─────────────────────────────────────────────────────────────────
 *  문 통행 차단은 "문짝 메시"가 담당한다(별도 콜리전 없음).
 *   · 물리적으로 열리는 문(스윙/슬라이드): 열리면 메시가 비켜나며 통로가 뚫림 → 추가 처리 불필요.
 *   · 안 움직이고 통과만 시키는 문(마법 문 등): BP의 OnDoorOpened/Closed에서
 *     문짝 메시 콜리전을 SetCollisionEnabled로 토글.
 */
UCLASS(Abstract)
class RETRIEVE_API ARetrieveDoorBase : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveDoorBase();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Door")
	bool IsOpen() const { return bOpen; }

	/** 문 열기 (권한에서만 유효, 이미 열려있으면 무시). */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Door")
	void OpenDoor();

	/** 문 닫기. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Door")
	void CloseDoor();

	/** 열림/닫힘 토글. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Door")
	void ToggleDoor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	/** bOpen 상태를 비주얼(BP 이벤트)에 반영. bInstant면 애니메이션 없이 즉시. */
	void ApplyDoorState(bool bInstant);

	UFUNCTION()
	void OnRep_bOpen();

	/** 문이 열릴 때 — BP에서 문짝 이동/애님/VFX/사운드 처리. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Door")
	void OnDoorOpened(bool bInstant);

	/** 문이 닫힐 때 — BP에서 문짝 이동/애님/VFX/사운드 처리. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Door")
	void OnDoorClosed(bool bInstant);

	/** 문짝 메시를 붙이는 루트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Door")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 시작 시 열린 상태로 둘지. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Door")
	bool bStartOpen = false;

	/** 열림 상태 (권한이 바꾸고 복제 → 클라 OnRep으로 비주얼 반영). */
	UPROPERTY(ReplicatedUsing = OnRep_bOpen, BlueprintReadOnly, Category = "Retrieve|Door")
	bool bOpen = false;
};
