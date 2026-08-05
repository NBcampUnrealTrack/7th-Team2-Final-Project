#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/RetrieveObjectiveMarkerTypes.h"
#include "RetrieveObjectiveMarkerSubsystem.generated.h"

class URetrieveObjectiveAnchorComponent;
class URetrieveObjectiveAnchorDataAsset;

/**
 * 퀘스트 목표 마커의 단일 진실 소스.
 *
 * 화면 마커(URetrieveObjectiveMarkerLayerWidget) / 나침반 / 미니맵 / 월드맵은
 * 전부 여기만 읽는다. 마커를 만드는 쪽은 두 갈래다:
 *
 *   - 인스턴스 퀘스트 : ARetrieveQuestEncounter가 Phase 전환마다 등록/해제.
 *                       위치는 각 URetrieveQuestObjective::GetMarkerState가 계산.
 *   - 메인 퀘스트     : QuestTrackerViewModel이 "현재 추적 중인 미완료 목표"의
 *                       CompletionTag를 SetTrackedQuestStep으로 알려주면,
 *                       같은 태그를 가진 URetrieveObjectiveAnchorComponent를 찾아 붙인다.
 *
 * 위치가 매 프레임 바뀌는 목표(도망치는 몬스터 등)가 있으므로 RefreshDelegate를
 * RefreshInterval 주기로 호출해 State를 갱신한다. 매 틱이 아닌 이유는 마커 위치가
 * 0.15초 단위로 갱신돼도 시각적으로 구분되지 않고, 목표당 액터 검색이 들어가기 때문.
 */
