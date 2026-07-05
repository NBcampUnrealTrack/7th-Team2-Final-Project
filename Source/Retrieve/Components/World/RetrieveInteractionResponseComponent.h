#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RetrieveInteractionResponseComponent.generated.h"

class URetrieveInteractionResultAsset;
class URetrieveInteractionPresetProfileAsset;
class URetrieveInteractionTypeAsset;
class URetrieveInteractionPresetAsset;
class URetrieveLootTableAsset;
class UAnimMontage;
class UTexture2D;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRetrieveOnInteractionAppliedSignature, AActor*, InteractionInstigator);

/** bReopenAfterRests 대기가 끝나 다시 상호작용 가능해졌을 때 브로드캐스트. BP에서 뚜껑 닫힘 등 비주얼 복원에 사용. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRetrieveOnInteractionReopenedSignature);

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
	TObjectPtr<URetrieveInteractionPresetProfileAsset> PresetProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction")
	FName PresetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Legacy",
		meta = (DeprecatedProperty, DeprecationMessage = "Use PresetProfile + PresetId."))
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
	 * 이 액터만 다른 몽타주를 쓰고 싶을 때 설정.
	 * TypeAsset 전체를 복사하지 않고 몽타주만 교체할 때 사용.
	 * 설정하면 TypeAsset.InteractionMontage를 덮어쓴다.
	 *
	 * 우선순위: MontageOverride → TypeAssetOverride.Montage → Preset.TypeAsset.Montage
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction",
		meta = (DisplayName = "Montage Override"))
	TObjectPtr<UAnimMontage> MontageOverride;

	/**
	 * 비주얼 메시(Synty 등 독립 AnimInstance) 전용 몽타주 액터별 override.
	 * 설정하면 Preset.VisualMeshMontage를 덮어쓴다.
	 *
	 * 우선순위: VisualMeshMontageOverride → Preset.VisualMeshMontage
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction",
		meta = (DisplayName = "Visual Mesh Montage Override"))
	TObjectPtr<UAnimMontage> VisualMeshMontageOverride;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Prompt",
		meta = (DisplayName = "프롬프트 강조색 Override 사용"))
	bool bOverridePromptAccentColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Prompt",
		meta = (DisplayName = "프롬프트 강조색 Override", EditCondition = "bOverridePromptAccentColor"))
	FLinearColor PromptAccentColorOverride = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Hold",
		meta = (DisplayName = "Hold 설정 Override 사용"))
	bool bOverrideHoldSettings = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Hold",
		meta = (DisplayName = "Hold 방식 Override", EditCondition = "bOverrideHoldSettings"))
	bool bHoldInteractionOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Hold",
		meta = (DisplayName = "Hold 지속 시간 Override", EditCondition = "bOverrideHoldSettings && bHoldInteractionOverride", ClampMin = "0.05"))
	float HoldDurationOverride = 1.0f;

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

	// ── 휴식 기반 재오픈 (상자 등 순환 재사용 오브젝트용) ─────────────────────
	/** true면 결과 적용 후 즉시 재적용을 막고, 모닥불 휴식 RestsUntilReopen회 후 다시 상호작용 가능해진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Respawn")
	bool bReopenAfterRests = false;

	/** bReopenAfterRests가 true일 때, 몇 번의 모닥불 휴식 후 다시 열 수 있는 상태로 돌아가는지. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Respawn",
		meta = (EditCondition = "bReopenAfterRests", ClampMin = "1"))
	int32 RestsUntilReopen = 3;

	/** 현재 결과 재적용 대기 중(이미 열려서 비활성)인지. 런타임 상태. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Interaction|Respawn")
	bool bIsDepleted = false;

	/**
	 * bReopenAfterRests가 true일 때 Manager_InteractionTarget에 강제 적용할 FinishMethod 값.
	 * 기본 3 = "Reactivate After Duration On Completed" — Destroy/Deactivate 계열이 설정돼 있어도
	 * 액터가 파괴/영구비활성되지 않고 항상 재상호작용 가능하도록 강제한다.
	 * 실제 재사용 가능 여부(로직상 게이트)는 bIsDepleted가 담당한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Respawn",
		meta = (EditCondition = "bReopenAfterRests"))
	uint8 ReopenFinishMethodValue = 3;

	/** true면 결과 적용 시 화면 디버그 메시지 출력 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Interaction|Debug")
	bool bShowDebugMessageOnApply = false;

	// ── 후처리 hook ──────────────────────────────────────────────────────
	/** 결과 적용 후 BP가 후처리(이펙트/사운드 등)할 수 있는 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Interaction")
	FRetrieveOnInteractionAppliedSignature OnApplied;

	/** bReopenAfterRests 대기(휴식 카운트)가 끝나 다시 상호작용 가능해졌을 때 브로드캐스트 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Interaction|Respawn")
	FRetrieveOnInteractionReopenedSignature OnReopened;

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
	 * 재생할 AnimMontage를 결정해 반환한다.
	 * 우선순위: MontageOverride → TypeAssetOverride.InteractionMontage → Preset.TypeAsset.InteractionMontage → nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Interaction")
	UAnimMontage* GetEffectiveMontage() const;

	/**
	 * 비주얼 메시용 몽타주를 결정해 반환한다.
	 * 우선순위: VisualMeshMontageOverride → Preset.VisualMeshMontage → nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Interaction")
	UAnimMontage* GetEffectiveVisualMeshMontage() const;

	/** Returns the expected montage play time after applying the configured play rate. Both montages considered — returns the longer duration. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Interaction")
	float GetEffectiveInteractionAnimationDuration() const;

	/**
	 * 유효 TypeAsset의 설정을 Manager_InteractionTarget에 즉시 적용한다.
	 * BeginPlay에서 자동 호출되지만, BP Construction Script에서도 호출 가능.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Interaction")
	void ApplyTypeAssetToManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Manager_InteractionTarget.OnInteractionBegin 자동 바인딩 핸들러.
	 * (Pawn* InteractorPawn) — Reflection으로 자동 연결됨.
	 */
	UFUNCTION()
	void HandleInteractionManagerBegin(APawn* InteractorPawn);

	/**
	 * Manager_InteractionTarget.OnInteractionUpdated 자동 바인딩 핸들러.
	 * (Pawn* InteractorPawn, float Progress) — Reflection으로 자동 연결됨.
	 */
	UFUNCTION()
	void HandleInteractionManagerUpdated(APawn* InteractorPawn, float Progress);

	/**
	 * Manager_InteractionTarget.OnInteractionEnd 시그니처에 맞춘 자동 바인딩 핸들러.
	 * (byte Result, Pawn* InteractorPawn) — Reflection으로 자동 연결됨.
	 */
	UFUNCTION()
	void HandleInteractionManagerEnd(uint8 Result, APawn* InteractorPawn);

	/** owner에서 Manager_InteractionTarget 찾고 TypeAsset 적용 + OnInteraction* 바인딩 */
	void TryAutoBindInteractionManager();

	UFUNCTION(Server, Reliable)
	void Server_ApplyResult(AActor* InteractionInstigator);

	/** Authority에서 실제 결과 적용 */
	UFUNCTION()
	void ApplyResultAuthoritative(AActor* InteractionInstigator);

	/**
	 * 애니메이션 dispatch.
	 * GetEffectiveMontage()로 몽타주를 결정한 뒤 Multicast_PlayInteractionAnim으로 전체 클라이언트에 재생.
	 * BP override용 OnPlayInteractionAnim 이벤트도 서버에서 호출된다.
	 */
	virtual void TryPlayInteractionAnim(AActor* InteractionInstigator);

	/**
	 * 모든 클라이언트에서 상호작용 몽타주를 재생한다.
	 * 서버의 TryPlayInteractionAnim에서 호출 — 직접 호출 불필요.
	 * Montage: 메인 메시(ALS 등), VisualMontage: 비주얼 메시(Synty 등). 각각 스켈레톤 자동 매칭.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayInteractionAnim(AActor* Instigator, UAnimMontage* Montage, float PlayRate, UAnimMontage* VisualMontage);

	/** 상호작용 종료 시 해당 캐릭터에서 재생 중인 상호작용 몽타주를 정지한다. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopInteractionAnim(AActor* Instigator, UAnimMontage* Montage, UAnimMontage* VisualMontage, float BlendOutTime);

	/** BP가 액터별 애니메이션을 override하고 싶을 때 사용 (서버에서 호출됨) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Interaction|Animation")
	void OnPlayInteractionAnim(AActor* InteractionInstigator);

private:
	void DisableShippingInteractionDebug();

	/** 결과 적용 후 bReopenAfterRests면 비활성 상태로 전환 + MapIconComponent에 전파 */
	void BeginDepletedState();

	/** Channel.Player.Rested 수신 핸들러 — 카운트다운 후 재오픈 처리 */
	void HandlePlayerRested();

	FGameplayMessageListenerHandle RestListenerHandle;
	int32 RestsRemaining = 0;
	TWeakObjectPtr<UActorComponent> CachedInteractionManagerComp;

	/** bReopenAfterRests용 — Manager_InteractionTarget의 FinishMethod/ReactivationDuration을 강제 재설정 */
	void ForcePersistentInteractionManager(UActorComponent* ManagerComp) const;

	const struct FRetrieveInteractionPresetData* GetEffectivePresetData() const;
	FText GetEffectiveDisplayText() const;
	bool GetEffectiveHoldInteraction() const;
	float GetEffectiveHoldDuration() const;
	float GetEffectiveMontagePlayRate() const;

	/** OpenChest처럼 결과 적용 전 상호작용 구간 동안 몽타주를 재생해야 하는 프리셋인지 확인한다. */
	bool ShouldPlayMontageDuringInteraction() const;
	UTexture2D* GetEffectivePromptIcon() const;
	FLinearColor GetEffectivePromptAccentColor() const;
	FName GetEffectiveMgrPropIcon() const;
	FName GetEffectiveMgrPropColor() const;

	/** InteractionTypeAsset 설정을 ManagerComp에 reflection으로 적용하는 내부 구현 */
	void ApplyTypeAssetToManagerInternal(UActorComponent* ManagerComp);
};
