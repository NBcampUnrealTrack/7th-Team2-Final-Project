
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SwimDetectionComponent.generated.h"


class ACharacter;
class IRetrieveWaterProvider;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSwimWaterRegionChanged, bool /*bEntered*/);

UCLASS(ClassGroup="Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API USwimDetectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USwimDetectionComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*) override;

	/** 물 박스 진입 시 호출(WaterBox 오버랩). 후보 Provider 등록 + 폴링 시작. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Swim")
	void NotifyEnterWaterRegion(const TScriptInterface<IRetrieveWaterProvider>& InWater);
	/** 물 박스 이탈 시 호출. 해당 후보 Provider만 제거. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Swim")
	void NotifyExitWaterRegion(const TScriptInterface<IRetrieveWaterProvider>& InWater);

	/** 입력 레이어(HeroComponent)가 수직 트림 의도 전달. +1 상승 / -1 하강 / 0. 적용은 이 컴포넌트 Tick. */
	void SetSwimVerticalInput(float Axis) { VerticalInput = Axis; }

	/** suppress 볼륨 진입(+1)/이탈(-1). >0이면 수영 진입 차단 + 유지 중이면 이탈. */
	void ChangeWaterSuppress(int32 Delta);

	/** 현재 진입한 물 영역 Provider(없으면 무효). FX 등 외부가 활성 수역 질의. */
	const TScriptInterface<IRetrieveWaterProvider>& GetCurrentWater() const { return CurrentWater; }

	FOnSwimWaterRegionChanged OnSwimWaterRegionChanged;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 깊이 임계/바닥 거리 등 튜너블은 URetrieveSwimSettings(Project Settings > Retrieve > Swim)로 이전됨.

private:
	/** 바닥 체크 헬퍼 */
	bool HasFloorBelow() const;

	/** 스폰/로드로 이미 물 안에 있을 때(BeginOverlap 미발화) 다음 틱 초기 진입 검사. */
	void CheckInitialWaterOverlap();

	/** 후보 Provider 중 실제 물기둥 안에 있는 Provider를 CurrentWater로 선택. */
	bool ResolveCurrentWater(const FVector& Location, float& OutSurfaceZ);

	/** MOVE_Flying 직접 제어. 진입=MOVE_Flying(중력 off·표면클램프 없음), 해제=MOVE_Falling. */
	void SetSwimming(bool bEnable);

	bool bSwimming = false; // 수영 상태 단일 소스(IsSwimming은 Flying에서 false라 대체)

	UPROPERTY(Transient)
	TScriptInterface<IRetrieveWaterProvider> CurrentWater;

	UPROPERTY(Transient)
	TArray<TScriptInterface<IRetrieveWaterProvider>> CandidateWaters;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	float VerticalInput = 0.f; // 수영 상하 트림 (입력 레이어가 SetSwimVerticalInput로 설정)

	int32 WaterSuppressCount = 0; // 겹친 suppress 볼륨 수 (>0이면 수영 차단)
};
