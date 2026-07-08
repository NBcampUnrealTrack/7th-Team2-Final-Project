#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "RetrieveHazardVolume.generated.h"

class UBoxComponent;
class UGameplayEffect;
class UPrimitiveComponent;

/**
 * 레벨에 배치하는 데미지 볼륨(용암/독/가시 등).
 *
 * 볼륨에 들어온 액터(ASC 보유)에게 주기적으로 데미지 GE를 자가 적용한다.
 * 소스(오너 ASC)가 필요 없는 환경 데미지 — 낙하 데미지(ApplyFallDamage)와 동일 경로.
 * HP가 0이 되면 기존 사망 시스템이 처리한다.
 *
 * ─ BP/레벨 세팅 ──────────────────────────────────────────────────────────────
 *  1. 이 클래스로 배치(또는 BP 생성) + HazardVolume 박스를 위험 구역 크기에 맞게 조정.
 *  2. DamageEffect에 SetByCaller 데미지 GE 지정(예: 낙하 데미지용 GE_FallDamage — Data.Damage.Fall을 읽음).
 *  3. DamagePerTick / DamageInterval 설정.
 *  4. (선택) OnHazardDamageApplied 이벤트에 화상 VFX/사운드 연출.
 *  ※ 데미지·사망은 서버 권위. 볼륨 콜리전은 Pawn을 Overlap하도록 되어 있음.
 */
UCLASS(Blueprintable)
class RETRIEVE_API ARetrieveHazardVolume : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveHazardVolume();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnHazardBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnHazardEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 타이머 콜백 — 구역 내 모든 대상에게 데미지 적용. */
	void ApplyPeriodicDamage();

	/** 대상 자기 ASC에 데미지 GE를 자가 적용. */
	void ApplyDamageTo(AActor* TargetActor);

	/** 데미지 적용 대상인지(ASC 보유). */
	bool IsAffectable(AActor* OtherActor) const;

	/** 대상이 데미지를 받을 때 발동(BP 연출: 화상 VFX/사운드 등). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Hazard")
	void OnHazardDamageApplied(AActor* TargetActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Hazard")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 데미지 판정 볼륨. BP에서 위험 구역 크기에 맞게 조정. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Hazard")
	TObjectPtr<UBoxComponent> HazardVolume;

	/** 적용할 데미지 GE(SetByCaller 방식). 예: GE_FallDamage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Hazard")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** DamageEffect가 읽는 SetByCaller 데미지 태그(기본 Data.Damage.Fall). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Hazard", meta = (Categories = "Data.Damage"))
	FGameplayTag DamageSetByCallerTag;

	/** 틱당 데미지량. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Hazard", meta = (ClampMin = "0.0"))
	float DamagePerTick = 20.f;

	/** 데미지 적용 간격(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Hazard", meta = (ClampMin = "0.05"))
	float DamageInterval = 0.5f;

	/** true면 진입 즉시 첫 데미지, false면 첫 간격 후부터. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Hazard")
	bool bDamageOnEnter = true;

private:
	FTimerHandle DamageTimerHandle;

	/** 현재 볼륨 안에 있는 대상들. */
	TSet<TWeakObjectPtr<AActor>> ActorsInside;
};
