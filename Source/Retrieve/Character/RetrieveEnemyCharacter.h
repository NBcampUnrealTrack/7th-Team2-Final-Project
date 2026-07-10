#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveCombatCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "Data/RetrieveDataTableTypes.h"
#include "RetrieveEnemyCharacter.generated.h"

class URetrieveAbilitySystemComponent;
class UEnemyCombatComponent;
class UEnemyPoiseComponent;
class UPatternCounterComponent;
class UDropComponent;
class UNormalMonsterHealthBarComponent;
class USphereComponent;
class UHitReactionComponent;
class URetrieveHitReactionProfile;
class UAnimMontage;
class URetrieveMapIconComponent;
class URetrieveOverlayStackComponent;
class UEnemySuspicionIndicatorComponent;

struct FEnemyPlayerSpottedPayload;
struct FMonsterDataRow;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathEnded, AActor*, DeadCharacter);

UCLASS()
class RETRIEVE_API ARetrieveEnemyCharacter : public ARetrieveCombatCharacter, public IGenericTeamAgentInterface
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

	void SetAerialMode(bool bAerial);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Enemy")
	void AlertFromDamageInstigator(AActor* DamageInstigator);

	// ABP Property Access 바인딩 전용
	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsDeadForAnim()    const { return bCachedIsDead; }

	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsChasingForAnim() const { return bCachedIsChasing; }

	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsSpecialAttackingForAnim()     const { return bCachedIsSpecialAttacking; }

	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsAttackingForAnim()     const { return bCachedIsAttacking; }
	
	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsHitForAnim()     const { return bCachedIsHit; }

	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsStaggeredForAnim()  const { return bCachedIsStaggered; }
	
	UFUNCTION(BlueprintPure, Category="Retrieve|Enemy|Animation", meta=(BlueprintThreadSafe))
	bool IsGroggyForAnim()  const { return bCachedIsGroggy; }
	
	const FMonsterDataRow* GetMonsterDataRow() const;
	FName GetMonsterDataRowName() const { return MonsterDataRowName; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Enemy|Epic")
	bool HasAerialPhase() const;
	virtual bool ShouldUseStateTreeAerialPhase() const { return HasAerialPhase(); }

	void SetAerialSpecialAttackReady(bool bReady);
	bool IsAerialSpecialAttackReady() const { return bAerialSpecialAttackReady; }
	void BeginAerialSpecialPhase();
	void ResetAerialSpecialPhase();
	float GetAerialSpecialPhaseElapsedTime() const;

	virtual bool UsesForwardLocomotion() const { return false; }
	virtual void UpdateGroundTurnAnimation(float SignedYawDelta) {}
	virtual bool ShouldGroundSnapOnSpawn() const { return false; }
	virtual void StopGroundTurnAnimation() { StopLocomotionMontages(); }
	virtual bool ShouldUseDirectChaseToTarget() const { return false; }
	virtual bool ShouldFaceTargetDuringShiftOrbit() const { return false; }
	virtual bool ShouldSuppressNormalAttackWhileFlying() const { return false; }
	virtual bool ShouldUsePatternRangeForNormalAttack() const { return false; }
	virtual bool ShouldUse2DPatternRangeWhileFlying() const { return false; }
	virtual bool ShouldTreatZeroPatternMaxRangeAsUnlimited() const { return false; }
	virtual bool ShouldUseFallbackEpicStateTree() const { return false; }
	virtual float GetHorizontalHalfFOVOverrideForAI() const { return -1.f; }
	virtual float GetPeripheralVisionAngleOverrideForAI() const { return -1.f; }

	void RefreshMoveSpeedFromAttribute();
	
	virtual FGenericTeamId GetGenericTeamId() const override
	{
		return FGenericTeamId(static_cast<uint8>(Team));
	}
	
protected:
	virtual void BeginPlay() override;
	
	
	virtual void InitializeAbilitySystem() override;
	
	virtual void InitializeComponents();

	/** 적별 이동/회전 설정 훅. 에픽 등 파생 클래스가 override 하여 자신만의 이동 설정을 적용한다. */
	virtual void ConfigureEnemyMovement() {}

	/** 로코모션용 동적 몽타주(예: 그라운드 턴) 정지 훅. 공중 진입·비활성화 시 호출된다. */
	virtual void StopLocomotionMontages() {}

	virtual void HandleDeathStarted(AActor* OwningActor) override;

	void OnDeadTagChanged(const FGameplayTag Tag, int32 Count);
	void OnChaseTagChanged(const FGameplayTag Tag, int32 Count);
	void OnAttackTagChanged(const FGameplayTag Tag, int32 Count);
	void OnSpecialAttackTagChanged(const FGameplayTag Tag, int32 Count);
	void OnHitTagChanged(const FGameplayTag Tag, int32 Count);
	void OnStaggeredTagChanged(const FGameplayTag Tag, int32 Count);
	void OnGroggyTagChanged(const FGameplayTag Tag, int32 Count);
	void OnMoveSpeedChanged(const FOnAttributeChangeData& Data);
	
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

	/** Poise 누적 기반 그로기 트리거 담당 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UEnemyPoiseComponent> EnemyPoiseComponent;

	/** 사망 시 드랍 아이템 처리 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UDropComponent> DropComponent;

	/** 피격 반응 consumer */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UHitReactionComponent> HitReactionComponent;
	
	/** 상태이상·파훼 표시 등 오버레이 머티리얼 슬롯 단일 관리 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrieveOverlayStackComponent> OverlayStackComponent;
	
	/** 적별로 할당하는 피격 반응 프로파일 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Combat")
	TObjectPtr<URetrieveHitReactionProfile> HitReactionProfile;
	

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UNormalMonsterHealthBarComponent> NormalHealthBarComponent;

	/** 경계(Suspicious) 게이지를 ?/! 아이콘으로 표시 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UEnemySuspicionIndicatorComponent> SuspicionIndicatorComponent;

	/** 미니맵·나침반 마커 등록용. 기본 IconType=Enemy(보스는 생성자에서 Boss로 변경). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrieveMapIconComponent> MapIconComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Combat")
	TObjectPtr<USphereComponent> FistHitbox;
	
	/** 감지된 플레이어 전파 수신 */
	FGameplayMessageListenerHandle GroupAlertHandle;

	UPROPERTY(EditAnywhere, Category = "Retrieve|AI")
	float GroupAlertRadius = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Team")
	ERetrieveTeam Team = ERetrieveTeam::Enemy;
	
	/** DataTable */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Monster")
	FName MonsterDataRowName;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Monster")
	TObjectPtr<UDataTable> MonsterDataTable;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Monster")
	TObjectPtr<UDataTable> PatternTable;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Monster")
	TObjectPtr<UDataTable> DropTable;

	UPROPERTY(EditAnywhere, Category = "Retrieve|AI", meta = (ClampMin = "0.0"))
	float EngageStaggerMaxDelay = 1.2f;
	
	bool bRespawnable = false;

	// Ragdoll 복구용. 파생 클래스(에픽)가 메시 오프셋 조정 후 갱신할 수 있도록 protected.
	FTransform InitialMeshRelativeTransform;

private:
	float BaseMaxWalkSpeed = 0.f;
	float DefaultGravityScale = 1.0f;
	EMovementMode DefaultMovementMode = MOVE_Walking;
	
	bool bCachedIsDead      = false;
	bool bCachedIsChasing   = false;
	bool bCachedIsAttacking = false;
	bool bCachedIsSpecialAttacking = false;
	bool bCachedIsHit       = false;
	bool bCachedIsStaggered    = false;
	bool bCachedIsGroggy    = false;

	bool bAerialSpecialAttackReady = false;
	float AerialSpecialPhaseStartTime = -1.f;

	FTimerHandle AlertStaggerTimer;
};
