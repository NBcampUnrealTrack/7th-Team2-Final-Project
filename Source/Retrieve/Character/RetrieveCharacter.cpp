#include "Character/RetrieveCharacter.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/RetrieveHeroComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"

ARetrieveCharacter::ARetrieveCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PawnExtensionComponent = CreateDefaultSubobject<URetrievePawnExtensionComponent>(TEXT("PawnExtensionComponent"));
}

UAbilitySystemComponent* ARetrieveCharacter::GetAbilitySystemComponent() const
{
	return GetRetrieveAbilitySystemComponent();
}

URetrieveAbilitySystemComponent* ARetrieveCharacter::GetRetrieveAbilitySystemComponent() const
{
	return PawnExtensionComponent ? PawnExtensionComponent->GetRetrieveAbilitySystemComponent() : nullptr;
}

void ARetrieveCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (PawnExtensionComponent)
	{
		if (DefaultPawnData)
		{
			PawnExtensionComponent->SetPawnData(DefaultPawnData);
			PawnExtensionComponent->HandleControllerChanged();
		}
		InitializeAbilitySystem();
	}
}

void ARetrieveCharacter::UnPossessed()
{
	Super::UnPossessed();
	if (PawnExtensionComponent)
	{
		PawnExtensionComponent->HandleControllerChanged();
		PawnExtensionComponent->UninitializeAbilitySystem();
	}
}

void ARetrieveCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	if (PawnExtensionComponent)
	{
		PawnExtensionComponent->HandleControllerChanged();
	}
}

void ARetrieveCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystem();
	if (PawnExtensionComponent)
	{
		PawnExtensionComponent->HandlePlayerStateReplicated();
	}
}

void ARetrieveCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (URetrieveHeroComponent* HeroComponent = URetrieveHeroComponent::FindHeroComponent(this))
	{
		HeroComponent->InitializePlayerInput(PlayerInputComponent);
	}
	
	if (PawnExtensionComponent)
	{
		PawnExtensionComponent->SetupPlayerInputComponent();
	}
}

void ARetrieveCharacter::InitializeAbilitySystem()
{
	// 베이스는 아무것도 하지 않음. 소버린은 PlayerState의 ASC를, AI는 Pawn 소유의 ASC를 제공함.
}
