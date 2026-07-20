#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameplayEffectTypes.h"
#include "RetrieveHealthComponent.generated.h"

class UCombatAttributeSet;
class URetrieveAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChanged, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathStarted, AActor*, OwningActor);

/**
 * HealthComponent가 이벤트를 소유하고, AttributeSet이 값을 소유합니다.
 * 체력이 0 이하로 떨어지면 OnDeathStarted를 발동합니다.
 * 아키타입별 사망 처리 로직은 OnDeathStarted를 구독하는 캐릭터 서브클래스에서 구현합니다.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveHealthComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	URetrieveHealthComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	void InitializeWithAbilitySystem(URetrieveAbilitySystemComponent* InASC);
	void UninitializeWithAbilitySystem();
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Health")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Health")
	bool IsDeadOrDying() const { return bDeathStarted; }
	
	FORCEINLINE AActor* GetLastDamageInstigator() const { return LastDamageInstigator.Get(); }
	FORCEINLINE AActor* GetLastDamageCauser()    const { return LastDamageCauser.Get(); }
	
	void NotifyDamageContext(AActor* InInstigator, AActor* InDamageCauser)
	{
		LastDamageInstigator = InInstigator;
		LastDamageCauser    = InDamageCauser;
	}
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ResetHealth();
	void Revive();
	void KillOwner();

	/** 부활 직후 데미지 면역 창이 활성인지. 시체 위치의 잔류 위협(해저드 주기 틱/잔류 투사체)이
	 * 부활 프레임에 즉시 재사망시키면, Result 재전환이 동일 상태로 무시된 채 InGame으로 넘어가
	 * "죽은 폰 + InGame"(조작 가능한 시체) 소프트락이 된다. CombatAttributeSet의 데미지 적용부가 확인.
	 * 사망→부활(Revive) 경로에서만 활성 — 모닥불 휴식의 적 ResetHealth에는 적용되지 않는다. */
	bool IsReviveProtectionActive() const;

public:
	UPROPERTY(BlueprintAssignable)
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnHealthChanged OnMaxHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnDeathStarted OnDeathStarted;
	
	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Health")
	TWeakObjectPtr<AActor> LastDamageInstigator;

	UPROPERTY(BlueprintReadOnly, Category="Retrieve|Health")
	TWeakObjectPtr<AActor> LastDamageCauser;

private:
	/** 부활 보호 종료 시각(RealTimeSeconds, 딜레이션 무관). 음수 = 비활성. Revive()가 세팅. */
	double ReviveProtectionEndRealTime = -1.0;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	virtual void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void OnRep_DeathStarted();

	UPROPERTY()
	TObjectPtr<URetrieveAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<const UCombatAttributeSet> AttributeSet;
	
	UPROPERTY(ReplicatedUsing = OnRep_DeathStarted)
	bool bDeathStarted = false;
};
