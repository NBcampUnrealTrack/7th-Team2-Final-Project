#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "GameplayTagContainer.h"
#include "NormalMonsterHealthBarComponent.generated.h"

class URetrieveHealthComponent;

UCLASS(ClassGroup = (Retrieve), meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UNormalMonsterHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UNormalMonsterHealthBarComponent();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Monster HP Bar")
	void SetHealthBarEnabled(bool bNewEnabled);

	/**
	 * 위젯에 표시할 이름과 등급 태그를 설정한다.
	 * EnemyCharacter BP의 BeginPlay에서 DataRow 읽은 후 호출하거나,
	 * 컴포넌트 인스턴스의 MonsterDisplayName / MonsterTypeTag를 에디터에서 직접 설정해도 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Monster HP Bar")
	void SetMonsterIdentity(FText InDisplayName, FGameplayTag InTypeTag);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Monster HP Bar")
	FText GetMonsterDisplayName() const { return MonsterDisplayName; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Monster HP Bar")
	FGameplayTag GetMonsterTypeTag() const { return MonsterTypeTag; }

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void HandleHealthChanged(float NewHealth);

	UFUNCTION()
	void HandleMaxHealthChanged(float NewMaxHealth);

	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

private:
	void BindToHealthComponent();
	void UnbindFromHealthComponent();
	void RefreshHealthPercent();
	void ShowForDuration();
	void HideBar();
	void SetBarVisible(bool bNewVisible);
	bool ShouldShowForHealth(float Health, float MaxHealth) const;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Monster HP Bar")
	bool bHealthBarEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Monster HP Bar", meta = (ClampMin = "0.0"))
	float VisibleDurationAfterDamage = 3.f;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Monster HP Bar")
	bool bHideWhenFullHealth = true;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Monster HP Bar")
	bool bShowOnBeginPlayForDebug = false;

	/** 체력바 위에 표시할 몬스터 이름. 비어 있으면 이름 텍스트 숨김 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Monster HP Bar",
		meta = (AllowPrivateAccess = "true"))
	FText MonsterDisplayName;

	/** 일반/엘리트/에픽 등 등급 태그 — 이름 색상 결정에 사용 (Monster.Type.*) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Monster HP Bar",
		meta = (Categories = "Monster.Type", AllowPrivateAccess = "true"))
	FGameplayTag MonsterTypeTag;

	UPROPERTY()
	TObjectPtr<URetrieveHealthComponent> BoundHealthComponent;

	// 생성자의 FClassFinder로 찾은 WBP 클래스 — BeginPlay에서 InitWidget() 전에 강제 적용
	UPROPERTY()
	TSubclassOf<UUserWidget> WBPWidgetClass;

	FTimerHandle HideTimerHandle;
	float LastObservedHealth = -1.f;
	float LastObservedMaxHealth = -1.f;
};
