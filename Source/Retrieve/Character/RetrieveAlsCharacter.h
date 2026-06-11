#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AlsCharacter.h"
#include "RetrieveAlsCharacter.generated.h"

class UAbilitySystemComponent;
class URetrieveAbilitySystemComponent;
class URetrievePawnExtensionComponent;
class URetrievePawnData;
class USpringArmComponent;

/**
 * ALS 가지의 Pawn-level 베이스. ARetrieveCharacter의 거울 가지.
 * AAlsCharacter를 상속하여 ALS 로코모션/회전/시점 기능을 모두 사용하며,
 * GAS 핸드셰이크는 ARetrieveCharacter와 동일하게 PawnExtensionComponent를 통해 처리합니다.
 *
 * ALS API는 이 클래스 안에 격리됩니다. 외부(GA / BP)는 다음만 사용합니다:
 *   - 상태성: GAS LooseTag (State_Player_Sprinting / Crouching, LockOn_Active 등) — 캐릭터가 자동 동기화
 *   - 즉시성 액션: TryMantle
 *
 * 주의: Cast<ARetrieveCharacter>(Pawn)은 이 가지에 닿지 않습니다.
 * 모든 Pawn 공통 처리는 URetrievePawnExtensionComponent::FindPawnExtensionComponent(Pawn) 으로 통일합니다.
 */
UCLASS()
class RETRIEVE_API ARetrieveAlsCharacter : public AAlsCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARetrieveAlsCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const;
	URetrievePawnExtensionComponent* GetPawnExtensionComponent() const { return PawnExtensionComponent; }

	/** ASC 초기화 완료 시점에 호출되어 GAS 태그 → ALS 상태 매핑을 등록. 자식이 트리거. */
	virtual void OnAbilitySystemReady();

	/** 자동 매달리기 시도. 가능하면 true. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Action")
	bool TryMantle();

	/** 사망/낙사/Groggy 등 시체화 진입. 시뮬레이션 시작. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Action")
	void StartRagdoll();

	/** 시체화 종료 (부활/기상 등). ALS가 GetUp 몽타주 자동 재생. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Action")
	bool StopRagdoll();

	/**
	 * Roll 액션 잠금 시작 + 목표 Yaw 설정.
	 * ALS StartRollingImplementation과 동일 흐름:
	 *   - RollingState.TargetYawAngle 설정 (매 프레임 RefreshRollingPhysics가 이 값으로 회전 보간)
	 *   - SetRotationInstant로 ALS 일관 회전 (actor + LocomotionState)
	 *   - SetLocomotionAction(Rolling)으로 회전 갱신 잠금
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Action")
	void BeginRollLockoutTowardYaw(float TargetYawAngle);

	/** Roll 액션 잠금 해제. (몽타주 NotifyState End가 이미 처리하지만 안전망 용도) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Action")
	void EndRollLockout();
	
	/** 애니메이션으로 락온 타겟과의 시점이 틀어졌을때 다시 타겟 방향으로 맞춰주는 보간 */
	void TurnYawTowardActor(AActor* Target, float InterpSpeed);

	/** 다음 착지 1회의 낙법(ALS Rolling on Land)을 억제하도록 표시 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Action")
	void SetSuppressLandingRoll(bool bSuppress) { bSuppressLandingRoll = bSuppress; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void InitializeAbilitySystem();

	/**
	 * 자식이 SpringArm을 가지면 override해서 반환. 베이스가 매 프레임 캡슐 HalfHeight 변화량을
	 * SpringArm RelativeLocation.Z로 역보정하여 Crouch 시 카메라 world Z를 유지합니다.
	 * nullptr 반환 시(디폴트) 보정 안 함.
	 */
	virtual USpringArmComponent* GetCameraSpringArm() const { return nullptr; }

	/** ALS 데모 디버그 HUD(우측 상단 키 토글 위젯) 비활성화 */
	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override {}

	/**
	 * Settings DA를 인스턴스 사본으로 복제 후, 코드 측 강제 값을 적용.
	 * DA가 .gitignore로 팀 공유가 어려운 항목들을 코드에 박아 일관성 보장.
	 * 자식이 추가 override 시 Super 먼저 호출 후 자기 값 덮어쓰기.
	 */
	virtual void ApplyRetrieveSettingsOverrides();

	/** GAS 태그 → ALS 상태 자동 동기화 핸들러 (RegisterGameplayTagEvent에 바인딩) */
	virtual void OnSprintTagChanged(const FGameplayTag Tag, int32 NewCount);
	virtual void OnCrouchTagChanged(const FGameplayTag Tag, int32 NewCount);
	virtual void OnLockOnTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** ALS LocomotionAction 변화 → GAS State 태그 미러링 (Rolling→Dodging 등) */
	virtual void NotifyLocomotionActionChanged(FGameplayTag PreviousLocomotionAction) override;

	/** ALS 착지 낙법 판정 시점. bSuppressLandingRoll이 켜져 있으면 이 착지 1회의 낙법을 억제 */
	virtual void NotifyLocomotionModeChanged(FGameplayTag PreviousLocomotionMode) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Pawn")
	TObjectPtr<const URetrievePawnData> DefaultPawnData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrievePawnExtensionComponent> PawnExtensionComponent;

private:
	void RefreshSwimmingRotation(float DeltaTime);	
	
	/** PostInitializeComponents에서 캡슐 디폴트 HalfHeight 캐싱. Crouch 보정용. */
	float CachedDefaultHalfHeight = 0.f;
	/** 락온 타겟 유효하면 보간 진행 중 */
	TWeakObjectPtr<AActor> TurnTarget;
	float TurnInterpSpeed = 0.f;

	/** true면 다음 착지 1회의 낙법을 억제 (NotifyLocomotionModeChanged에서 소비). JumpAttack이 발동 시 설정 */
	bool bSuppressLandingRoll = false;
};
