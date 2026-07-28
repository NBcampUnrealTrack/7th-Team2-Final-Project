#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GCN_HitFlash.generated.h"

class UMaterialInterface;

/**
 * 피격 오버레이 플래시(타격감). 대상 메시에 FlashMaterial을 잠깐 오버레이로 씌웠다 원복. 코스메틱(MP 안전)
 */
UCLASS(Blueprintable)
class RETRIEVE_API UGCN_HitFlash : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/** 오버레이 플래시 머티리얼(흰색 emissive 권장). 미지정이면 no-op. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Flash")
	TSoftObjectPtr<UMaterialInterface> FlashMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Flash", meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float FlashDuration = 0.08f;
};
