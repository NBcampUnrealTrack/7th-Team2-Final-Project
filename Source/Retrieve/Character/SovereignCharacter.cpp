#include "Character/SovereignCharacter.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CombatReactionComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/RetrieveHealthComponent.h"
#include "Components/RetrieveHeroComponent.h"
#include "Components/ElementGaugeComponent.h"
#include "Components/ElementUnlockComponent.h"
#include "Components/PlayerBurstComponent.h"
#include "Components/RetrievePawnCosmeticComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WeaponComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Input/RetrieveInputComponent.h"
#include "Player/RetrievePlayerState.h"

ASovereignCharacter::ASovereignCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)   // 부모로 ObjectInitializer 전달 → SetDefaultSubobjectClass<URetrieveCharacterMovementComponent> 작동
{
	OverrideInputComponentClass = URetrieveInputComponent::StaticClass();

	// ALS 요구사항: 세 플래그 모두 false 유지
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// ALS가 매 프레임 SetActorRotation으로 회전을 직접 제어하므로
	// bOrientRotationToMovement / RotationRate 라인은 제거됨 (이전 의도는 GAS 태그로 처리됨).
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->JumpZVelocity = 600.f;
	MoveComp->AirControl = 0.35f;

	// 메인 메시: ALS 골격(SKM_Als). 비주얼은 VisualMesh가 담당하므로 숨김 + 본 갱신 강제.
	if (USkeletalMeshComponent* MainMesh = GetMesh())
	{
		MainMesh->SetHiddenInGame(true);
		MainMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// VisualMesh: 커스텀 스켈레톤 메시. Retarget Pose From Mesh ABP로 메인 메시 포즈를 따라감.
	// BP에서 SkeletalMesh 지정 + AnimClass에 리타겟 ABP 지정 필요.
	VisualMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(GetMesh());
	VisualMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	HeroComponent = CreateDefaultSubobject<URetrieveHeroComponent>(TEXT("HeroComponent"));
	CombatReactionComponent = CreateDefaultSubobject<UCombatReactionComponent>(TEXT("CombatReactionComponent"));
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
	ElementGaugeComponent = CreateDefaultSubobject<UElementGaugeComponent>(TEXT("ElementGaugeComponent"));
	PawnCosmeticComponent = CreateDefaultSubobject<URetrievePawnCosmeticComponent>(TEXT("PawnCosmeticComponent"));
	PlayerBurstComponent = CreateDefaultSubobject<UPlayerBurstComponent>(TEXT("PlayerBurstComponent"));
	ElementUnlockComponent = CreateDefaultSubobject<UElementUnlockComponent>(TEXT("ElementUnlockComponent"));
	
	CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
	CameraSpringArm->SetupAttachment(RootComponent);
	CameraSpringArm->TargetArmLength = 400.0f;
	CameraSpringArm->bUsePawnControlRotation = true;
	CameraSpringArm->bDoCollisionTest = false;
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
	
	if (PawnCosmeticComponent)
	{
		PawnCosmeticComponent->InitializeWithAbilitySystem(ASC);
	}

	if (ElementGaugeComponent)
	{
		ElementGaugeComponent->BindToASC();
	}

	if (ElementUnlockComponent)
	{
		ElementUnlockComponent->InitializeWithAbilitySystem(ASC);
	}
}

void ASovereignCharacter::UnPossessed()
{
	Super::UnPossessed();  // 부모의 PawnExtensionComponent 해제 포함
	
	if (PawnCosmeticComponent)
	{
		PawnCosmeticComponent->UninitializeFromAbilitySystem();
	}

	if (ElementUnlockComponent)
	{
		ElementUnlockComponent->UninitializeFromAbilitySystem();
	}
}

void ASovereignCharacter::HandleDeathStarted(AActor* OwningActor)
{
	Super::HandleDeathStarted(OwningActor); 
	
	if (URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent()) 
	{
		ASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Player_Dead);
	}
	
	if (!HasAuthority())
	{
		return;
	}
	
	const URetrieveHealthComponent* HC = GetHealthComponent();
	
	FPlayerDiedPayload Payload;
	Payload.DeadActor     = this;
	Payload.DeathLocation = GetActorLocation();
	Payload.Killer        = HC ? HC->LastDamageInstigator.Get() : nullptr;
	Payload.DamageCauser  = HC ? HC->LastDamageCauser.Get() : nullptr;
	
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		RetrieveGameplayTags::Channel_Player_Died, 
		Payload
	);
}
