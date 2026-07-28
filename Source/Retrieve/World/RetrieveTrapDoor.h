#pragma once

#include "CoreMinimal.h"
#include "World/RetrieveDoorBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "RetrieveTrapDoor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class URetrieveInteractionResponseComponent;
struct FMonsterDiedPayload;

/**
 * 트랩방 문.
 *
 * 평소: 일반 상호작용 문처럼 열고/닫을 수 있다(플레이어가 함정임을 눈치채지 못하게).
 * 트리거: 플레이어가 RoomVolume에 진입 + 구역 내 몬스터가 있으면
 *   → 문이 닫히고(이미 닫혔으면 유지) 상호작용이 잠긴다(열리지 않음) + OnTrapArmed 발동.
 * 해제: 구역 몬스터를 전부 처치하면 → 잠금 해제 + 문 개방 + OnTrapCleared 발동.
 *
 * 상호작용 프롬프트까지 완전히 숨기려면 OnTrapArmed에서 Manager_InteractionTarget을
 * 비활성화하고 OnTrapCleared에서 복구하면 된다(BP).
 *
 * ─ BP/레벨 세팅 ──────────────────────────────────────────────────────────────
 *  1. 문짝 메시(닫혔을 때 Pawn Block) + 플러그인 Manager_InteractionTarget("InteractionTarget").
 *  2. InteractionResponse에 문용 TypeAsset + FinishMethod 재사용(평소 열고/닫기).
 *  3. RoomVolume 박스를 방 크기로 조정. MonsterClass 지정(기본 RetrieveEnemyCharacter).
 *  4. OnDoorOpened/Closed(문짝 연출), OnTrapArmed/Cleared(잠금 연출·프롬프트 제어).
 *  ※ 진입 순간 구역에 몬스터가 있어야 트랩 발동(빈 방이면 안 닫힘).
 */
UCLASS(Blueprintable)
class RETRIEVE_API ARetrieveTrapDoor : public ARetrieveDoorBase
{
	GENERATED_BODY()

public:
	ARetrieveTrapDoor();

	/** 트랩이 무장되어 상호작용으로 열 수 없는 상태인지. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Door|Trap")
	bool IsLocked() const { return bArmed && !bCleared; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** InteractionResponse.OnApplied — 잠기지 않았을 때만 문 토글. */
	UFUNCTION()
	void HandleInteracted(AActor* InteractionInstigator);

	/** RoomVolume 진입 — 플레이어면 트랩 무장. */
	UFUNCTION()
	void OnRoomBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 구역 내 몬스터를 스냅샷하고 문을 닫고 잠근다(몬스터 없으면 무시). */
	void ArmTrap();

	/** Channel.Monster.Died 수신 — 스냅샷에서 제거, 전멸 시 해제. */
	void OnMonsterDied(FGameplayTag Channel, const FMonsterDiedPayload& Payload);

	bool IsMonster(const AActor* Actor) const;

	/** 트랩 발동 순간(문 닫힘·잠김). BP에서 프롬프트 숨김/경보 사운드/조명 등. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Door|Trap")
	void OnTrapArmed();

	/** 트랩 해제 순간(몬스터 전멸). BP에서 프롬프트 복구/개방 연출 등. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Door|Trap")
	void OnTrapCleared();

	/** 잠긴 상태에서 상호작용 시도(문은 안 열림). "덜컹" 피드백 등에 사용. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Door|Trap")
	void OnLockedInteract(AActor* InteractionInstigator);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Door|Trap")
	TObjectPtr<URetrieveInteractionResponseComponent> InteractionResponse;

	/** 플레이어 진입 + 몬스터 감지 볼륨. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Door|Trap")
	TObjectPtr<UBoxComponent> RoomVolume;

	/** 몬스터로 취급할 클래스 필터. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Door|Trap")
	TSubclassOf<AActor> MonsterClass;

private:
	/** 무장 시점에 구역에 있던(살아있는) 몬스터들. */
	TSet<TWeakObjectPtr<AActor>> LiveMonsters;

	bool bArmed = false;
	bool bCleared = false;

	FGameplayMessageListenerHandle DiedHandle;
};
