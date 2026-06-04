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

	/** 슬롯의 월드 좌표(타깃 중심 링). Step2에서 NavMesh 투영 추가. */
	FVector GetSlotLocation(const AActor* Target, int32 SlotIndex) const;

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	
private:
	struct FRing
	{
		TArray<TWeakObjectPtr<AActor>> Slots;
	};   // Slots.Num() == NumSlots
	FRing& FindOrAddRing(AActor* Target);
	
	void DrawDebug() const; 
	
private:
	int32 NumSlots = 10;
	float Radius   = 100.f;   // 공격 사거리(200) 약간 안쪽

	TMap<TWeakObjectPtr<AActor>, FRing> Rings;   // TWeakObjectPtr 키 → GC 시 자동 무효
};
