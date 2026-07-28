#include "Character/Cosmetics/RetrieveSwimmingLinkedAnimInstance.h"

#include "Character/Cosmetics/RetrieveAlsAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void URetrieveSwimmingLinkedAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	RetrieveParent = Cast<URetrieveAlsAnimInstance>(GetSkelMeshComponent()->GetAnimInstance());

#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (IsValid(World) && !World->IsGameWorld() && !RetrieveParent.IsValid())
	{
		RetrieveParent = GetMutableDefault<URetrieveAlsAnimInstance>();
	}
#endif
}

URetrieveAlsAnimInstance* URetrieveSwimmingLinkedAnimInstance::GetRetrieveParent() const
{
	return RetrieveParent.Get();
}
