
#include "SovereignAnimInstance.h"

#include "AbilitySystemGlobals.h"
#include "Components/Pawn/RetrieveCharacterMovementComponent.h"
#include "GameFramework/Character.h"

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

	// ACharacter::GetCharacterMovement()로 직접 접근 — Sovereign이 ALS 가지로 옮겨가도 작동
	const ACharacter* Character = Cast<ACharacter>(GetOwningActor());
	if (!Character) { return; }

	URetrieveCharacterMovementComponent* MoveComp = Cast<URetrieveCharacterMovementComponent>(Character->GetCharacterMovement());
	if (!MoveComp)  { return; }
	const FRetrieveCharacterGroundInfo GroundInfo = MoveComp->GetGroundInfo();
	GroundDistance = GroundInfo.GroundDistance;
}
