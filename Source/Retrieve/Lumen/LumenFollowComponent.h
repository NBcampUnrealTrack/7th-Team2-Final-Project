#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "LumenFollowComponent.generated.h"

class ACharacter;
class APawn;
class UAbilitySystemComponent;
class UAnimMontage;

UENUM(BlueprintType)
enum class EIdleMicroAction : uint8
{
	None,
	Wander,
	LookAround,
	Stretch
};

class UEnvQuery;
struct FEnvQueryResult;

/**
 * 루멘 행동 상태 보유자. 결정은 ST_Lumen(호스트 전용 State Tree)이 내리고,
 * 이 컴포넌트는 실행(이동/몽타주/EQS 후퇴 쿼리)과 복제 상태(Mode)를 관리합니다.
 * Mode는 호스트 권한 — SetModeFromStateTree()로만 쓰며, 직접 수정을 금지합니다.
 */
UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API ULumenFollowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULumenFollowComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- State Tree 인터페이스 (호스트 전용; ST_Lumen Task/Evaluator가 호출) ----
	void SetModeFromStateTree(EFollowMode NewMode);
	EFollowMode GetMode() const { return Mode; }
	bool IsWaitRequested() const { return bWaitRequested; }

	UFUNCTION(BlueprintPure, Category = "Lumen|Combat")
	bool IsCowering() const { return Mode == EFollowMode::RetreatCombat; }

	// ---- Retire ----
	/* 호스트 전용. Retire 상태에서는 루멘이 제자리에 정지합니다. */
	void SetRetired(bool bInRetired);
	bool IsRetired() const { return bRetired; }

	// ---- EQS 안전지대 ----
	void RequestSafeSpotQuery();
	bool HasValidSafeSpot() const { return bSafeSpotValid; }
	FVector GetSafeSpot() const { return SafeSpot; }

	/** 호스트의 뒤쪽 + 왼쪽 배치 오프셋. Follow Task와 Recall GA가 공유합니다. */
	static FVector ComputeBehindLeftOffset(const AActor* Host, float InOffsetBack, float InOffsetLeft);

	/**
	 * 호스트 곁 착지 "월드 위치"를 지면에 투영해 반환합니다(텔레포트용).
	 * 호스트 Z를 그대로 쓰면 경사지에서 착지 XY의 지형이 더 높을 때 캡슐이 지형에 파묻히므로,
	 * 착지 XY의 실제 지면을 트레이스해 캡슐을 그 위에 앉힙니다. 지면이 없으면(미로딩)
	 * 호스트 높이 + 여유를 반환하고 이후는 지면 가드가 처리합니다.
	 */
	static FVector ComputeSafeLandingBehindHost(const AActor* Host, const ACharacter* LumenCharacter,
		float InOffsetBack, float InOffsetLeft);

	// Follow Tuning
	float GetFollowDistance() const { return FollowDistance; }
	float GetTeleportDistance() const { return TeleportDistance; }
	float GetOffsetBack() const { return OffsetBack; }
	float GetOffsetLeft() const { return OffsetLeft; }
	float GetMoveTargetRefreshThreshold() const { return MoveTargetRefreshThreshold; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- Tuning ----
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float FollowDistance = 300.f;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float TeleportDistance = 2500.f;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float OffsetBack = 150.f;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float OffsetLeft = 80.f;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float MoveTargetRefreshThreshold = 50.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Idle")
	TArray<TObjectPtr<UAnimMontage>> IdleMontages;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Idle")
	float IdleTriggerSeconds = 6.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Combat")
	TObjectPtr<UEnvQuery> SafeSpotQuery;

	// ---- Replicated state ----
	UPROPERTY(ReplicatedUsing = OnRep_Mode)
	EFollowMode Mode = EFollowMode::Follow;

	UFUNCTION()
	void OnRep_Mode();

	// ---- Idle (cosmetic; unreliable multicast) ----
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayIdleMontage(int32 Index);

private:
	APawn* ResolveHostPawn() const;

	void HandleToggleWaitBroadcast(const FRetrieveLumenCommandPayload& Payload);
	void ApplyToggleWait();
	void HandleRecallBroadcast(const FRetrieveLumenCommandPayload& Payload);
	void TickIdle();
	void OnIdleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	float GetLocalHostIdleSeconds() const;
	void OnSafeSpotQueryFinished(TSharedPtr<FEnvQueryResult> Result);
	
	EIdleMicroAction IdleAction = EIdleMicroAction::None;
	FGameplayMessageListenerHandle ToggleWaitHandle;
	FGameplayMessageListenerHandle RecallHandle;
	FTimerHandle IdleTimerHandle;
	FVector SafeSpot = FVector::ZeroVector;
	
	bool bWaitRequested = false;
	bool bSafeSpotValid = false;
	bool bRetired = false;
};
