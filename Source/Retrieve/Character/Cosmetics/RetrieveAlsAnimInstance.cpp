#include "Character/Cosmetics/RetrieveAlsAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayTags/RetrieveGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif


void URetrieveAlsAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	// PropertyMap은 내부에서 델리게이트를 안전하게 관리. 재호출도 무해 (Lyra 기준).
	CombatTagMap.Initialize(this, ASC);
}

void URetrieveAlsAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (AActor* OwningActor = GetOwningActor())
	{
		if (UAbilitySystemComponent* ASC =
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
		{
			InitializeWithAbilitySystem(ASC);
		}
		// ASC가 아직 없으면 (PS 리플리케이션 지연 등) 경로 B에서 처리됨.
	}

	if (!WeaponTypeTag.IsValid())
	{
		SetWeaponTypeTag(RetrieveGameplayTags::Weapon_Type_Unarmed);
	}
}

void URetrieveAlsAnimInstance::SetWeaponTypeTag(const FGameplayTag& NewWeaponTypeTag)
{
	WeaponTypeTag = NewWeaponTypeTag.IsValid()
		? NewWeaponTypeTag
		: RetrieveGameplayTags::Weapon_Type_Unarmed;
}

#if WITH_EDITOR
EDataValidationResult URetrieveAlsAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	CombatTagMap.IsDataValid(this, Context);

	return Context.GetNumErrors() > 0
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif
