#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveAbilitySet.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/PawnComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "WeaponComponent.generated.h"

class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSystem;
class URetrieveAbilitySystemComponent;
class UElementUnlockComponent;
class UMeshComponent;
class UStaticMeshComponent;
class USceneComponent;
class UMaterialInterface;
struct FGameplayEventData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWeaponChangedSignature, FName, WeaponItemId);

// 무기 메시가 실제로 스폰된 직후 신호(모든 스폰 경로의 단일 진입점). C++ 내부 조율용 — BP 노출 없음.
// CombatStance가 이 시점에 현재 스탠스로 소켓을 맞춘다(장착이 스탠스 init보다 늦어도 손/등 동기화 보장).
DECLARE_MULTICAST_DELEGATE(FWeaponVisualsSpawnedSignature);

// 발검/납검 소켓 스왑용 파트 기록. 메시 ref와 그 파트의 손 소켓(무기 데이터 AttachSocketName)을 짝지어 둔다.
// 등(수납) 소켓은 전역 설정 URetrieveWeaponSocketSettings에서 손 소켓(DrawnSocket)으로 해석한다.
USTRUCT()
struct FRetrieveEquippedWeaponPart
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> Mesh = nullptr;

	FName DrawnSocket = NAME_None;

	// 등(수납) 소켓. 스폰 시점에 Settings(무기 타입)로 해석해 캐싱 — Unequip으로 타입 태그가 비어도 유지된다.
	FName SheathedSocket = NAME_None;

	// 소켓 스냅(SnapToTarget) 후 다시 적용할 상대 오프셋(장착 시 데이터 값).
	FTransform RelativeTransform = FTransform::Identity;
};

USTRUCT()
struct FRetrieveEquippedWeaponMesh
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> Mesh = nullptr;

	bool bGeneratesHitVolume = false;
	bool bUseBoundsTrace = false;
	ERetrieveBoundsTraceShape BoundsTraceShape = ERetrieveBoundsTraceShape::SweptLongAxis;
	float BoundsRadiusScale = 1.0f;
	float BoundsLengthPadding = 0.0f;
	FName TraceStartSocket = NAME_None;
	FName TraceEndSocket = NAME_None;
};

// 장착된 무기의 전투 데이터, 비주얼, 무기 전용 어빌리티를 적용한다.
// 장착 가능 여부와 보유 검사는 InventoryComponent에서 먼저 처리한다.
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UWeaponComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Weapon")
	bool EquipWeapon(FName WeaponItemId);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Weapon")
	void UnequipWeapon();

	// 원소 모드 전환 이벤트(GameplayEvent.Element.ModeChange)를 구독해 장착 무기의
	// ElementModeMaterials에 따라 검 머티리얼을 교체한다. 캐릭터가 ASC 준비 후 호출.
	void InitializeWithAbilitySystem(URetrieveAbilitySystemComponent* InASC);
	void UninitializeFromAbilitySystem();

	// 상태 조회: 장착 여부 → 어떤 무기 → 데이터 → 전투 테이블 순으로 배치
	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	bool IsEquipped() const { return !CurrentWeaponDataRow.IsNone(); }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	FName GetCurrentWeaponDataRow() const { return CurrentWeaponDataRow; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	FRetrieveWeaponDataRow GetWeaponData() const { return CurrentWeaponData; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	UMeshComponent* GetPrimaryEquippedWeaponMesh() const;
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	UMeshComponent* GetWeaponMeshForTrace(FName StartSocket, FName EndSocket) const;
	
	void GetHitVolumeMeshes(TArray<FRetrieveEquippedWeaponMesh>& OutParts) const;
	
	UMeshComponent* GetEquippedMeshBySocket(FName AttachSocketName) const;

	// 발검/납검 시 무기 파트들을 손/등 소켓으로 재부착한다(상태/타이밍은 호출자가 결정, 부착 연산만 담당).
	// OnlyDrawnSocket을 지정하면 그 손 소켓(DrawnSocket)을 가진 파트만 스왑한다 — 한 몽타주에서
	// 검/방패를 다른 프레임에 따로 옮기는 용도. NAME_None이면 전체 파트(기본).
	// bSetHidden=true면 소켓 스왑 대신 대상 파트를 Hidden 처리한다(등 소켓 없는 방패 등, 파괴 직전 은폐).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Weapon")
	void SetWeaponDrawn(bool bDrawn, FName OnlyDrawnSocket = NAME_None, bool bSetHidden = false);

	// 노킹 화살(별도 메시, bIsNockedArrow attachment) 표시/숨김. 노티(AnimNotify_SetNockedArrow)가 호출.
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Weapon")
	void SetNockedArrowVisible(bool bVisible);

	// 현재 화살이 노킹(장전)돼 있는가. GA_BowShot이 드로우 전 장전 필요 판정에 쓴다.
	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	bool IsArrowNocked() const { return bArrowNocked; }

	// 비주얼 축(로컬). 장착/해제 몽타주의 노티와 ReconcileVisuals가 직접 호출한다.
	void SpawnWeaponVisuals();   // 현재 데이터로 메시 스폰 (장착 중일 때만)
	void ClearWeaponVisuals();   // 메시 파괴
	void ReconcileVisuals();     // = Clear + Spawn : 비주얼을 데이터 상태로 강제 일치 (안전망)
	void DestroyPendingVisuals(); // 교체 시 보류된 OLD 메시 파괴 (Equip 몽타주 끝/캔슬에서 GA가 호출)
	// Equip/Unequip 몽타주 정상 완료 가드: 노티가 블렌드아웃으로 누락돼도 최종 비주얼을 맞춘다.
	// 장착 상태면 전 파트 visible 강제(hidden 잔존 방지), 해제 상태면 메시 정리 보장.
	void FinalizeEquipTransitionVisuals();

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Weapon")
	FWeaponChangedSignature OnWeaponEquipped;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Weapon")
	FWeaponChangedSignature OnWeaponUnequipped;

	// 메시 스폰 직후 발화(SpawnWeaponVisuals 끝). UPROPERTY 불가(non-dynamic) — C++ 구독 전용.
	FWeaponVisualsSpawnedSignature OnWeaponVisualsSpawned;

	const FRetrieveWeaponDataRow& GetWeaponDataRef() const { return CurrentWeaponData; }

	/** WeaponItemId 에 해당하는 DataTable 행을 반환. 없으면 nullptr. 장착 여부 무관. */
	const FRetrieveWeaponDataRow* FindWeaponData(FName WeaponItemId) const;
	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Weapon")
	TObjectPtr<UDataTable> WeaponDataTable;

	// 무기 장착 시 캐릭터 AttackPower에 가산할 GameplayEffect
	// GE 설정: Duration=Infinite, Modifier=AttackPower Add, Magnitude=SetByCaller(Data.Weapon.AttackPower)
	// Blueprint(BP_WeaponComponent 등)에서 GE_WeaponAttackPower 에셋을 할당해야 함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Weapon|Stats")
	TSubclassOf<UGameplayEffect> WeaponAttackPowerEffect;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentWeaponDataRow, Category = "Retrieve|Weapon")
	FName CurrentWeaponDataRow = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Weapon")
	FGameplayTag CurrentWeaponTypeTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Weapon")
	FGameplayTag CurrentWeaponAffinityTag;

	UPROPERTY(Transient)
	FRetrieveWeaponDataRow CurrentWeaponData;
	
	UPROPERTY(Transient)
	TArray<FRetrieveEquippedWeaponMesh> EquippedWeaponMeshComponents;

	// 교체 중 OLD 메시. EquipWeapon이 NEW 스폰 전에 여기로 옮겨 두고, 몽타주 끝(또는 fallback)에서 파괴한다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> PendingDestroyMeshComponents;

	// 발검/납검 소켓 스왑용. EquippedWeaponMeshComponents와 별개로 손/등 소켓 짝을 들고 있다.
	// (#6이 EquippedWeaponMeshComponents를 재구성해도 영향 없게 분리. 나중에 한 struct로 합칠 수 있음)
	UPROPERTY(Transient)
	TArray<FRetrieveEquippedWeaponPart> WeaponAttachParts;

	// 노킹 화살 메시(bIsNockedArrow attachment). 표시/숨김 토글용. EquippedWeaponMeshComponents가 소유·파괴한다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> NockedArrowMeshes;
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Weapon|Enhancement VFX")
	TSoftObjectPtr<UNiagaraSystem> EnhancementVFXTier1;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Weapon|Enhancement VFX")
	TSoftObjectPtr<UNiagaraSystem> EnhancementVFXTier2;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Weapon|Enhancement VFX")
	TSoftObjectPtr<UNiagaraSystem> EnhancementVFXTier3;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> WeaponEnhancementVFXComponents;

	// 원소 모드 강화(가디언 코어 흡수로 해방) 시 무기에 붙는 원소별 오라. 키 = Element.Fire/Water/Wind.
	// 원소 머티리얼 없는 기본 무기에도 표시(플레이어 상태 연출). 활(Weapon.Type.Bow)은 제외.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Weapon|Element Empower VFX")
	TMap<FGameplayTag, TSoftObjectPtr<UNiagaraSystem>> ElementEmpowerVFX;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> ElementEmpowerVFXComponents;

	// 노킹 상태(화살 장전됨). SetNockedArrowVisible에서 갱신. 드로우 전 장전 판정용(로컬 비주얼 상태).
	bool bArrowNocked = false;

	UPROPERTY(Transient)
	FRetrieveAbilitySet_GrantedHandles WeaponGrantedHandles;

	// 현재 장착된 무기의 AttackPower GE 핸들 — 언장착 시 제거에 사용
	UPROPERTY(Transient)
	FActiveGameplayEffectHandle WeaponAttackPowerEffectHandle;

	UFUNCTION()
	void OnRep_CurrentWeaponDataRow();

	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const;
	void ClearGrantedWeaponAbilities();
	void ClearWeaponData();                                       // 데이터/어빌리티/GE 정리 (비주얼·브로드캐스트 없음)
	bool TryTriggerEquipTransition(const FGameplayTag& EventTag); // GA_EquipTransition 트리거, 발동 여부 반환
	bool HasAuthorityToModify() const;
	// Equip/Unequip 전환 GA가 활성인가. 스폰 숨김 + 스탠스 자동부착 위임 게이트 판정에 쓴다.
	bool IsEquipTransitionActive() const;
	bool ApplyWeaponData(FName WeaponItemId, const FRetrieveWeaponDataRow& WeaponData);
	// bSpawnHidden=true면 메시를 숨겨서 스폰한다(Equip 전환 중 — 발검 노티가 손에 부착하며 보이게 함).
	bool ApplyWeaponVisuals(const FRetrieveWeaponDataRow& WeaponData, bool bSpawnHidden = false);
	// 어태치먼트 데이터로 메시 컴포넌트를 생성해 반환. 메시 미설정 시 nullptr
	UMeshComponent* CreateWeaponMeshComponent(const FRetrieveWeaponAttachmentData& Attachment) const;
	USceneComponent* FindAttachmentParent(const FRetrieveWeaponAttachmentData& Attachment) const;
	void SpawnWeaponEnhancementVFX();
	void ClearWeaponEnhancementVFX();

	// 강화/원소 오라 공용: VFX를 붙일 주 무기 스태틱메시(bGeneratesHitVolume 우선) 선택.
	UStaticMeshComponent* FindWeaponVFXTargetMesh() const;

	// 원소 모드 강화 오라: 조건(장착 && 현재 모드 원소가 해방됨 && 활 아님) 재평가 후 스폰/제거.
	void RefreshElementEmpowerVFX();
	void ClearElementEmpowerVFX();

	// ElementUnlockComponent::OnElementUnlocked 콜백. 장착 중 해방(가디언 처치 직후 같은 모드) 즉시 반영.
	UFUNCTION()
	void HandleElementUnlockedForVFX(FGameplayTag ElementTag);

	// 원소 모드 전환 콜백(GenericGameplayEventCallbacks). payload의 InstigatorTags[0]가 새 원소.
	void OnElementModeChanged(const FGameplayEventData* Payload);
	// 현재 원소모드에 매핑된 머티리얼을 장착 검 메시에 적용(맵 비었거나 매핑 없으면 무시).
	void ApplyElementModeMaterial();

	// 원소 이벤트 구독 상태
	TWeakObjectPtr<URetrieveAbilitySystemComponent> ElementEventASC;
	FDelegateHandle ElementModeChangedHandle;
	FGameplayTag CurrentElementModeTag;

	// 원소 해방 상태 조회/구독용(오너 폰의 ElementUnlockComponent). Init 시 캐싱.
	TWeakObjectPtr<UElementUnlockComponent> CachedElementUnlockComponent;
};
