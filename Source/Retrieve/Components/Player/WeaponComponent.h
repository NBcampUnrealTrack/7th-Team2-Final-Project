#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveAbilitySet.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/PawnComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "WeaponComponent.generated.h"

class UGameplayEffect;
class URetrieveAbilitySystemComponent;
class UMeshComponent;
class USceneComponent;

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

	// 발검/납검 시 무기 파트들을 손/등 소켓으로 재부착한다(상태/타이밍은 호출자가 결정, 부착 연산만 담당).
	// OnlyDrawnSocket을 지정하면 그 손 소켓(DrawnSocket)을 가진 파트만 스왑한다 — 한 몽타주에서
	// 검/방패를 다른 프레임에 따로 옮기는 용도. NAME_None이면 전체 파트(기본).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Weapon")
	void SetWeaponDrawn(bool bDrawn, FName OnlyDrawnSocket = NAME_None);

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
	TArray<TObjectPtr<UMeshComponent>> EquippedWeaponMeshComponents;

	// 교체 중 OLD 메시. EquipWeapon이 NEW 스폰 전에 여기로 옮겨 두고, 몽타주 끝(또는 fallback)에서 파괴한다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> PendingDestroyMeshComponents;

	// 발검/납검 소켓 스왑용. EquippedWeaponMeshComponents와 별개로 손/등 소켓 짝을 들고 있다.
	// (#6이 EquippedWeaponMeshComponents를 재구성해도 영향 없게 분리. 나중에 한 struct로 합칠 수 있음)
	UPROPERTY(Transient)
	TArray<FRetrieveEquippedWeaponPart> WeaponAttachParts;

	UPROPERTY(Transient)
	FRetrieveAbilitySet_GrantedHandles WeaponGrantedHandles;

	// 현재 장착된 무기의 AttackPower GE 핸들 — 언장착 시 제거에 사용
	UPROPERTY(Transient)
	FActiveGameplayEffectHandle WeaponAttackPowerEffectHandle;

	UFUNCTION()
	void OnRep_CurrentWeaponDataRow();

	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const;
	const FRetrieveWeaponDataRow* FindWeaponData(FName WeaponItemId) const;
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
};
