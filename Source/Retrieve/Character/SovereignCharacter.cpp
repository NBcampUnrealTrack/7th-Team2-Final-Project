#include "Character/SovereignCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CombatReactionComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/RetrieveHeroComponent.h"
#include "Components/ElementGaugeComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/WeaponComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/RetrieveInputComponent.h"
#include "Player/RetrievePlayerState.h"

ASovereignCharacter::ASovereignCharacter(const FObjectInitializer& ObjectInitializer)
{
	OverrideInputComponentClass = URetrieveInputComponent::StaticClass();

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.f, 720.f, 0.f);
	MoveComp->JumpZVelocity = 600.f;
	MoveComp->AirControl = 0.35f;

	HeroComponent = CreateDefaultSubobject<URetrieveHeroComponent>(TEXT("HeroComponent"));
	CombatReactionComponent = CreateDefaultSubobject<UCombatReactionComponent>(TEXT("CombatReactionComponent"));
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
	ElementGaugeComponent = CreateDefaultSubobject<UElementGaugeComponent>(TEXT("ElementGaugeComponent"));
	
	CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
	CameraSpringArm->SetupAttachment(RootComponent);
	CameraSpringArm->TargetArmLength = 400.0f;
	CameraSpringArm->bUsePawnControlRotation = true;
	CameraSpringArm->bDoCollisionTest = true;
	CameraSpringArm->CameraLagSpeed = 10.f;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(CameraSpringArm, USpringArmComponent::SocketName);
}

void ASovereignCharacter::InitializeAbilitySystem()
{
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

	if (PawnExtensionComponent)
	{
		PawnExtensionComponent->InitializeAbilitySystem(ASC, RetrievePS);
		
		// 검/방패 장착 테스트용 코드
		if (HasAuthority() && WeaponComponent && !WeaponComponent->IsEquipped())
		{
			WeaponComponent->EquipWeapon(TEXT("Weapon_SwordShield_Basic"));
		}
	}
}
