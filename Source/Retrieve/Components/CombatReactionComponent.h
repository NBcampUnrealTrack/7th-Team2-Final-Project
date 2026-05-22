
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatReactionComponent.generated.h"

class ULockOnComponent;
// class UHitReactionComponent; // TODO 2주차 추가 예정
// class UCombatFeedbackComponent // TODO 2주차 추가 예정

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnTargetChangedRelay, AActor*, NewTarget);

/**
 * 플레이어의 전투 반응 레이어 Facade
 * LockOn/HitReaction/CombatFeedback sub-component를 런타임에 생성해 Owner Actor에 부착
 * 캐릭터/GA가 이 컴포넌트 1개만 알도록 통합
 */

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API UCombatReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatReactionComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	// LockOn Facade
	// Tab 토글
	UFUNCTION(BlueprintCallable, Category = "Retrieve|CombatReaction|LockOn")
	bool TryToggleLockOn();
	// 좌/우 입력 기반 인접 타겟 전환
	UFUNCTION(BlueprintCallable, Category = "Retrieve|CombatReaction|LockOn")
	bool TrySwitchLockOnTarget(FVector2D InputDir);
	// Getter
	UFUNCTION(BlueprintPure, Category = "Retrieve|CombatReaction|LockOn")
	AActor* GetLockOnTarget() const;
	UFUNCTION(BlueprintPure, Category = "Retrieve|CombatReaction|LockOn")
	bool IsLockedOn() const;
	// Sub-Comp 직접 접근(디버그/특수용도)
	UFUNCTION(BlueprintPure, Category = "Retrieve|CombatReaction|LockOn")
	ULockOnComponent* GetLockOnComponent() const { return LockOnComp; }
	// Sub-Comp의 OnTargetChanged를 외부로 릴레이
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|CombatReaction|LockOn")
	FOnLockOnTargetChangedRelay OnLockOnTargetChanged;
protected:
	// BeginPlay에서 Owner Actor에 런타임 생성/부착되는 하위 기능 컴포넌트
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Retrieve|CombatReaction|SubComponents")
	TObjectPtr<ULockOnComponent> LockOnComp;
	// TODO 2주차: UHitReactionComponent
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|CombatReaction|SubComponents")
	// TObjectPtr<UHitReactionComponent> HitReactionComp;

	// TODO 2주차: UCombatFeedbackComponent
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|CombatReaction|SubComponents")
	// TObjectPtr<UCombatFeedbackComponent> CombatFeedbackComp;

private:
	UFUNCTION()
	void HandleLockOnTargetChanged(AActor* NewTarget);
};
