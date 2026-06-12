

#include "RetrieveCameraBoom.h"

#include "AbilitySystemComponent.h"
#include "Character/RetrieveAlsCharacter.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void URetrieveCameraBoom::AddZoomInput(float AxisValue)
{
	// 락온 중 줌 인/아웃 방지(선택사항)
	// if (IsOwnerLockedOn())
	// {
	// 	return;
	// }
	if (DesiredArmLength < 0.f)
	{
		DesiredArmLength = TargetArmLength;
	}

	const float NewDesired = DesiredArmLength - AxisValue * ZoomStep;
	// 충돌로 당겨진 상태에서 줌아웃은 누적 금지
	if (IsCollisionFixApplied() && NewDesired > DesiredArmLength)
	{
		return;
	}
	DesiredArmLength = FMath::Clamp(NewDesired, MinArmLength, MaxArmLength);
}

void URetrieveCameraBoom::UpdateDesiredArmLocation(bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag,
                                                   float DeltaTime)
{
	// 줌: 목표 거리로 부드럽게
	if (DesiredArmLength >= 0.f)
	{
		TargetArmLength = FMath::FInterpTo(TargetArmLength, DesiredArmLength, DeltaTime, ZoomInterpSpeed);
	}

	// 맨틀 중 충돌 테스트 off: 캡슐이 턱 솔리드를 타고 올라 프로브가 붕괴 → 카메라 허리 관통 방지
	if (const ARetrieveAlsCharacter* OwnerChar = Cast<ARetrieveAlsCharacter>(GetOwner()))
	{
		if (OwnerChar->IsMantling())
		{
			bDoTrace = false;
		}
	}

	Super::UpdateDesiredArmLocation(bDoTrace, bDoLocationLag, bDoRotationLag, DeltaTime);
	// 붐에 붙은 자식 카메라 캐싱
	if (IsValid(ChildCamera) == false)
	{
		const TArray<USceneComponent*>& Kids = GetAttachChildren();
		ChildCamera = Kids.Num() > 0 ? Kids[0] : nullptr;
	}

	if (IsValid(ChildCamera) == false)
	{
		return;
	}
	// 락온 중엔 0으로 복귀(리그가 SocketOffset 관리), 아니면 충돌 시 숄더
	const FVector Desired = IsOwnerLockedOn()
		? FVector::ZeroVector
		: (IsCollisionFixApplied() ? ShoulderOffset : BaseOffset);

	// SocketOffset이 아니라 카메라 RelativeLocation에 적용 (프로브에 영향 X)
	ChildCamera->SetRelativeLocation(
		FMath::VInterpTo(ChildCamera->GetRelativeLocation(), Desired, DeltaTime, ShoulderBlendSpeed));
}

bool URetrieveCameraBoom::IsOwnerLockedOn() const
{
	const URetrievePawnExtensionComponent* PawnExt =
	   URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	const UAbilitySystemComponent* ASC =
		PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
	return ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::LockOn_Active);
}
