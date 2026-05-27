
#include "SovereignAnimInstance.h"

#include "AbilitySystemGlobals.h"
#include "Character/RetrieveCharacter.h"
#include "Components/RetrieveCharacterMovementComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

USovereignAnimInstance::USovereignAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USovereignAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	GameplayTagPropertyMap.Initialize(this, ASC);
}

#if WITH_EDITOR
EDataValidationResult USovereignAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	GameplayTagPropertyMap.IsDataValid(this, Context);

	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

void USovereignAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (AActor* OwningActor = GetOwningActor())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
		{
			InitializeWithAbilitySystem(ASC);
		}
	}
}

void USovereignAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ARetrieveCharacter* Character = Cast<ARetrieveCharacter>(GetOwningActor());
	if (!Character) { return; }

	URetrieveCharacterMovementComponent* MoveComp = Cast<URetrieveCharacterMovementComponent>(Character->GetCharacterMovement());
	if (!MoveComp)  { return; }
	const FRetrieveCharacterGroundInfo GroundInfo = MoveComp->GetGroundInfo();
	GroundDistance = GroundInfo.GroundDistance;
}
