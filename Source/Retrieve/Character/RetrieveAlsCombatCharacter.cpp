
#include "RetrieveAlsCombatCharacter.h"

#include "Components/RetrieveHealthComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ARetrieveAlsCombatCharacter::ARetrieveAlsCombatCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HealthComponent = CreateDefaultSubobject<URetrieveHealthComponent>(TEXT("HealthComponent"));
}

void ARetrieveAlsCombatCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.AddDynamic(this, &ARetrieveAlsCombatCharacter::HandleDeathStarted);
	}
	
	if (URetrievePawnExtensionComponent* PawnExt = GetPawnExtensionComponent())
	{
		PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this,
				&ARetrieveAlsCombatCharacter::HandleAbilitySystemInitialized));	
	}
}

void ARetrieveAlsCombatCharacter::HandleAbilitySystemInitialized()
{
	URetrievePawnExtensionComponent* PawnExt = GetPawnExtensionComponent();

	if (PawnExt && HealthComponent)
	{
		HealthComponent->InitializeWithAbilitySystem(PawnExt->GetRetrieveAbilitySystemComponent());
	}

	// 베이스의 GAS 태그 → ALS 상태 매핑 등록 (Sprint / Crouch / LockOn 등)
	OnAbilitySystemReady();
}

void ARetrieveAlsCombatCharacter::HandleDeathStarted(AActor* OwningActor)
{
	// 기본 구현: 이동 정지. 아키타입 서브클래스(Sovereign 등)가 사망 GA 활성화/GameplayEvent.*.Die 전송을 추가합니다.
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	// ALS 사망 레그돌 (Phase B에서 결정한 사항: 사망 시에만 발동)
	StartRagdolling();
}
