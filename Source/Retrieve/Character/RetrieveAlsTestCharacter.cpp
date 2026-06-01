#include "Character/RetrieveAlsTestCharacter.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/RetrieveHeroComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/RetrieveInputComponent.h"
#include "Player/RetrievePlayerState.h"

ARetrieveAlsTestCharacter::ARetrieveAlsTestCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// HeroComponent의 GAS 입력 라우팅을 위해 RetrieveInputComponent 사용
	OverrideInputComponentClass = URetrieveInputComponent::StaticClass();

	// ALS가 매 프레임 SetActorRotation으로 회전을 직접 제어합니다.
	// 따라서 Sovereign의 bOrientRotationToMovement / RotationRate / bUseControllerRotation* 라인은
	// 의도적으로 생략합니다 (ALS와 충돌하는 라인입니다).

	// 메인 메시: ALS 골격(SKM_Als). 비주얼은 VisualMesh가 담당하므로 숨김 + 본 갱신 강제.
	if (USkeletalMeshComponent* MainMesh = GetMesh())
	{
		MainMesh->SetHiddenInGame(true);
		MainMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// VisualMesh: 커스텀 메시. Retarget Pose From Mesh ABP로 메인 메시 포즈를 따라감.
	// BP에서 SkeletalMesh 지정 + AnimClass에 리타겟 ABP 지정 필요.
	VisualMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(GetMesh());
	VisualMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	HeroComponent = CreateDefaultSubobject<URetrieveHeroComponent>(TEXT("HeroComponent"));

	CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
	CameraSpringArm->SetupAttachment(RootComponent);
	CameraSpringArm->TargetArmLength = 400.0f;
	CameraSpringArm->bUsePawnControlRotation = true;
	CameraSpringArm->bDoCollisionTest = true;
	// SpringArm은 actor(=캡슐 origin) 기준 고정. Crouch/Roll로 캡슐 HalfHeight가 변해도
	// actor location은 유지되므로 카메라 위치는 변하지 않습니다.

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(CameraSpringArm, USpringArmComponent::SocketName);
}

void ARetrieveAlsTestCharacter::InitializeAbilitySystem()
{
	// Sovereign 패턴 그대로: PlayerState 소유 ASC → PawnExt 위임.
	ARetrievePlayerState* RetrievePS = GetPlayerState<ARetrievePlayerState>();
	if (!RetrievePS)
	{
		return;
	}

	URetrieveAbilitySystemComponent* ASC = RetrievePS->GetRetrieveAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (URetrievePawnExtensionComponent* PawnExt = GetPawnExtensionComponent())
	{
		PawnExt->InitializeAbilitySystem(ASC, RetrievePS);
	}
}
