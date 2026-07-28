#include "Test/DebugAbilitySetActor.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

ADebugAbilitySetActor::ADebugAbilitySetActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	ASC = CreateDefaultSubobject<URetrieveAbilitySystemComponent>(TEXT("ASC"));
	ASC->SetIsReplicated(true);
	ASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

UAbilitySystemComponent* ADebugAbilitySetActor::GetAbilitySystemComponent() const
{
	return ASC;
}

void ADebugAbilitySetActor::TriggerDebugAbility()
{
	ASC->AbilityInputTagPressed(RetrieveGameplayTags::Ability_Player_Attack);
	ASC->ProcessAbilityInput(0.f, false);
	ASC->AbilityInputTagReleased(RetrieveGameplayTags::Ability_Player_Attack);
	ASC->ProcessAbilityInput(0.f, false);
}

void ADebugAbilitySetActor::ApplyTestDamage()
{
	if (!ASC || !TestDamageEffect)
	{
		return;
	}
	
	const UGameplayEffect* GE = TestDamageEffect->GetDefaultObject<UGameplayEffect>();
	if (GE)
	{
		ASC->ApplyGameplayEffectToSelf(GE, 1.f, ASC->MakeEffectContext());
	}
}

void ADebugAbilitySetActor::BeginPlay()
{
	Super::BeginPlay();

	ASC->InitAbilityActorInfo(this, this);
	
	if (HasAuthority() && AbilitySet)
	{
		AbilitySet->GiveToAbilitySystem(ASC, &GrantedHandles, this);
	}
}
