#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EncirclementSubsystem.generated.h"

/**
 * 타깃 주위 링에 공격 슬롯을 분배한다. 적이 한 점에 몰리지 않도록 분산.
 * 적은 RequestSlot으로 슬롯을 받아 그 좌표로 이동(공격 판정은 타깃에).
 */
UCLASS()
class RETRIEVE_API UEncirclementSubsystem : public UTickableWorldSubsystem   
{
	GENERATED_BODY()

public:
	/** 요청자의 접근 방향에서 가장 가까운 빈 슬롯을 예약해 인덱스 반환. 꽉 차면 INDEX_NONE. */
	int32 RequestSlot(AActor* Target, AActor* Requester);

	/** Requester가 점유한 슬롯 반납. */
	void ReleaseSlot(AActor* Target, AActor* Requester);

	/** 공격 진입 전 토큰 요청 */
	UFUNCTION(BlueprintCallable, Category = "Attack Token")
	bool RequestAttackToken(AActor* Target, AActor* Requester);
	
	bool HasAttackToken(AActor* Target, AActor* Requester) const;
	
	UFUNCTION(BlueprintPure, Category = "Attack Token")
	bool CanRequestAttackToken(AActor* Target, AActor* Requester) const;
	
	/** 플레이어가 Deadband를 넘어설 때만 대기자 링 앵커를 플레이어 위치로 재고정한다.*/
	FVector GetOrUpdateRingAnchor(AActor* Target);

	/** 공격 완료 또는 탈출 시 토큰 반납 */
	UFUNCTION(BlueprintCallable, Category = "Attack Token")
	void ReleaseAttackToken(AActor* Target, AActor* Requester);

	/** 슬롯의 월드 좌표(타깃 중심 링). Step2에서 NavMesh 투영 추가. */
	FVector GetSlotLocation(const AActor* Target, int32 SlotIndex, bool bUseOuterRadius = false, float MinNoise = 0.f,
	                        float MaxNoise = 0.f, float InnerRadiusOverride = 0.f, float OuterRadiusOverride = 0.f,
	                        float MaxWaiterRadiusOverride = 0.f) const;
	
	/** 링이 꽉 찼을 때 넘치는 적이 대기할 외곽 standoff 위치 */
	FVector GetOverflowStandoffLocation(const AActor* Target, const AActor* Requester, float DesiredRadius) const;
	
	/** 현재 슬롯을 점유한(교전 중인) 적 수 */
	int32 GetCommittedCount(const AActor* Target) const;
	
	/** 배정된 슬롯의 인덱스를 반환 */
	int32 GetCurrentSlot(AActor* Target, AActor* Requester) const;
	
	/** 기존 슬롯을 해제하고, 원하는 TargetSlotIndex가 비어있다면 강제 선점 */
	UFUNCTION(BlueprintCallable, Category = "Encirclement")
	int32 ShiftSlotExplicit(AActor* Target, AActor* Requester, int32 TargetSlotIndex);
	
	int32 GetNumSlots() const {return NumSlots;}
	
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	struct FRing
	{
		TArray<TWeakObjectPtr<AActor>> Slots;
		TArray<TWeakObjectPtr<AActor>> AttackTokens;
		FVector Anchor = FVector::ZeroVector; // 대기자 링이 구성되는 기준이 되는 고정된 중심
		TMap<TWeakObjectPtr<AActor>, float> TokenReleaseTime;
		bool bAnchorValid = false;
	}; // Slots.Num() == NumSlots
	
	FRing& FindOrAddRing(AActor* Target);
	void CompactInvalidAttackTokens(FRing& Ring) const;
	int32 GetAttackTokenCost(const AActor* Requester) const;
	int32 GetAttackTokenBudget(const FRing& Ring, const AActor* Requester) const;
	int32 GetCurrentAttackTokenCost(const FRing& Ring) const;
	bool IsOnTokenCooldown(const FRing& Ring, const AActor* Requester) const;
	
	float ComputeRingRadius(int32 NumOccupants, float BaseRadius, float MaxRadius) const;
	int32 CountWaiters(const FRing& Ring) const;
	float SlotRadiusNoise(int32 SlotIndex, float MinNoise, float MaxNoise) const;
	bool IsAmongBestCandidates(const FRing& Ring, const AActor* Target, const AActor* Requester, int32 SlotsAvailable) const;

	int32 PickBalancedSlot(const FRing& Ring, float BearingAngle) const;
	int32 DistanceToNearestOccupied(const FRing& Ring, int32 SlotIndex) const;
	
	
	void DrawDebug() const; 
	
private:
	int32 NumSlots = 10;
	float InnerRadius = 160.f;   // 공격 사거리(200) 약간 안쪽
	float OuterRadius = 300.f;   // 공격 사거리(200) 약간 안쪽
	
	// 어택 토큰 패턴 적용
	int32 DefaultAttackTokenBudget = 3;
	
	TMap<TWeakObjectPtr<AActor>, FRing> Rings;   // TWeakObjectPtr 키 → GC 시 자동 무효
};
