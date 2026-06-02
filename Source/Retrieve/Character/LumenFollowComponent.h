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

/**
 * 루멘 동작: 호스트 폰 추적, 전투 시 후퇴, 대기/추적 명령 토글, 호스트가 너무 멀어지면 텔레포트, 소환, Idle 액션.
 * Mode는 Replicate되며 호스트만 수정할 수 있습니다. 컴포넌트 외부에서 Mode를 수정하지 말 것.
 */
UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API ULumenFollowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULumenFollowComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Tuning ----
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float FollowDistance = 300.f;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float ReengageDistance = 500.f;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float TeleportDistance = 2500.f;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float OffsetBack = 150.f;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float OffsetLeft = 80.f;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Follow")
	float MoveTargetRefreshThreshold = 50.f;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Combat")
	float RetreatRadius = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Idle")
	TArray<TObjectPtr<UAnimMontage>> IdleMontages;
	UPROPERTY(EditDefaultsOnly, Category = "Lumen|Idle")
	float IdleTriggerSeconds = 6.f;

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
	FVector ComputeFollowOffset(const APawn* Host) const;
	FVector ComputeRetreatPosition(const APawn* Host) const;
	void RequestMoveTo(const FVector& Target);
	void StopMove();

	void BindCombatTagWatcher();
	void OnCombatTagChanged(const FGameplayTag Tag, int32 Count);

	void HandleToggleWaitBroadcast(const FRetrieveLumenCommandPayload& Payload);
	void ApplyToggleWait();
	void HandleRecallBroadcast(const FRetrieveLumenCommandPayload& Payload);

	void TickIdle();
	void OnIdleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	float GetLocalHostIdleSeconds() const;

	EIdleMicroAction IdleAction = EIdleMicroAction::None;
	EFollowMode PreCombatMode = EFollowMode::Follow;
	FVector LastIssuedTarget = FVector::ZeroVector;

	FGameplayMessageListenerHandle ToggleWaitHandle;
	FGameplayMessageListenerHandle RecallHandle;
	FTimerHandle IdleTimerHandle;

	TWeakObjectPtr<UAbilitySystemComponent> BoundCombatASC;
	FDelegateHandle CombatTagHandle;
};
