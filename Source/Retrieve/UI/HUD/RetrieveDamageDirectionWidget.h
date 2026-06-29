#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "RetrieveDamageDirectionWidget.generated.h"

/**
 * 플레이어 피격 방향 표시 위젯 (WBP: FX_FantasyWarrior_Damage_Direction_01의 부모 클래스).
 *
 * Channel_Combat_DamageDealt 를 수신해, 로컬 플레이어가 피격 대상(Target)일 때만
 * 공격자(Instigator) 방향을 카메라 기준 상대 각도(도)로 계산하여
 * BP 이벤트 PlayDamageAnimation(DirectionAngle) 을 호출한다.
 *
 * 설정:
 * 1. WBP_..._Damage_Direction_01 의 부모 클래스를 이 클래스로 지정(Reparent).
 * 2. 기존 BP 커스텀 이벤트 PlayDamageAnimation(float DirectionAngle) 가 그대로 구현부가 된다.
 *    (Set Visibility → Set Render Transform Angle → Play Anim_Active)
 */
UCLASS()
class RETRIEVE_API URetrieveDamageDirectionWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * BP 구현: 피격 방향 연출 재생.
	 * @param DirectionAngle 카메라 정면(0도) 기준 공격자 방향 각도. +우측 / -좌측 / ±180 후방.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Damage Direction")
	void PlayDamageAnimation(float DirectionAngle);

	/** 카메라 정면과 공격자 방향이 이 각도(도) 이내로 차이나면 후방으로 간주하지 않고 정면 0으로 스냅한다(노이즈 방지). 0이면 비활성. */
	UPROPERTY(EditDefaultsOnly, Category = "Damage Direction", meta = (ClampMin = 0))
	float FrontDeadZoneDegrees = 0.f;

private:
	// Channel_Combat_DamageDealt 리스너 콜백. 로컬 플레이어가 Target일 때만 처리.
	void HandleDamageDealt(FGameplayTag Channel, const FRetrieveDamageDealtPayload& Payload);

	// 공격자 위치 → 카메라 기준 상대 yaw(도) 계산.
	bool ComputeDirectionAngle(const AActor* Attacker, float& OutAngleDeg) const;

	FGameplayMessageListenerHandle DamageListener;
};
