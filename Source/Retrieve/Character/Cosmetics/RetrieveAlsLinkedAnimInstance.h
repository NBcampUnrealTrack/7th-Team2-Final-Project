#pragma once

#include "CoreMinimal.h"
#include "AlsLinkedAnimationInstance.h"

#include "RetrieveAlsLinkedAnimInstance.generated.h"

class UAnimMontage;
class URetrieveAlsAnimInstance;

/**
 * Retrieve ALS 가지의 Linked AnimInstance 베이스.
 * Linked ABP가 부모 AnimInstance(URetrieveAlsAnimInstance)의 멤버에 캐스트 없이
 * Property Access 식으로 접근할 수 있도록 typed parent 캐시를 제공합니다.
 *
 * 사용 예 (Linked ABP):
 *   - 노드 핀에 GetRetrieveParent.<자식 ABP에서 정의한 변수> 식 바인딩 → 매 틱 ThreadSafe 평가
 *   - 변수 자체는 ABP_RetrieveAls_Sovereign에서 정의/매핑하고, 여기서는 캐스트 없는 접근만 보장.
 *
 * UAlsLinkedAnimationInstance가 이미 ALS 측 Parent / Character 캐시를 보유하므로,
 * 여기서는 Retrieve 측 typed 캐시만 추가합니다.
 */
UCLASS()
class RETRIEVE_API URetrieveAlsLinkedAnimInstance : public UAlsLinkedAnimationInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;

	// 이 무기 레이어의 발검/납검 몽타주. GA_StanceTransition이 전이 시 꺼내 슬롯에 재생한다.
	// 무기별 레이어 ABP마다 자기 몽타주를 지정(무기 교체 relink 시 자동으로 딸려옴).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Stance")
	TObjectPtr<UAnimMontage> DrawMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Stance")
	TObjectPtr<UAnimMontage> SheatheMontage;

	// 이 무기 레이어의 장착/해제 몽타주. GA_EquipTransition이 교체 시 꺼내 슬롯에 재생한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Equip")
	TObjectPtr<UAnimMontage> EquipMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Equip")
	TObjectPtr<UAnimMontage> UnequipMontage;

protected:
	/**
	 * Retrieve typed parent 접근자. Property Access (BlueprintThreadSafe) 전용.
	 * Worker thread에서 평가되므로 부모의 NativeUpdateAnimation에서 갱신되는 변수만 안전.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Linked Anim",
		Meta = (BlueprintThreadSafe, ReturnDisplayName = "Retrieve Parent"))
	URetrieveAlsAnimInstance* GetRetrieveParent() const;

	UPROPERTY(VisibleAnywhere, Category = "State", Transient)
	TWeakObjectPtr<URetrieveAlsAnimInstance> RetrieveParent;
};
