
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LockOnTargetHighlighter.generated.h"

/**
 * 락온 타겟의 메시 외곽선 표시를 책임지는 헬퍼 UObject
 * 
 * 메시의 CustomDepth Stencil 값을 토글해서 
 * PostProcess 머티리얼이 Stencil_ID == LockOnStencilValue인 픽셀에 외곽선을 그리도록 한다
 * UCombatReactionComponent가 멤버로 소유(추후 다른 상황에서도 사용시 서브시스템으로 승격)
 */
UCLASS()
class RETRIEVE_API ULockOnTargetHighlighter : public UObject
{
	GENERATED_BODY()
	
public:
	// 새 타겟에 외곽선 적용 이전 타겟이 있으면 자동 해제
	UFUNCTION(BlueprintCallable, Category = "Retrieve|LockOn|Highlight")
	void Apply(AActor* Target);
	// 현재 강조된 타겟의 외곽선 해제
	UFUNCTION(BlueprintCallable, Category = "Retrieve|LockOn|Highlight")
	void Clear();
	
protected:
	// 대상 액터의 모든 PrimitiveComponent에 CustomDepth 표시를 토글
	void ToggleHighlight(AActor* Target, bool bEnable);
	// 메시에 적용하는 CustomDepth Stencil 값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|LockOn|Highlight")
	int32 LockOnStencilValue = 3;
	// 현재 외곽선이 적용된 액터
	TWeakObjectPtr<AActor> CurrentHighlighted;
};
