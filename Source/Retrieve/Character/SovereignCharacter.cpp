#include "Character/SovereignCharacter.h"

#include "MotionWarpingComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Audio/RetrieveMusicSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/Player/ArmorComponent.h"
#include "Components/Combat/CombatReactionComponent.h"
#include "Components/Combat/CombatStanceComponent.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Components/Player/RetrieveHeroComponent.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "Components/Element/ElementUnlockComponent.h"
#include "Components/Player/StaminaComponent.h"
#include "Components/Player/PlayerBurstComponent.h"
#include "Components/Water/RetrieveCameraWaterProbeComponent.h"
#include "Components/Pawn/RetrievePawnCosmeticComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Water/SwimDetectionComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Components/Pawn/RetrieveCameraBoom.h"
#include "Components/Player/CounterTimeDilationComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Input/RetrieveInputComponent.h"
#include "Player/RetrievePlayerState.h"
#include "UI/HUD/RetrieveBuffUIBroadcastComponent.h"

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

	// 메인 메시 = 가시 leader. 가시성은 PawnCosmeticComponent::ApplyVisualLayout이 모듈러 바디 유무로 제어.
	if (USkeletalMeshComponent* MainMesh = GetMesh())
	{
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
	ArmorComponent = CreateDefaultSubobject<UArmorComponent>(TEXT("ArmorComponent"));
	ElementGaugeComponent = CreateDefaultSubobject<UElementGaugeComponent>(TEXT("ElementGaugeComponent"));
	StaminaComponent = CreateDefaultSubobject<UStaminaComponent>(TEXT("StaminaComponent"));
	PawnCosmeticComponent = CreateDefaultSubobject<URetrievePawnCosmeticComponent>(TEXT("PawnCosmeticComponent"));
	CombatStanceComponent = CreateDefaultSubobject<UCombatStanceComponent>(TEXT("CombatStanceComponent"));
	PlayerBurstComponent = CreateDefaultSubobject<UPlayerBurstComponent>(TEXT("PlayerBurstComponent"));
	ElementUnlockComponent = CreateDefaultSubobject<UElementUnlockComponent>(TEXT("ElementUnlockComponent"));
	BuffUIBroadcastComponent = CreateDefaultSubobject<URetrieveBuffUIBroadcastComponent>(TEXT("BuffUIBroadcastComponent"));
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
	SwimDetectionComponent = CreateDefaultSubobject<USwimDetectionComponent>(TEXT("SwimDetectionComponent"));
	CounterTimeDilationComponent = CreateDefaultSubobject<UCounterTimeDilationComponent>(TEXT("CounterTimeDilationComponent"));

	CameraSpringArm = CreateDefaultSubobject<URetrieveCameraBoom>(TEXT("CameraSpringArm"));
	CameraSpringArm->SetupAttachment(RootComponent);
	CameraSpringArm->TargetArmLength = 400.0f;
	CameraSpringArm->bUsePawnControlRotation = true;
	CameraSpringArm->bDoCollisionTest = true;
	CameraSpringArm->ProbeSize = 8.0f;
	CameraSpringArm->bEnableCameraLag = true;
	CameraSpringArm->CameraLagSpeed = 10.f;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(CameraSpringArm, USpringArmComponent::SocketName);

	CameraWaterProbe = CreateDefaultSubobject<URetrieveCameraWaterProbeComponent>(TEXT("CameraWaterProbe"));
	CameraWaterProbe->SetupAttachment(ThirdPersonCamera);
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
		// if (HasAuthority() && WeaponComponent && !WeaponComponent->IsEquipped())
		// {
		// 	WeaponComponent->EquipWeapon(TEXT("Weapon_SwordShield_Basic"));
		// }
	}
	
	if (PawnCosmeticComponent)
	{
		PawnCosmeticComponent->InitializeWithAbilitySystem(ASC);
	}

	// 스탠스는 cosmetic 이후 — 무기 레이어 relink가 끝난 뒤 초기 납검 소켓을 맞춘다.
	if (CombatStanceComponent)
	{
		CombatStanceComponent->InitializeWithAbilitySystem(ASC);
	}

	// BGM은 로컬 스피커에서만 재생해야 하므로 원격 프록시는 등록하지 않는다.
	if (IsLocallyControlled())
	{
		if (URetrieveMusicSubsystem* Music = GetWorld()->GetSubsystem<URetrieveMusicSubsystem>())
		{
			Music->RegisterPlayer(this);
		}
	}

	// 투구 장착 테스트용 코드 (suppression 검증) — cosmetic 초기화 이후에 호출해야 한다.
	// if (HasAuthority() && ArmorComponent)
	// {
	// 	ArmorComponent->EquipArmor(RetrieveGameplayTags::Equipment_Slot_Head, TEXT("Helmet_Test"));
	// }

	if (ElementGaugeComponent)
	{
		ElementGaugeComponent->BindToASC();
	}

	if (StaminaComponent)
	{
		StaminaComponent->InitializeWithAbilitySystem(ASC);
	}

	if (ElementUnlockComponent)
	{
		ElementUnlockComponent->InitializeWithAbilitySystem(ASC);
	}

	if (BuffUIBroadcastComponent)
	{
		BuffUIBroadcastComponent->RefreshAbilitySystemBinding();
	}
}

void ASovereignCharacter::UnPossessed()
{
	Super::UnPossessed();  // 부모의 PawnExtensionComponent 해제 포함
	
	if (PawnCosmeticComponent)
	{
		PawnCosmeticComponent->UninitializeFromAbilitySystem();
	}

	if (CombatStanceComponent)
	{
		CombatStanceComponent->UninitializeFromAbilitySystem();
	}

	if (ElementUnlockComponent)
	{
		ElementUnlockComponent->UninitializeFromAbilitySystem();
	}

	if (StaminaComponent)
	{
		StaminaComponent->UninitializeFromAbilitySystem();
	}
}

void ASovereignCharacter::HandleDeathStarted(AActor* OwningActor)
{
	Super::HandleDeathStarted(OwningActor); 
	
	URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (ASC)
	{
		ASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Player_Dead);
	}
	
	if (!HasAuthority())
	{
		return;
	}

	// GA_Die 활성화는 RetrieveHealthComponent::HandleHealthChanged가 단일 소스로 처리한다
	// (OnDeathStarted Broadcast 직전에 TryActivateAbilitiesByTag(Die) 수행 → 여기 도달 시 이미 발동됨).
	// 여기서 다시 부르면 이중 활성화 시도가 되므로 호출하지 않는다.
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

void ASovereignCharacter::Revive(const FTransform& RespawnTransform)
{
	Super::Revive(RespawnTransform);

	if (ElementGaugeComponent)
	{
		ElementGaugeComponent->ClearSlot();
	}
	if (StaminaComponent)
	{
		StaminaComponent->ResetStamina();
	}

	if (URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(RetrieveGameplayTags::State_Player_Dead);
	}
}
