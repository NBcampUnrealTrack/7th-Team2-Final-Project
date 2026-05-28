#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "RetrieveInteractionResponseComponent.generated.h"

class URetrieveInteractionResultAsset;
class URetrieveInteractionTypeAsset;
class URetrieveInteractionPresetAsset;
class URetrieveLootTableAsset;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRetrieveOnInteractionAppliedSignature, AActor*, InteractionInstigator);

/**
 * 상호작용 결과를 받아 적용하는 응답 컴포넌트.
 *
 * ─ 프리셋 기반 워크플로 (권장) ─────────────────────────────────────────────
 * 1. DA_Preset_PickupItem, DA_Preset_OpenChest 같은 URetrieveInteractionPresetAsset을
 *    Content Browser에 미리 만들어 둔다.
 * 2. 각 액터 BP에서 ResponseComponent.Preset 슬롯에 프리셋 1개만 할당.
 * 3. 조합(TypeAsset + ResultAssets)을 바꾸려면 프리셋 에셋 1개만 수정 →
 *    해당 프리셋을 참조하는 모든 액터에 즉시 반영.
 *
 * ─ 액터별 커스터마이징 ───────────────────────────────────────────────────────
 * TypeAssetOverride / ResultAssetsOverride 에 값을 넣으면 프리셋 값을 덮어쓴다.
 * 비워두면 Preset에서 읽은 값이 그대로 사용된다.
 *
 * ─ 유효 값 결정 우선순위 ─────────────────────────────────────────────────────
 *   GetEffectiveTypeAsset()    : TypeAssetOverride → Preset.TypeAsset → nullptr
 *   GetEffectiveResultAssets() : ResultAssetsOverride(비면 스킵) → Preset.ResultAssets → 빈 배열
 *
 * ─ 애니메이션 ────────────────────────────────────────────────────────────────
 * GetEffectiveTypeAsset().InteractionMontage가 있으면 instigator 캐릭터에 직접 재생.
 * 액터별 추가 처리가 필요하면 BP OnPlayInteractionAnim 이벤트 override.
 *
 * ─ 네트워크 ──────────────────────────────────────────────────────────────────
 * 컴포넌트 자체가 Replicated. 클라이언트 → Server_ApplyResult RPC → 서버 권한 처리.
 */
