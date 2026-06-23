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

// 발검/납검 소켓 스왑용 파트 기록. 메시 ref와 그 파트의 손 소켓(무기 데이터 AttachSocketName)을 짝지어 둔다.
// 등(수납) 소켓은 데이터가 아니라 무기 타입 레이어의 SheathedSocketByDrawnSocket 맵에서 DrawnSocket으로 해석한다.
USTRUCT()
struct FRetrieveEquippedWeaponPart
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> Mesh = nullptr;

	FName DrawnSocket = NAME_None;

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
	UDataTable* GetAttackTable() const { return CurrentWeaponAttackTable; }
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	UMeshComponent* GetPrimaryEquippedWeaponMesh() const;
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|Weapon")
	UMeshComponent* GetWeaponMeshForTrace(FName StartSocket, FName EndSocket) const;

	// 발검/납검 시 무기 파트들을 손/등 소켓으로 재부착한다(상태/타이밍은 호출자가 결정, 부착 연산만 담당).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Weapon")
	void SetWeaponDrawn(bool bDrawn);

	// 비주얼 축(로컬). 장착/해제 몽타주의 노티와 ReconcileVisuals가 직접 호출한다.
	void SpawnWeaponVisuals();   // 현재 데이터로 메시 스폰 (장착 중일 때만)
	void ClearWeaponVisuals();   // 메시 파괴
	void ReconcileVisuals();     // = Clear + Spawn : 비주얼을 데이터 상태로 강제 일치 (안전망)

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Weapon")
	FWeaponChangedSignature OnWeaponEquipped;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Weapon")
	FWeaponChangedSignature OnWeaponUnequipped;

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
	TObjectPtr<UDataTable> CurrentWeaponAttackTable;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> EquippedWeaponMeshComponents;

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
	bool ApplyWeaponData(FName WeaponItemId, const FRetrieveWeaponDataRow& WeaponData);
	bool ApplyWeaponVisuals(const FRetrieveWeaponDataRow& WeaponData);
	// 어태치먼트 데이터로 메시 컴포넌트를 생성해 반환. 메시 미설정 시 nullptr
	UMeshComponent* CreateWeaponMeshComponent(const FRetrieveWeaponAttachmentData& Attachment) const;
	USceneComponent* FindAttachmentParent(const FRetrieveWeaponAttachmentData& Attachment) const;
};
