#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "LumenFollowComponent.generated.h"

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
	
	// ---- EQS 안전지대 ----
	void RequestSafeSpotQuery();
	bool HasValidSafeSpot() const { return bSafeSpotValid; }
	FVector GetSafeSpot() const { return SafeSpot; }

	/** 호스트의 뒤쪽 + 왼쪽 배치 오프셋. Follow Task와 Recall GA가 공유합니다. */
	static FVector ComputeBehindLeftOffset(const AActor* Host, float InOffsetBack, float InOffsetLeft);

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
};
