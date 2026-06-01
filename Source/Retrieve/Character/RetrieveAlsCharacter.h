
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

	/** GAS 태그 → ALS 상태 자동 동기화 핸들러 (RegisterGameplayTagEvent에 바인딩) */
	virtual void OnSprintTagChanged(const FGameplayTag Tag, int32 NewCount);
	virtual void OnCrouchTagChanged(const FGameplayTag Tag, int32 NewCount);
	virtual void OnLockOnTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** ALS LocomotionAction 변화 → GAS State 태그 미러링 (Rolling→Dodging 등) */
	virtual void NotifyLocomotionActionChanged(FGameplayTag PreviousLocomotionAction) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Pawn")
	TObjectPtr<const URetrievePawnData> DefaultPawnData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrievePawnExtensionComponent> PawnExtensionComponent;

private:
	/** PostInitializeComponents에서 캡슐 디폴트 HalfHeight 캐싱. Crouch 보정용. */
	float CachedDefaultHalfHeight = 0.f;
};
