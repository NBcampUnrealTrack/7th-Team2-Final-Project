

#include "LockOnTargetHighlighter.h"

#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

void ULockOnTargetHighlighter::Apply(AActor* Target)
{
	// 같은 타겟이면 동작 안함
	if (CurrentHighlighted.Get() == Target)
	{
		return;
	}
	
	// 이전 타겟 OFF
	if (AActor* Prev = CurrentHighlighted.Get())
	{
		ToggleHighlight(Prev, false);
	}
	
	CurrentHighlighted = Target;
	
	if (IsValid(Target))
	{
		ToggleHighlight(Target, true);
	}
}

void ULockOnTargetHighlighter::Clear()
{
	if (AActor* Prev = CurrentHighlighted.Get())
	{
		ToggleHighlight(Prev, false);
	}
	CurrentHighlighted.Reset();
}

void ULockOnTargetHighlighter::ToggleHighlight(AActor* Target, bool bEnable)
{
	if (IsValid(Target) == false)
	{
		return;
	}
	
	TInlineComponentArray<UPrimitiveComponent*> Prims;
	Target->GetComponents<UPrimitiveComponent>(Prims);
	
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (IsValid(Prim) == false)
		{
			continue;
		}
		
		Prim->SetRenderCustomDepth(bEnable);
		if (bEnable)
		{
			Prim->SetCustomDepthStencilValue(LockOnStencilValue);
		}
	}
}
