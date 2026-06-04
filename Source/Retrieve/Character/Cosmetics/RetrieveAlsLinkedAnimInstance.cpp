#include "Character/Cosmetics/RetrieveAlsLinkedAnimInstance.h"

#include "Character/Cosmetics/RetrieveAlsAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"


void URetrieveAlsLinkedAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// ALS base가 같은 SkelMesh의 main AnimInstance를 Parent로 캐시함.
	// 동일 인스턴스를 Retrieve 타입으로 재캐스트하여 RetrieveParent에 보관.
	RetrieveParent = Cast<URetrieveAlsAnimInstance>(GetSkelMeshComponent()->GetAnimInstance());

#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (IsValid(World) && !World->IsGameWorld())
	{
		// 에디터 프리뷰에서 RetrieveParent가 없으면 CDO로 폴백 (ALS base와 동일 패턴)
		if (!RetrieveParent.IsValid())
		{
			RetrieveParent = GetMutableDefault<URetrieveAlsAnimInstance>();
		}
	}
#endif
}

URetrieveAlsAnimInstance* URetrieveAlsLinkedAnimInstance::GetRetrieveParent() const
{
	return RetrieveParent.Get();
}
