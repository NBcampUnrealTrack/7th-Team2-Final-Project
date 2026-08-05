#pragma once

#include "CoreMinimal.h"
#include "RetrieveObjectiveMarkerTypes.generated.h"

/**
 * 목표 마커의 종류. 색/우선순위/화면 표시 규칙을 가른다.
 *   Main   = 메인 퀘스트 현재 목표(금색, 항상 1개, 거리 컬링/상한 제외)
 *   Side   = 수락한 인카운터 퀘스트 목표(빨강, 여러 개 가능)
 *   TurnIn = 모든 목표 완료 후 보상을 받으러 갈 NPC(초록)
 */
UENUM(BlueprintType)
enum class ERetrieveObjectiveMarkerKind : uint8
{
	Main   UMETA(DisplayName = "메인 퀘스트"),
	Side   UMETA(DisplayName = "인스턴스 퀘스트"),
	TurnIn UMETA(DisplayName = "보상 수령"),
	// 신규 값은 반드시 끝에 추가 — 저장된 정수값이 밀리지 않도록.
	/** 아직 수락하지 않은 의뢰. "저기 누가 뭘 부탁하려나 보다"를 알리는 단계. */
	Offer  UMETA(DisplayName = "의뢰 있음"),
	/**
	 * 메인 퀘스트의 선택 목표. 안 해도 진행되지만 위치는 알려준다.
	 * 필수 목표와 나란히 떠서 "저쪽은 선택, 저쪽은 진행"을 구분하게 한다.
	 */
	MainOptional UMETA(DisplayName = "메인 퀘스트(선택)")
};

/**
 * 매 갱신마다 다시 계산되는 마커의 가변 상태.
 *
 * 위치를 액터 포인터가 아니라 좌표로 들고 있는 이유:
 * 목표 종류마다 "지금 가리켜야 할 지점"이 달라지기 때문이다.
 * (스폰그룹 = 멀면 스포너 / 가까우면 최근접 생존 몬스터,
 *  물건찾기 = 멀면 인카운터 중심 지역 / 가까우면 실제 물건 위치)
 */
USTRUCT(BlueprintType)
struct FRetrieveObjectiveMarkerState
{
	GENERATED_BODY()

	/** 마커를 그릴 월드 좌표. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	FVector WorldLocation = FVector::ZeroVector;

	/** 아이콘 옆/아래에 붙는 진행 문구(예: "남은 적 3", "늑대 가죽 2/5"). 비우면 미표시. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	FText ProgressText;

	/** false면 이번 갱신에서 마커를 숨긴다(대상 미로드 등). */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	bool bVisible = true;

	/**
	 * false면 나침반/맵에만 표시하고 화면(3D 투영) 마커는 만들지 않는다.
	 * 드랍 수집형(AcquireItem)처럼 "정해진 한 지점"이 없는 목표에 사용한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	bool bAllowScreenMarker = true;

	/** 대상 위치가 아직 지역 단위로만 확정된 상태(정밀 스냅 전). 아이콘을 흐리게 그리는 용도. */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|ObjectiveMarker")
	bool bApproximate = false;
};

/**
 * 마커 위치/문구를 다시 계산하는 콜백.
 * false를 반환하면 해당 마커는 목록에서 제거된다(목표 완료·대상 소멸).
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FRetrieveObjectiveMarkerRefresh, FRetrieveObjectiveMarkerState& /*InOutState*/);

/**
 * 서브시스템이 보관하는 마커 1개.
 *
 * USTRUCT이 아닌 이유: 갱신 델리게이트를 멤버로 들고 있기 때문.
 * UI로 넘길 값은 State/Kind/Label만이며, 위젯은 이 구조체를 직접 읽는다(C++ 전용).
 */
struct FRetrieveObjectiveMarker
{
	/** 고유 키. 인카운터 마커는 "<EncounterId>_Obj0" 형식으로 발급된다. */
	FName MarkerId;

	ERetrieveObjectiveMarkerKind Kind = ERetrieveObjectiveMarkerKind::Side;

	/** 목표 제목(예: "늑대 소굴 정리"). 중거리 이상에서 아이콘 아래에 표시. */
	FText Label;

	/** 같은 거리일 때의 정렬 우선순위. 큰 값이 위. */
	int32 SortPriority = 0;

	FRetrieveObjectiveMarkerState State;

	/** 선택. 바인딩되어 있으면 서브시스템이 주기적으로 호출해 State를 갱신한다. */
	FRetrieveObjectiveMarkerRefresh RefreshDelegate;
};
