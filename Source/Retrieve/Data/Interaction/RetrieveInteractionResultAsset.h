#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RetrieveInteractionResultAsset.generated.h"

/**
 * 상호작용 결과의 추상 베이스 DataAsset.
 *
 * InteractionManager(상용 BP 에셋)이 OnInteractionFinished를 발동하면
 * URetrieveInteractionResponseComponent가 이 ResultAsset의 ApplyResult를 호출한다.
 *
 * 결과 종류마다 자식을 만들어 ApplyResult_Implementation을 override한다.
 *   - URetrievePickupGroupResultAsset : 여러 아이템 고정 묶음 (상자·퀘스트 보상 등)
 *   - URetrieveCompositeResultAsset   : 복수 결과 유형의 중첩 조합
 *   - URetrieveCustomEventResultAsset : BP에서 자유 구현 (문/레버/대화/봉인 해제 등)
 *
 * ※ 단순 픽업(1종 아이템)·확률 드롭은 ResponseComponent 인라인 필드로 처리한다.
 *   ResultAsset DA가 불필요하므로 자식 클래스를 만들지 않아도 된다.
 *
 * BlueprintNativeEvent라 BP 자식 에셋(예: DA_Door_OpenA가 URetrieveCustomEventResultAsset
 * 상속)에서 그래프로 ApplyResult를 override할 수 있다.
 *
 * 네트워크: ApplyResult는 서버 권한에서만 호출됨 (ResponseComponent가 보장).
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class RETRIEVE_API URetrieveInteractionResultAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 결과 적용 — 자식이 override.
	 * @param WorldContextObject   호출자 컨텍스트 (보통 ResponseComponent의 owner)
	 * @param Instigator           상호작용을 발동한 액터 (보통 플레이어 폰)
	 * @param InteractedActor      ResponseComponent가 부착된 액터 (상호작용 대상)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Retrieve|Interaction|Result")
	void ApplyResult(UObject* WorldContextObject, AActor* Instigator, AActor* InteractedActor) const;
	virtual void ApplyResult_Implementation(UObject* WorldContextObject, AActor* Instigator, AActor* InteractedActor) const;

	/** UI/로그/디버그용 표시 이름 (예: "회복약", "철문") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Interaction|Result")
	FText DisplayName;
};
