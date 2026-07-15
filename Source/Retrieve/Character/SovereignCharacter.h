#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveAlsCombatCharacter.h"
#include "GenericTeamAgentInterface.h"
#include "Data/RetrieveDataTableTypes.h"
#include "SovereignCharacter.generated.h"

class USwimDetectionComponent;
class URetrievePawnCosmeticComponent;
class UArmorComponent;
class UCameraComponent;
class UInventoryComponent;
class URetrieveHeroComponent;
class USkeletalMeshComponent;
class UCombatReactionComponent;
class USpringArmComponent;
class UWeaponComponent;
class UElementGaugeComponent;
class UElementResonanceComponent;
class UStaminaComponent;
class UPlayerBurstComponent;
class URetrieveBuffUIBroadcastComponent;
class UElementUnlockComponent;
class UMotionWarpingComponent;
class URetrieveCameraWaterProbeComponent;
class UCombatStanceComponent;
class UCounterTimeDilationComponent;
class UNavigationInvokerComponent;

UCLASS()
class RETRIEVE_API ASovereignCharacter : public ARetrieveAlsCombatCharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ASovereignCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual FGenericTeamId GetGenericTeamId() const override
	{
		return FGenericTeamId(static_cast<uint8>(Team));
	}

protected:
	virtual void InitializeAbilitySystem() override;
	virtual void UnPossessed() override;
	virtual void HandleDeathStarted(AActor* OwningActor) override;
	virtual void Revive(const FTransform& RespawnTransform) override;

	virtual USpringArmComponent* GetCameraSpringArm() const override { return CameraSpringArm; }

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<URetrieveHeroComponent> HeroComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<UCombatReactionComponent> CombatReactionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<URetrievePawnCosmeticComponent> PawnCosmeticComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<UCombatStanceComponent> CombatStanceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UArmorComponent> ArmorComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<UElementGaugeComponent> ElementGaugeComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<UElementResonanceComponent> ElementResonanceComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<UStaminaComponent> StaminaComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<UPlayerBurstComponent> PlayerBurstComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<UElementUnlockComponent> ElementUnlockComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|MotionWarping")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<URetrieveBuffUIBroadcastComponent> BuffUIBroadcastComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<USwimDetectionComponent> SwimDetectionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<UCounterTimeDilationComponent> CounterTimeDilationComponent;

	/** 플레이어 주변에서만 navmesh 타일이 생성/제거되도록 하는 내비게이션 인보커. */
	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Navigation")
	TObjectPtr<UNavigationInvokerComponent> NavigationInvoker;

	/** 커스텀 스켈레톤 비주얼. 메인 메시(ALS 골격)의 포즈를 Retarget Pose From Mesh ABP로 따라감. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Retrieve|Components")
	TObjectPtr<USkeletalMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Camera")
	TObjectPtr<USpringArmComponent> CameraSpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Camera")
	TObjectPtr<URetrieveCameraWaterProbeComponent> CameraWaterProbe;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Team")
	ERetrieveTeam Team = ERetrieveTeam::Player;
};
