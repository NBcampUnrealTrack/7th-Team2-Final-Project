
#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "ReticleViewModel.generated.h"

struct FRetrieveBowChargePayload;
class UAbilitySystemComponent;

// 위젯이 UMG 애님을 트리거하도록 (Tick 폴링 회피) MaxChargeTime을 재생 길이로 활용
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBowChargeStarted, float, MaxChargeTime);    
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBowChargeEnded, bool, bReleased, float, ChargeRatio);

/**                                                                
 * 레티클 UI 구동. Channel.Bow.Charge(GA_BowShot 발행)를 구독해 조준/차징 상태를 노출한다.                                         
 * 차징 진행값 자체는 위젯 UMG 애니메이션이 그리고, 여기선 시작/종료 이벤트 + 표시여부만 다룬다.                              
 */ 
UCLASS(BlueprintType)
class RETRIEVE_API UReticleViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()
	
public:
	// Channel.Bow.Charge 구독 시작/종료, PlayerController 가 HUD 세팅/해제 시 호출
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void BindToMessage(UWorld* World);
	
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void UnbindFromMessages();

	// State.Player.Aiming 태그 감시 시작/종료. PlayerController가 ASC 준비 시 호출.
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void BindToASC(UAbilitySystemComponent* InASC);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI")
	void UnbindFromASC();

	// 조준(우클릭) 중이면 true — 크로스헤어(레티클) 표시 여부 바인딩용.
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	bool GetIsAiming() const { return bIsAiming; }

	// 차징(좌클릭 홀드) 중이면 true — 차징 게이지 표시 여부 바인딩용.
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	bool GetIsCharging() const { return bIsCharging; }
	
	// 풀차징까지 시간(초) — 위젯 애니메이션 길이.
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|UI")
	float GetMaxChargeTime() const { return MaxChargeTime; }
	
	// 차징 시작 — 위젯이 fill 애니메이션을 MaxChargeTime 길이로 재생.
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|UI")
	FOnBowChargeStarted OnBowChargeStarted;

	// 차징 종료 — bReleased=true 발사, false 취소(역재생 복귀). ChargeRatio는 발사 시점 비율.
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|UI")
	FOnBowChargeEnded OnBowChargeEnded;
	
private:
	void HandleChargeMessage(FGameplayTag Channel, const FRetrieveBowChargePayload& Payload);
	void HandleAimingTagChanged(FGameplayTag Tag, int32 NewCount);

	FGameplayMessageListenerHandle ChargeListener;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	bool bIsAiming = false;
	bool bIsCharging = false;
	float MaxChargeTime = 0.f;
};
