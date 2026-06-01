#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveAlsCombatCharacter.h"
#include "RetrieveAlsTestCharacter.generated.h"

class UCameraComponent;
class URetrieveHeroComponent;
class USkeletalMeshComponent;
class USpringArmComponent;

/**
 * ALS 채택 검증용 플레이어. Sovereign 패턴의 최소 형태.
 * - 메인 메시(GetMesh()) = ALS 골격(SKM_Als), 숨김 + AlwaysTickPoseAndRefreshBones
 * - VisualMesh = 커스텀 메시, 메인을 Retarget Pose From Mesh ABP로 따라감 (ABP는 BP에서 지정)
 */
UCLASS()
class RETRIEVE_API ARetrieveAlsTestCharacter : public ARetrieveAlsCombatCharacter
{
	GENERATED_BODY()

public:
	ARetrieveAlsTestCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void InitializeAbilitySystem() override;

	virtual USpringArmComponent* GetCameraSpringArm() const override { return CameraSpringArm; }

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<URetrieveHeroComponent> HeroComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<USkeletalMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Camera")
	TObjectPtr<USpringArmComponent> CameraSpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;
};
