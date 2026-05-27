#pragma once

#include "CoreMinimal.h"
#include "Data/Interaction/RetrieveInteractionResultAsset.h"
#include "RetrieveCustomEventResultAsset.generated.h"

/**
 * BP에서 ApplyResult를 자유롭게 override하기 위한 결과 자식.
 *
 * 인벤토리 추가가 아닌 결과(문 열기 / 레버 토글 / 대화 시작 / 봉인 해제 등)를
 * 별도 C++ 자식 클래스 없이 BP만으로 구현할 때 사용한다.
 *
 * 사용법:
 *   1. Content Browser → Right-click → Miscellaneous → Data Asset
 *      → Class: RetrieveCustomEventResultAsset
 *      → 예: DA_Door_OpenA
 *   2. 그 DataAsset을 더블클릭하면 ApplyResult를 그래프에서 override 가능
 *   3. 그래프 안에서 회전, 사운드, 다이얼로그 호출 등 자유 구현
 *
 * 또는 이 클래스를 다시 자식 BP로 상속받아 (예: BP_DoorResult) 거기서 override해도 됨.
 *
 * 참고: BlueprintNativeEvent의 BP override는 부모 클래스가 Blueprintable이어야 한다 (베이스에서 보장).
 */
UCLASS(BlueprintType, Blueprintable)
class RETRIEVE_API URetrieveCustomEventResultAsset : public URetrieveInteractionResultAsset
{
	GENERATED_BODY()

	// 본체 없음. ApplyResult는 BP에서 override.
	// 베이스의 ApplyResult_Implementation이 no-op이라 BP override가 없으면 아무것도 안 일어남 (정상).
};
