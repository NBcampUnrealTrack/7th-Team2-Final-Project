#include "Character/RetrieveCombatCharacter.h"

#include "Components/RetrieveHealthComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"

ARetrieveCombatCharacter::ARetrieveCombatCharacter(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer)
{
	HealthComponent = CreateDefaultSubobject<URetrieveHealthComponent>(TEXT("HealthComponent"));
}

void ARetrieveCombatCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.AddDynamic(this, &ARetrieveCombatCharacter::HandleDeathStarted);
	}

	if (URetrievePawnExtensionComponent* PawnExt = GetPawnExtensionComponent())
	{
		PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(
				this, &ARetrieveCombatCharacter::HandleAbilitySystemInitialized));
	}
}

void ARetrieveCombatCharacter::HandleAbilitySystemInitialized()
{
	URetrievePawnExtensionComponent* PawnExt = GetPawnExtensionComponent();
	if (PawnExt && HealthComponent)
	{
		HealthComponent->InitializeWithAbilitySystem(PawnExt->GetRetrieveAbilitySystemComponent());
	}
}

void ARetrieveCombatCharacter::HandleDeathStarted(AActor* OwningActor)
{
	// 기본 구현은 비어 있음. 아키타입(소버린/적/보스) 서브클래스가 오버라이드하여 사망 GA 활성화 및 GameplayEvent.*.Die 전송을 수행합니다.
}
