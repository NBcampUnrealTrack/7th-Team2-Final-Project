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

	/** 공격 완료 또는 탈출 시 토큰 반납 */
	UFUNCTION(BlueprintCallable, Category = "Attack Token")
	void ReleaseAttackToken(AActor* Target, AActor* Requester);
	
	/** 슬롯의 월드 좌표(타깃 중심 링). Step2에서 NavMesh 투영 추가. */
	FVector GetSlotLocation(const AActor* Target, int32 SlotIndex,
		bool bUseOuterRadius = false, float MinNoise = 0.f, float MaxNoise = 0.f) const;
	
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
	};   // Slots.Num() == NumSlots
	
	FRing& FindOrAddRing(AActor* Target);
	void CompactInvalidAttackTokens(FRing& Ring) const;
	
	void DrawDebug() const; 
	
private:
	int32 NumSlots = 10;
	float InnerRadius = 160.f;   // 공격 사거리(200) 약간 안쪽
	float OuterRadius = 300.f;   // 공격 사거리(200) 약간 안쪽
	
	// 어택 토큰 패턴 적용
	int32 MaxAttackTokens = 3;
	
	TMap<TWeakObjectPtr<AActor>, FRing> Rings;   // TWeakObjectPtr 키 → GC 시 자동 무효
};
