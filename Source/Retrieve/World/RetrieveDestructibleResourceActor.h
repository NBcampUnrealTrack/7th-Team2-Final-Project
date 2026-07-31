#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interface/RetrieveAttackHitReceiver.h"
#include "RetrieveDestructibleResourceActor.generated.h"

class UGeometryCollectionComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class USoundBase;
class UStaticMeshComponent;
class URetrieveInteractionResponseComponent;

/**
 * 공격으로 일정 횟수 피격 시 Chaos Geometry Collection으로 파괴되며 보상을 지급하는 월드 자원(광물 등).
 * ASC를 붙이지 않고 IRetrieveAttackHitReceiver로 공격 판정을 받는다.
 * 보상 지급은 기존 URetrieveInteractionResponseComponent를 재사용한다(InteractionManager는 붙이지 않음).
 */
UCLASS(Blueprintable)
class RETRIEVE_API ARetrieveDestructibleResourceActor : public AActor, public IRetrieveAttackHitReceiver
{
	GENERATED_BODY()

public:
	ARetrieveDestructibleResourceActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool ReceiveRetrieveAttackHit_Implementation(
		AActor* Attacker,
		const FHitResult& HitResult,
		FGameplayTag AttackTypeTag,
		FGameplayTag ElementTag) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UStaticMeshComponent> IntactMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UGeometryCollectionComponent> FracturedMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrieveInteractionResponseComponent> RewardComponent;

	/** 파괴에 필요한 피격 횟수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource", meta = (ClampMin = "1"))
	int32 RequiredHitCount = 4;

	UPROPERTY(ReplicatedUsing = OnRep_Broken, VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource")
	bool bBroken = false;

	/** 세이브 슬롯 저장 키. 비우면 액터 이름으로 폴백. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource")
	FName PersistentId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource")
	int32 CurrentHitCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Chaos")
	float BreakRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Chaos")
	float BreakStrain = 500000.0f;

	/** 파괴 순간 조각에 가하는 바깥 방향 임펄스 세기. ApplyExternalStrain은 클러스터 결합만 끊을 뿐 힘을 주지 않아 값을 따로 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Chaos")
	float BreakImpulseStrength = 300000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Chaos")
	float BrokenLifeSpan = 5.0f;

	/** 일반 타격 시 충격 지점에 재생할 기본 Niagara 효과. BP에서 자원별로 교체할 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Feedback")
	TSoftObjectPtr<UNiagaraSystem> HitImpactVFX = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/Retrieve/Blueprints/VFX/Hit/NS_Hit_Impact.NS_Hit_Impact")));

	/** 일반 타격 시 재생할 기본 공간 음향. BP에서 자원별로 교체할 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Feedback")
	TSoftObjectPtr<USoundBase> HitImpactSound = TSoftObjectPtr<USoundBase>(
		FSoftObjectPath(TEXT("/Game/Retrieve/Audio/SFX/SwordShield/SC_SSH_SwordHit.SC_SSH_SwordHit")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Feedback", meta = (ClampMin = "0.05"))
	float HitShakeDuration = 0.2f;

	// 타격 시 흔들림 거리(UU). "깨지는 중"이 확실히 보이도록 기본값을 키웠다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Feedback", meta = (ClampMin = "0.0"))
	float HitShakeDistance = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Feedback", meta = (ClampMin = "0.0"))
	float HitShakeRotationDegrees = 5.0f;

	/**
	 * 타격 시 순간 스쿼시(축소→복원) 강도. 0이면 스케일 펀치 없음.
	 * 머티리얼 크랙과 무관하게 메시가 움찔하므로 어떤 자원이든 타격 반응이 확실히 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Feedback", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float HitScalePunch = 0.14f;

	/**
	 * 진행형 크랙 머티리얼 스칼라 파라미터 이름.
	 * IntactMesh 머티리얼에 이 이름의 스칼라 파라미터가 있으면 타격마다 0→1로 균열이 진행된다.
	 * 파라미터가 없으면 무해하게 무시된다(별도 아트 없이도 나머지 피드백은 동작).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Feedback")
	FName CrackProgressParamName = TEXT("CrackProgress");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Resource|Feedback", meta = (ClampMin = "0.01"))
	float HitImpactVFXScale = 0.85f;

protected:
	/** 파괴 확정 처리(서버 전용): bBroken 설정 → IntactMesh 콜리전 해제 → 보상 지급 → 파괴 연출 전파 */
	void BreakResource(AActor* Attacker, const FVector& ImpactPoint);

	/** 파괴 시각 상태 적용. 두 번 호출돼도 안전(멱등). */
	void ApplyBrokenVisual(const FVector& ImpactPoint);

	UFUNCTION()
	void OnRep_Broken();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayHitFeedback(FVector_NetQuantize ImpactPoint, FVector_NetQuantizeNormal ImpactNormal, int32 HitCount);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayBreak(FVector_NetQuantize ImpactPoint);

	/** BP가 자원별 VFX/SFX/흔들림을 연출하도록 노출하는 확장 지점. 비파괴 피격마다 호출됨. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Resource|Feedback")
	void PlayHitFeedback(FVector ImpactPoint, FVector ImpactNormal, int32 HitCount, float HitProgress);

	/** BP가 파괴 순간 연출(강한 VFX/SFX)을 구성하도록 노출하는 확장 지점. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Resource|Feedback")
	void PlayBreakFeedback(FVector ImpactPoint);

private:
	FName GetSaveId() const;
	class URetrieveSaveSubsystem* GetSaveSubsystem() const;

	UFUNCTION()
	void HandleSaveLoaded();

	// Option A: 파괴 시 Destroy 대신 숨김 + 로드 시 양방향 복원.
	void HideBrokenResource();     // 파괴 연출 종료 후 숨김(채굴됨 상태)
	void ApplyDepletedInstant();   // 로드: 연출/보상 없이 즉시 고갈 상태
	void ApplyIntactState();       // 로드: 온전 상태로 복원

	FTimerHandle BrokenHideTimerHandle;
	/** 원래 IntactMesh 콜리전(복원 시 되돌리기용). BeginPlay에서 캡처. */
	TEnumAsByte<ECollisionEnabled::Type> DefaultIntactCollision = ECollisionEnabled::QueryAndPhysics;

	void PlayDefaultHitFeedback(const FVector& ImpactPoint, const FVector& ImpactNormal, int32 HitCount, float HitProgress);
	void StartHitShake(const FVector& ImpactNormal, int32 HitCount, float HitProgress);
	void UpdateHitShake();
	void StopHitShake();

	/** IntactMesh 각 머티리얼 슬롯을 다이나믹 인스턴스로 교체해 크랙 진행 파라미터를 구동할 수 있게 한다. */
	void CreateCrackMIDs();

	/** 크랙 진행도(0~1)를 모든 MID에 적용. 파라미터가 없으면 무해하게 무시된다. */
	void UpdateCrackProgress(float Progress);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CrackMIDs;

	/** ApplyBrokenVisual 중복 실행 방지(멀티캐스트 + OnRep이 겹쳐도 한 번만 재생) */
	bool bBreakVisualPlayed = false;
	bool bHitBaseTransformCaptured = false;
	FTransform HitBaseRelativeTransform;
	FVector HitShakeLocalDirection = FVector::BackwardVector;
	float HitShakeStartTime = 0.0f;
	float HitShakeStrength = 1.0f;
	int32 HitShakeSeed = 0;
	FTimerHandle HitShakeTimer;
};
