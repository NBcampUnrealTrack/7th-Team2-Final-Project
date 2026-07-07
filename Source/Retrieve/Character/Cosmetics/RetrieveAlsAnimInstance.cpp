#include "Character/Cosmetics/RetrieveAlsAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/MeshComponent.h"
#include "Components/Player/WeaponComponent.h"
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

void URetrieveAlsAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	AActor* Owner = GetOwningActor();
	const UWeaponComponent* Weapon = IsValid(Owner) ? Owner->FindComponentByClass<UWeaponComponent>() : nullptr;
	const UMeshComponent* BowMesh = IsValid(Weapon) ? Weapon->GetPrimaryEquippedWeaponMesh() : nullptr;

	// 타겟 = 드로우 그립 소켓 월드(활 자기 애님이 당긴 위치를 그대로 추종). 활이 아니거나 소켓 없으면 갱신 안 함.
	if (IsValid(BowMesh) && !BowDrawGripSocket.IsNone() && BowMesh->DoesSocketExist(BowDrawGripSocket))
	{
		BowDrawHandTargetWorld = BowMesh->GetSocketTransform(BowDrawGripSocket, RTS_World);
	}

	// 알파 = Drawing(차징) 태그로 램프. 당김~홀드 ON, 발사 OFF. (비활/비활성 시 0)
	float TargetAlpha = 0.f;
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
	{
		TargetAlpha = ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_BowShot_Drawing) ? 1.f : 0.f;
	}
	BowDrawHandIkAlpha = FMath::FInterpTo(BowDrawHandIkAlpha, TargetAlpha, DeltaSeconds, BowDrawIkBlendSpeed);
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
