
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SwimDetectionComponent.generated.h"


class ACharacter;
class IRetrieveWaterProvider;

UCLASS(ClassGroup="Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API USwimDetectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USwimDetectionComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*) override;

	/** 물 박스 진입 시 호출(WaterBox 오버랩). Provider 캐싱 + 폴링 시작. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Swim")
	void NotifyEnterWaterRegion(const TScriptInterface<IRetrieveWaterProvider>& InWater);
	/** 물 박스 이탈 시 호출. 수영 해제 + 폴링 중단. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Swim")
	void NotifyExitWaterRegion();

	/** 입력 레이어(HeroComponent)가 수직 트림 의도 전달. +1 상승 / -1 하강 / 0. 적용은 이 컴포넌트 Tick. */
	void SetSwimVerticalInput(float Axis) { VerticalInput = Axis; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Swim")
	float ChestThreshold = 40.f; // 진입: (수면Z - 액터Z) > 이 값. TODO(6.5): SwimSettings

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Swim")
	float WadeThreshold = 25.f; // 이탈(히스테리시스)

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Swim")
	float UnderwaterDepthThreshold = 150.f; // 이 깊이보다 깊으면 수중 모드(3D). TODO(6.5)

private:
	/** MOVE_Flying 직접 제어. 진입=MOVE_Flying(중력 off·표면클램프 없음), 해제=MOVE_Falling. */
	void SetSwimming(bool bEnable);

	bool bSwimming = false; // 수영 상태 단일 소스(IsSwimming은 Flying에서 false라 대체)

	UPROPERTY(Transient)
	TScriptInterface<IRetrieveWaterProvider> CurrentWater;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	float VerticalInput = 0.f; // 수영 상하 트림 (입력 레이어가 SetSwimVerticalInput로 설정)
};
