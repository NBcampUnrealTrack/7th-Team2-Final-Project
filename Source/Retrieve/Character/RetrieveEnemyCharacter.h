#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveCombatCharacter.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "RetrieveEnemyCharacter.generated.h"

class URetrieveAbilitySystemComponent;
class UEnemyCombatComponent;
class UPatternCounterComponent;
class UDropComponent;
class USphereComponent;

struct FEnemyPlayerSpottedPayload;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathEnded, AActor*, DeadCharacter);
	
UCLASS()
class RETRIEVE_API ARetrieveEnemyCharacter : public ARetrieveCombatCharacter
{
	GENERATED_BODY()

public:
	ARetrieveEnemyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	void SetRespawnable(bool NewRespawnable);
	
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Enemy")
	void HandleDeathEnded(AActor* OwningActor);
	
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Enemy")
	void ActivateEnemy(const FTransform& SpawnTransform, bool bIsRespawn = false);
	void DeactivateEnemy();
	
	// ABP Property Access 바인딩 전용
	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsDeadForAnim()    const { return bCachedIsDead; }

	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsChasingForAnim() const { return bCachedIsChasing; }

	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsHitForAnim()     const { return bCachedIsHit; }

	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsStaggeredForAnim()  const { return bCachedIsStaggered; }
	
	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsGroggyForAnim()  const { return bCachedIsGroggy; }
	
protected:
	virtual void BeginPlay() override;
	
	
	virtual void InitializeAbilitySystem() override;
	
	virtual void InitializeComponents();
	
	virtual void HandleDeathStarted(AActor* OwningActor) override;

	
	void OnDeadTagChanged(const FGameplayTag Tag, int32 Count);
	void OnChaseTagChanged(const FGameplayTag Tag, int32 Count);
	void OnHitTagChanged(const FGameplayTag Tag, int32 Count);
	void OnStaggeredTagChanged(const FGameplayTag Tag, int32 Count);
	void OnGroggyTagChanged(const FGameplayTag Tag, int32 Count);
	
private:
	void OnAlerted(FGameplayTag Channel, const FEnemyPlayerSpottedPayload& Payload);
	
public:
	UPROPERTY()
	TObjectPtr<AActor> AlertedTarget;
	
	UPROPERTY(BlueprintAssignable)
	FOnDeathEnded OnDeathEnded;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|AbilitySystem")
	TObjectPtr<URetrieveAbilitySystemComponent> OwnedASC;

	/** 공격 패턴 선택·발동·쿨다운 담당 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

	/** 패턴 카운터 윈도우 추적 및 그로기 트리거. 에픽·보스에서 확장 재사용. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UPatternCounterComponent> PatternCounterComponent;

	/** 사망 시 드랍 아이템 처리 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UDropComponent> DropComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Combat")
	TObjectPtr<USphereComponent> FistHitbox;
	
	/** 감지된 플레이어 전파 수신 */
	FGameplayMessageListenerHandle GroupAlertHandle;

	UPROPERTY(EditAnywhere, Category = "Retrieve|AI")
	float GroupAlertRadius = 1500.f;
	
	/** DataTable */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Monster")
	FName MonsterDataRowName;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Monster")
	TObjectPtr<UDataTable> MonsterDataTable;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Monster")
	TObjectPtr<UDataTable> PatternTable;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Monster")
	TObjectPtr<UDataTable> DropTable;
	
	bool bRespawnable = false;
	
private:
	// Ragdoll 복구용
	FTransform InitialMeshRelativeTransform;
	
	float DefaultGravityScale = 1.0f;
	EMovementMode DefaultMovementMode;
	
	bool bCachedIsDead      = false;
	bool bCachedIsChasing   = false;
	bool bCachedIsHit       = false;
	bool bCachedIsStaggered    = false;
	bool bCachedIsGroggy    = false;
};
