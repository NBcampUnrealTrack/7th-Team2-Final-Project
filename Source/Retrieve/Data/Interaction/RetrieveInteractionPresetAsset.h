#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RetrieveInteractionPresetAsset.generated.h"

class URetrieveInteractionTypeAsset;
class URetrieveInteractionResultAsset;

/**
 * 상호작용 유형 + 결과를 하나로 묶은 공용 프리셋 DataAsset.
 *
 * 문제 상황 (프리셋 이전):
 *   액터마다 InteractionTypeAsset + ResultAssets[] 을 각각 할당해야 함.
 *   "아이템 픽업" 액터가 10개면 같은 에셋을 10번 드래그해서 할당.
 *   TypeAsset이나 ResultAsset을 바꾸면 10개를 모두 열어서 수정.
 *
 * 프리셋 사용 후:
 *   DA_Preset_PickupItem, DA_Preset_OpenChest, DA_Preset_MineOre 처럼
 *   자주 쓰는 조합을 프리셋 에셋 하나에 저장.
 *   액터의 ResponseComponent.Preset에 프리셋 1개만 할당하면 끝.
 *   조합을 바꾸려면 프리셋 에셋 1개만 수정 → 모든 참조 액터에 즉시 반영.
 *
 * 액터별 커스터마이징이 필요할 때:
 *   ResponseComponent의 TypeAssetOverride / ResultAssetsOverride 에 값을 넣으면
 *   프리셋 값을 덮어쓸 수 있다. 비워두면 프리셋 값이 사용된다.
 *
 * 권장 프리셋 목록 (Content Browser에서 미리 생성해두기):
 *   DA_Preset_PickupItem   TypeAsset=DA_IT_PickupItem,  Results=[] (인라인 QuickPickupItemId 사용)
 *   DA_Preset_OpenChest    TypeAsset=DA_IT_OpenChest,   Results=[DA_PickupGroup_ChestReward]
 *   DA_Preset_MineOre      TypeAsset=DA_IT_MineOre,     Results=[] (인라인 DirectLootTable 사용)
 *   DA_Preset_LootEnemy    TypeAsset=DA_IT_LootEnemy,   Results=[] (인라인 DirectLootTable 사용)
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveInteractionPresetAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ── 상호작용 타입 ──────────────────────────────────────────────────────
	/**
	 * 상호작용 종류 설정 (프롬프트 텍스트 / Tap·Hold 모드 / Hold 지속 시간 / AnimMontage).
	 * 여러 프리셋이 같은 TypeAsset을 공유할 수 있다.
	 * 예: "아이템 획득" 텍스트를 바꾸면 해당 TypeAsset을 참조하는 모든 프리셋에 반영.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Preset")
	TObjectPtr<URetrieveInteractionTypeAsset> TypeAsset;

	// ── 결과 목록 ──────────────────────────────────────────────────────────
	/**
	 * 상호작용 성공 시 순서대로 적용될 결과 에셋 목록.
	 *
	 * ※ 단순 픽업·확률 드롭은 DA 없이 ResponseComponent 인라인 필드로 처리한다.
	 *   - 1종 아이템 픽업  → ResponseComponent.QuickPickupItemId + Quantity
	 *   - 확률 드롭        → ResponseComponent.DirectLootTable (LootTableAsset 직접 참조)
	 *
	 * ResultAssets 사용이 적합한 경우:
	 *   URetrievePickupGroupResultAsset  : 여러 아이템 고정 묶음 (상자·퀘스트 보상 등)
	 *   URetrieveCompositeResultAsset    : 복수 결과 유형의 중첩 조합
	 *   URetrieveCustomEventResultAsset  : BP override 자유 결과 (문·레버·대화 등)
	 *
	 * 공용 에셋을 참조하는 방식이므로 Actor당 새 인스턴스를 만들 필요가 없다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Preset")
	TArray<TObjectPtr<URetrieveInteractionResultAsset>> ResultAssets;
};
