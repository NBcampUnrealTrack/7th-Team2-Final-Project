#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatReactionComponent.generated.h"

class ULockOnComponent;
class ULockOnCameraRig;
class ULockOnTargetHighlighter;
class ULockOnConfig;
class ULockOnCameraConfig;
class UHitReactionComponent;
class URetrieveHitReactionProfile;
class UUserWidget;
class UWidgetComponent;

// class UCombatFeedbackComponent // TODO: 추가 예정

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnTargetChangedRelay, AActor*, NewTarget);

/**
 * 플레이어의 전투 반응 레이어 Facade
 */
UCLASS(ClassGroup = "Retrieve", meta=(BlueprintSpawnableComponent))
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
	float GetTurnInterpSpeed() const;
	UFUNCTION(BlueprintPure, Category = "Retrieve|CombatReaction|LockOn")
	bool IsLockedOn() const;
	// Sub-Comp 직접 접근(디버그/특수용도)
	UFUNCTION(BlueprintPure, Category = "Retrieve|CombatReaction|LockOn")
	ULockOnComponent* GetLockOnComponent() const { return LockOnComp; }
	// Sub-Comp의 OnTargetChanged를 외부로 릴레이
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|CombatReaction|LockOn")
	FOnLockOnTargetChangedRelay OnLockOnTargetChanged;

	// HitReaction Facade
	UFUNCTION(BlueprintPure, Category = "Retrieve|CombatReaction|HitReact")
	UHitReactionComponent* GetHitReactionComponent() const { return HitReactionComp; }
protected:
	// 튜닝 Config 
	// LockOnComp 전용 탐색/스코어링/해제 파라미터
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|CombatReaction|config")
	TObjectPtr<ULockOnConfig> LockOnConfig;
	// LockOnCameraRig 전용 카메라 추적/오프셋 파라미터
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|CombatReaction|Config")
	TObjectPtr<ULockOnCameraConfig> LockOnCameraConfig;
	// 락온 레티클 WBP(HUD_Reticle_Crosshair_05 등). 미지정 시 외곽선만 동작
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|CombatReaction|LockOn|Reticle")
	TSubclassOf<UUserWidget> ReticleWidgetClass;
	// 타겟 몸통 중앙 소켓 이름. 없으면 바운드 중심으로 폴백
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|CombatReaction|LockOn|Reticle")
	FName LockOnSocketName = TEXT("LockOnSocket");
	// 레티클 스크린 공간 그리기 크기(픽셀)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|CombatReaction|LockOn|Reticle")
	FVector2D ReticleDrawSize = FVector2D(64.f, 64.f);
	// 레티클 앵커 기준점(0~1). (0.5,0.5)면 소켓 지점에 정중앙 정렬(몸 기준 고정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|CombatReaction|LockOn|Reticle")
	FVector2D ReticlePivot = FVector2D(0.5f, 0.5f);
	// 캡슐 세로 기준점: 0=바닥, 0.5=중심, 1=상단
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|CombatReaction|LockOn|Reticle", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReticleCapsuleHeightRatio = 0.5f;
	// 기준점 위에 추가할 부착 오프셋(cm). 캡슐 없으면 소켓 기준으로 적용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|CombatReaction|LockOn|Reticle")
	FVector ReticleAttachOffset = FVector(0.f, 0.f, 0.f);
	// BeginPlay에서 Owner Actor에 런타임 생성/부착되는 하위 기능 컴포넌트
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Retrieve|CombatReaction|SubComponents")
	TObjectPtr<ULockOnComponent> LockOnComp;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Retrieve|CombatReaction|SubComponents")
	TObjectPtr<ULockOnCameraRig> LockOnCameraRigComp;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Retrieve|CombatReaction|SubComponents")
	TObjectPtr<ULockOnTargetHighlighter> LockOnHighlighter;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|CombatReaction|HitReact")
	TObjectPtr<URetrieveHitReactionProfile> HitReactionProfile;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Retrieve|CombatReaction|SubComponents")
	TObjectPtr<UHitReactionComponent> HitReactionComp;

	// TODO: UCombatFeedbackComponent
	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|CombatReaction|SubComponents")
	// TObjectPtr<UCombatFeedbackComponent> CombatFeedbackComp;

private:
	UFUNCTION()
	void HandleLockOnTargetChanged(AActor* NewTarget);
	// 레티클 위젯 컴포넌트 lazy 생성(스크린 공간)
	void EnsureReticleComp();
	// 타겟의 LockOnSocket에 스냅(없으면 메시에 붙인 뒤 바운드 중심으로 올림)
	void AttachReticleToTarget(AActor* Target);
	// 타겟에 어태치되는 레티클 위젯 컴포넌트
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> ReticleWidgetComp;
};