UCLASS(ClassGroup = (Retrieve), meta = (BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveInteractionResponseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URetrieveInteractionResponseComponent();

	// ── 프리셋 (공용 에셋, 권장) ───────────────────────────────────────────
	/**
	 * TypeAsset + ResultAssets 조합을 담은 공용 프리셋 에셋.
	 * 다수의 액터가 같은 프리셋을 공유하며, 프리셋 수정 시 모든 참조 액터에 즉시 반영된다.
	 *
	 * 추천 프리셋:
	 *   DA_Preset_PickupItem  — DA_IT_PickupItem  + [Pickup ResultAssets]
	 *   DA_Preset_OpenChest   — DA_IT_OpenChest   + [Loot ResultAssets]
	 *   DA_Preset_MineOre     — DA_IT_MineOre     + [Loot ResultAssets]
	 *   DA_Preset_LootEnemy   — DA_IT_LootEnemy   + [Loot ResultAssets]
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction")
	TObjectPtr<URetrieveInteractionPresetAsset> Preset;

	// ── 액터별 override (비어있으면 Preset 값 사용) ─────────────────────
	/**
	 * 이 액터만 다른 TypeAsset을 쓰고 싶을 때 설정.
	 * 설정하면 Preset.TypeAsset을 덮어쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction",
		meta = (DisplayName = "TypeAsset Override"))
	TObjectPtr<URetrieveInteractionTypeAsset> TypeAssetOverride;

	/**
	 * 이 액터만 다른 ResultAssets를 쓰고 싶을 때 설정.
	 * 배열이 비어 있지 않으면 Preset.ResultAssets를 완전히 대체한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction",
		meta = (DisplayName = "ResultAssets Override"))
	TArray<TObjectPtr<URetrieveInteractionResultAsset>> ResultAssetsOverride;

	// ── 단순 픽업 인라인 (DA 파일 불필요, 아이템 1종 고정) ───────────────
	/**
	 * ★ 레벨 디자이너용 — 아이템 1종을 바닥에 배치할 때 사용.
	 *
	 * DA 파일 없이 인스턴스마다 직접 설정하는 가장 빠른 방법.
	 * Preset.ResultAssets + ResultAssetsOverride가 모두 비어 있고,
	 * DirectLootTable도 없을 때 이 값이 사용된다.
	 *
	 * 설정 방법:
	 *   1. ItemId         → DataTable RowName (예: "sword_iron_01", "potion_heal_small")
	 *   2. ItemCategory   → Item.Weapon / Item.Consumable / Item.Material 중 선택
	 *   3. Quantity       → 획득 수량 (기본 1)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|★ 빠른 픽업 설정",
		meta = (DisplayName = "아이템 ID (RowName)"))
	FName QuickPickupItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|★ 빠른 픽업 설정",
		meta = (DisplayName = "아이템 카테고리", Categories = "Item"))
	FGameplayTag QuickPickupItemCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|★ 빠른 픽업 설정",
		meta = (DisplayName = "수량", ClampMin = "1"))
	int32 QuickPickupQuantity = 1;

	/**
	 * QuickPickupItemId가 설정된 단일 아이템 상호작용이면 Manager 프롬프트에
	 * 아이템 DataTable의 DisplayName / DT_ItemIcon 아이콘을 우선 적용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Prompt",
		meta = (DisplayName = "단일 아이템 프롬프트 자동 적용"))
	bool bUseQuickPickupItemPrompt = true;

	/**
	 * 단일 아이템 프롬프트 문구 포맷. {ItemName}, {ActionText}, {Quantity} 사용 가능.
	 * 예: "{ItemName}", "{ActionText} {ItemName}", "{ItemName} x{Quantity}"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Prompt",
		meta = (DisplayName = "아이템 프롬프트 텍스트 포맷"))
	FText QuickPickupPromptFormat = INVTEXT("{ItemName}");

	/** 이 액터만 프롬프트 텍스트를 직접 덮어쓰고 싶을 때 사용한다. 비어 있으면 자동 계산값을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Prompt",
		meta = (DisplayName = "프롬프트 텍스트 Override"))
	FText PromptTextOverride;

	/** 이 액터만 프롬프트 아이콘을 직접 덮어쓰고 싶을 때 사용한다. None이면 자동 계산값을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Prompt",
		meta = (DisplayName = "프롬프트 아이콘 Override"))
	TObjectPtr<UTexture2D> PromptIconOverride;

	/** 아이템이 아닌 문/들기/채집류 프롬프트에서는 TypeAsset 아이콘을 비우고 텍스트만 보이게 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Prompt",
		meta = (DisplayName = "비아이템 프롬프트 아이콘 숨김"))
	bool bHideIconForNonItemPrompt = true;

	// ── 확률 드롭 직접 참조 (상자·몬스터·채광) ────────────────────────────
	/**
	 * ★ 레벨 디자이너용 — 상자/몬스터/광맥에 드롭 테이블을 직접 연결.
	 *
	 * 동종 액터(예: 일반 상자 30개)가 모두 같은 LootTable을 공유하므로
	 * 드롭 내용을 바꾸려면 LootTable 에셋 1개만 수정하면 모두 반영된다.
	 * Preset.ResultAssets + ResultAssetsOverride가 비어 있을 때 사용된다.
	 * (QuickPickupItemId보다 우선순위가 높다)
	 *
	 * 설정 방법:
	 *   DirectLootTable 슬롯에 DA_LootTable_CommonChest 같은 에셋 드래그&드롭
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|★ 빠른 픽업 설정",
		meta = (DisplayName = "드롭 테이블 (LootTableAsset)"))
	TObjectPtr<URetrieveLootTableAsset> DirectLootTable;

	// ── 라이프사이클 ───────────────────────────────────────────────────────
	/** true면 결과 적용 직후 owner 액터 Destroy. 1회성 픽업에 사용 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction")
	bool bDestroyOwnerOnApplied = false;

	/** true면 결과 적용 시 화면 디버그 메시지 출력 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Debug")
	bool bShowDebugMessageOnApply = true;

	// ── 후처리 hook ──────────────────────────────────────────────────────
	/** 결과 적용 후 BP가 후처리(이펙트/사운드 등)할 수 있는 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Interaction")
	FRetrieveOnInteractionAppliedSignature OnApplied;

	// ── InteractionManager 자동 바인딩 ─────────────────────────────────────
	/**
	 * true면 BeginPlay에서 owner의 Manager_InteractionTarget(BP) 컴포넌트를 찾아
	 * OnInteractionEnd 멀티캐스트에 자동으로 우리 핸들러를 연결한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|AutoBind")
	bool bAutoBindInteractionManager = true;

	/**
	 * 자동 바인딩 대상 컴포넌트 이름.
	 * BP에서 Manager_InteractionTarget 컴포넌트를 부착할 때 사용한 변수 이름과 일치해야 함.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|AutoBind")
	FName InteractionManagerComponentName = TEXT("InteractionTarget");

	/**
	 * OnInteractionEnd Result 파라미터 중 "성공"으로 처리할 값.
	 * Manager_InteractionTarget 플러그인 기준: 0 = Success, 1 = Cancel, 2 = Fail.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|AutoBind")
	uint8 SuccessResultValue = 0;

	// ── 진입점 ────────────────────────────────────────────────────────────
	/**
	 * InteractionManager의 OnInteractionEnd 이벤트가 발동되면 자동으로 호출 (자동 바인딩 시).
	 * 또는 BP가 직접 호출할 수도 있음.
	 * 클라이언트에서 호출되면 자동으로 서버 RPC로 위임된다.
	 *
	 * @param InteractionInstigator 상호작용을 발동한 액터 (Manager_Interactor의 owner)
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Interaction")
	void HandleInteractionApplied(AActor* InteractionInstigator);

	/**
	 * 유효 TypeAsset을 반환한다.
	 * 우선순위: TypeAssetOverride → Preset.TypeAsset → nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Interaction")
	URetrieveInteractionTypeAsset* GetEffectiveTypeAsset() const;

	/**
	 * 유효 ResultAssets를 반환한다.
	 * 우선순위: ResultAssetsOverride(비어있지 않으면) → Preset.ResultAssets → 빈 배열
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Interaction")
	TArray<URetrieveInteractionResultAsset*> GetEffectiveResultAssets() const;

	/**
	 * 유효 TypeAsset의 설정을 Manager_InteractionTarget에 즉시 적용한다.
	 * BeginPlay에서 자동 호출되지만, BP Construction Script에서도 호출 가능.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Interaction")
	void ApplyTypeAssetToManager();

protected:
	virtual void BeginPlay() override;

	/**
	 * Manager_InteractionTarget.OnInteractionEnd 시그니처에 맞춘 자동 바인딩 핸들러.
	 * (byte Result, Pawn* InteractorPawn) — Reflection으로 자동 연결됨.
	 */
	UFUNCTION()
	void HandleInteractionManagerEnd(uint8 Result, APawn* InteractorPawn);

	/** owner에서 Manager_InteractionTarget 찾고 TypeAsset 적용 + OnInteractionEnd 바인딩 */
	void TryAutoBindInteractionManager();

	UFUNCTION(Server, Reliable)
	void Server_ApplyResult(AActor* InteractionInstigator);

	/** Authority에서 실제 결과 적용 */
	UFUNCTION()
	void ApplyResultAuthoritative(AActor* InteractionInstigator);

	/**
	 * 애니메이션 dispatch.
	 * GetEffectiveTypeAsset().InteractionMontage가 있으면 instigator 캐릭터에 직접 재생.
	 * BP override용 OnPlayInteractionAnim 이벤트도 호출된다.
	 */
	virtual void TryPlayInteractionAnim(AActor* InteractionInstigator);

	/** BP가 액터별 애니메이션을 override하고 싶을 때 사용 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Interaction|Animation")
	void OnPlayInteractionAnim(AActor* InteractionInstigator);

private:
	/** InteractionTypeAsset 설정을 ManagerComp에 reflection으로 적용하는 내부 구현 */
	void ApplyTypeAssetToManagerInternal(UActorComponent* ManagerComp);
};
