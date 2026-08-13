#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EncirclementSubsystem.generated.h"

/**
 * 타깃 주변에 이동 슬롯과 공격 토큰을 배정해 다수의 Enemy가 분산되도록 관리한다.
 */
UCLASS()
class RETRIEVE_API UEncirclementSubsystem : public UTickableWorldSubsystem   
{
	GENERATED_BODY()

public:
	/** 점유 분산과 접근 방향을 고려해 빈 슬롯을 예약하고 인덱스를 반환한다. */
	int32 RequestSlot(AActor* Target, AActor* Requester);

	/** Requester가 점유한 슬롯 반납. */
	void ReleaseSlot(AActor* Target, AActor* Requester);

	/** 예산·쿨다운·거리 우선순위 조건을 만족하면 공격 토큰을 할당한다. */
	UFUNCTION(BlueprintCallable, Category = "Attack Token")
	bool RequestAttackToken(AActor* Target, AActor* Requester);
	
	bool HasAttackToken(AActor* Target, AActor* Requester) const;
	
	/** 공격 토큰을 할당하지 않고 현재 요청 가능 여부만 검사한다. */
	UFUNCTION(BlueprintPure, Category = "Attack Token")
	bool CanRequestAttackToken(AActor* Target, AActor* Requester) const;
	
	/** 공격 완료 또는 이탈 시 공격 토큰을 반납한다. */
	UFUNCTION(BlueprintCallable, Category = "Attack Token")
	void ReleaseAttackToken(AActor* Target, AActor* Requester);
	
	/** 플레이어가 Deadband를 벗어났을 때 대기자 링 앵커를 현재 위치로 갱신한다. */
	FVector GetOrUpdateRingAnchor(AActor* Target);
	
	/** 타깃 주변 슬롯 위치를 계산하고 NavMesh에 투영한다. */
	FVector GetSlotLocation(const AActor* Target, int32 SlotIndex, bool bUseOuterRadius = false, float MinNoise = 0.f,
	                        float MaxNoise = 0.f, float InnerRadiusOverride = 0.f, float OuterRadiusOverride = 0.f,
	                        float MaxWaiterRadiusOverride = 0.f) const;
	
	/** 링이 꽉 찼을 때 넘치는 적이 대기할 외곽 standoff 위치 */
	FVector GetOverflowStandoffLocation(const AActor* Target, const AActor* Requester, float DesiredRadius) const;
	
	/** 현재 유효한 슬롯 점유 수를 반환한다. */
	int32 GetCommittedCount(const AActor* Target) const;
	
	/** 배정된 슬롯의 인덱스를 반환 */
	int32 GetCurrentSlot(AActor* Target, AActor* Requester) const;
	
	/** 요청자를 지정 슬롯으로 이동한다. 링이 가득 찬 경우 토큰이 없는 점유자와 교환할 수 있다. */
	UFUNCTION(BlueprintCallable, Category = "Encirclement")
	int32 ShiftSlotExplicit(AActor* Target, AActor* Requester, int32 TargetSlotIndex);
	
	int32 GetNumSlots() const {return NumSlots;}
	
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	struct FRing
	{
		TArray<TWeakObjectPtr<AActor>> Slots; // 항상 NumSlots 크기로 유지한다.
		TArray<TWeakObjectPtr<AActor>> AttackTokens;
		FVector Anchor = FVector::ZeroVector; // 대기자 링이 구성되는 기준이 되는 고정된 중심
		TMap<TWeakObjectPtr<AActor>, float> TokenReleaseTime;
		bool bAnchorValid = false;
	};
	
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
	float InnerRadius = 160.f;   // 공격 토큰 보유자의 기본 링 반경
	float OuterRadius = 300.f;   // 대기자의 기본 링 반경
	
	int32 DefaultAttackTokenBudget = 3;
	
	// 액터 파괴 시 키는 무효화되지만 엔트리는 자동 제거되지 않는다.
	TMap<TWeakObjectPtr<AActor>, FRing> Rings;
};