UCLASS()
class RETRIEVE_API URetrieveObjectiveMarkerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ── 마커 등록/해제 ────────────────────────────────────────────────────────
	/** MarkerId 기준 upsert. 이미 있으면 내용을 덮어쓴다. */
	void RegisterMarker(FRetrieveObjectiveMarker&& InMarker);

	void RemoveMarker(FName MarkerId);

	/** "<EncounterId>_"처럼 접두사가 같은 마커를 모두 제거(인카운터 정리용). */
	void RemoveMarkersWithPrefix(const FString& Prefix);

	/** 화면/나침반/맵이 읽는 현재 마커 목록. */
	const TArray<FRetrieveObjectiveMarker>& GetMarkers() const { return Markers; }

	/** 화면 마커를 허용하고 현재 보이는 마커만 골라 담는다(거리 정렬은 호출부 담당). */
	void GetScreenMarkers(TArray<const FRetrieveObjectiveMarker*>& OutMarkers) const;

	/**
	 * 수락한 인카운터 퀘스트(수행 중 + 보상 대기)를 가까운 순으로 담는다.
	 * 아직 수락하지 않은 의뢰(Offer)와 메인 퀘스트는 제외된다.
	 *
	 * bRequireCurrentProximity:
	 *   true  = 지금 발견 반경 안에 있는 것만 (트래커 — 멀어지면 사라진다)
	 *   false = 한 번이라도 발견한 것 전부 (저널 — 받아둔 의뢰 목록이므로 유지된다)
	 */
	void GetAcceptedQuestMarkers(
		const FVector& ViewerLocation,
		TArray<const FRetrieveObjectiveMarker*>& OutMarkers,
		bool bRequireCurrentProximity = false) const;

	/** MarkerId로 마커를 찾는다(없으면 nullptr). */
	const FRetrieveObjectiveMarker* FindMarkerById(FName MarkerId) const;

	// ── 인스턴스 퀘스트 발견 반경 (화면/나침반/미니맵/월드맵 공용) ─────────────
	/**
	 * 인스턴스 계열(의뢰/수행/보상) 마커가 보이기 시작하는 거리(UU).
	 * 화면 마커 레이어가 자기 설정값을 여기에 밀어 넣어, 네 표시 경로가 같은 값을 쓴다.
	 * 메인 퀘스트 마커는 이 제한을 받지 않는다.
	 */
	void SetDiscoveryRadius(float InRadius) { DiscoveryRadius = InRadius; }
	float GetDiscoveryRadius() const { return DiscoveryRadius; }

	/**
	 * 지금 이 순간 반경 안에 있는지. 화면 마커·나침반이 쓴다.
	 * 메인은 항상 true. (실시간 근접 판정 — 멀어지면 다시 숨는다)
	 */
	static bool PassesDiscoveryRadius(
		const FRetrieveObjectiveMarker& Marker, const FVector& ViewerLocation, float Radius);

	/**
	 * 맵(월드맵/미니맵)에 표시해도 되는지.
	 * 메인은 항상 true, 인스턴스 계열은 **한 번이라도 발견한 적 있으면** true.
	 * 지도는 "가본 곳을 기록하는 물건"이므로 멀어져도 계속 남는 것이 자연스럽다.
	 */
	bool PassesMapVisibility(const FRetrieveObjectiveMarker& Marker) const;

	/** 이 마커를 발견한 적 있는지(세션 단위 기록). */
	bool IsMarkerDiscovered(FName MarkerId) const { return DiscoveredMarkerIds.Contains(MarkerId); }

	/**
	 * 같은 스텝 태그를 여러 액터가 주장할 때의 서열.
	 * 값이 클수록 "과제가 실제로 있는 곳"일 확률이 높다고 본다.
	 * 베이크(URetrieveObjectiveAnchorDataAsset)도 같은 규칙을 쓰므로 public.
	 */
	static int32 GetAnchorPriority(const URetrieveObjectiveAnchorComponent* Anchor);

	// ── 메인/사이드(DT_Quest) 추적 목표 ───────────────────────────────────────
	/** 추적 중인 목표 하나(필수 또는 선택). */
	struct FTrackedStep
	{
		FGameplayTag StepTag;
		FText Label;
		ERetrieveObjectiveMarkerKind Kind = ERetrieveObjectiveMarkerKind::Main;
	};

	/**
	 * 지금 추적 중인 퀘스트의 미완료 목표들을 알린다.
	 * 필수 목표와 선택 목표를 함께 넘기면 둘 다 마커가 생겨
	 * "저쪽은 선택, 저쪽은 진행"을 한눈에 구분할 수 있다.
	 * 앵커가 아직 스트리밍되지 않은 목표는 태그를 기억해 두고, 앵커 등록 시 붙는다.
	 */
	void SetTrackedQuestSteps(const TArray<FTrackedStep>& Steps);

	void ClearTrackedQuestStep();

	// ── 앵커 등록(메인 퀘스트 위치원) ─────────────────────────────────────────
	void RegisterAnchor(URetrieveObjectiveAnchorComponent* Anchor);
	void UnregisterAnchor(URetrieveObjectiveAnchorComponent* Anchor);

	/** 추적 마커에 쓰는 고정 MarkerId. */
	static const FName TrackedQuestMarkerId;

	// ── 레거시 퀘스트 액터용 편의 함수 ────────────────────────────────────────
	/**
	 * 대상 액터를 따라다니는 마커 하나를 등록한다.
	 * LostCargo/Rescue처럼 자체 상태 머신을 가진 퀘스트 액터가 상태 전환마다 호출한다.
	 * Target이 사라지면 마커도 자동으로 정리된다.
	 */
	static void RegisterActorMarker(
		UWorld* World,
		FName MarkerId,
		ERetrieveObjectiveMarkerKind Kind,
		const FText& Label,
		AActor* Target,
		const FText& ProgressText,
		float ZOffset = 150.0f,
		int32 SortPriority = 0);

	/** 접두사가 같은 마커를 모두 제거(월드에서 서브시스템을 찾아 위임). */
	static void RemoveMarkersByPrefix(UWorld* World, const FString& Prefix);

	// ── UTickableWorldSubsystem ───────────────────────────────────────────────
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	/**
	 * 추적 마커를 다시 만든다.
	 * 우선순위: 로드된 앵커(정확한 현재 위치) > 베이크된 좌표(스트리밍 전 폴백).
	 * 둘 다 없으면 마커 없음.
	 */
	void RefreshTrackedQuestMarker();

	/** DA_MapConfig에 연결된 목표 지점 베이크 에셋을 (없으면) 찾아 캐시한다. */
	const URetrieveObjectiveAnchorDataAsset* GetBakedAnchorData();

	FRetrieveObjectiveMarker* FindMarker(FName MarkerId);

	TArray<FRetrieveObjectiveMarker> Markers;

	TArray<TWeakObjectPtr<URetrieveObjectiveAnchorComponent>> Anchors;

	/** 현재 추적 중인 목표들(필수 + 선택). 각각 마커 하나를 만든다. */
	TArray<FTrackedStep> ActiveSteps;

	/** 스트리밍되지 않은 목표의 폴백 좌표. DA_MapConfig 경유로 로드. */
	UPROPERTY(Transient)
	TObjectPtr<const URetrieveObjectiveAnchorDataAsset> BakedAnchorData;

	bool bBakedAnchorLookupDone = false;

	/** 인스턴스 마커 발견 반경(UU). 화면 마커 레이어가 매 갱신마다 자기 값으로 덮어쓴다. */
	float DiscoveryRadius = 6000.0f;

	/** 발견 반경 안에 한 번이라도 들어왔던 마커들. 맵 표시 판정에 쓴다.
	 *  현재는 세션 단위(세이브에 남기지 않음) — 게임을 다시 켜면 다시 발견해야 한다. */
	TSet<FName> DiscoveredMarkerIds;

	/** 매 갱신마다 플레이어 근처 마커를 발견 기록에 추가한다. */
	void UpdateDiscoveredMarkers();

	float RefreshAccumulator = 0.0f;

	/** RefreshDelegate 호출 주기(초). */
	static constexpr float RefreshInterval = 0.15f;
};
