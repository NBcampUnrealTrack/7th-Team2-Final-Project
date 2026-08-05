#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Data/RetrieveObjectiveMarkerTypes.h"
#include "RetrieveObjectiveMarkerLayerWidget.generated.h"

class UCanvasPanel;
class URetrieveObjectiveMarkerWidget;
class URetrieveObjectiveMarkerSubsystem;

/**
 * 목표 마커를 화면에 투영해 그리는 HUD 레이어.
 *
 * WBP 설정:
 *   1. 이 클래스를 부모로 하는 WBP_ObjectiveMarkerLayer 생성
 *   2. 루트에 CanvasPanel을 두고 이름을 CanvasPanel_Markers 로 (Is Variable 체크)
 *   3. MarkerWidgetClass 에 WBP_ObjectiveMarker(URetrieveObjectiveMarkerWidget 파생) 할당
 *   4. HUD에 ZOrder 5 정도로 추가 (나침반보다 아래)
 *
 * 표시 규칙(플레이 흐름을 끊지 않기 위한 장치):
 *   - 메인 마커는 거리 컬링/개수 상한에서 제외 — 항상 보인다.
 *   - 인스턴스 마커는 SideMarkerViewDistance 밖이면 화면에서 내리고 나침반에 맡긴다.
 *   - 화면에서 서로 겹치면 가까운 쪽만 라벨을 보여준다(DeclutterRadius).
 *   - 근접(NearFadeDistance)하면 페이드아웃해 상호작용 프롬프트에 자리를 내준다.
 *   - 패널이 열려 있거나 시네마틱 중이면 전체를 숨긴다.
 */
UCLASS()
class RETRIEVE_API URetrieveObjectiveMarkerLayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 마커 1개를 표현할 위젯 클래스(WBP_ObjectiveMarker). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker")
	TSubclassOf<URetrieveObjectiveMarkerWidget> MarkerWidgetClass;

	/** 화면 가장자리 클램프 여백(px). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout", meta = (ClampMin = "0.0"))
	float ScreenEdgePadding = 64.0f;

	/**
	 * 마커 위젯 전체에 곱하는 기본 크기 배율.
	 * WBP 아트가 100px 기준이라 그대로 쓰면 화면에서 과하게 크다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout", meta = (ClampMin = "0.1"))
	float MarkerBaseScale = 0.55f;

	/**
	 * 마커를 목표 지점보다 화면상 위로 띄우는 오프셋(px).
	 * 목표가 바로 앞에 있을 때 마커가 지면/대상에 겹쳐 보이는 것을 막는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout")
	FVector2D MarkerScreenOffset = FVector2D(0.0f, -50.0f);

	/**
	 * 화면 중앙(3인칭 캐릭터가 서 있는 영역)에 마커가 겹치면 위로 밀어 올린다.
	 * 목표가 정면 가까이 있을 때 캐릭터를 가리는 문제를 해결한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout")
	bool bAvoidScreenCenter = true;

	/** 이 반경(px) 안으로 들어오면 밀어내기 시작. 캐릭터가 차지하는 화면 크기에 맞춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout", meta = (ClampMin = "0.0"))
	float AvoidCenterRadius = 170.0f;

	/** 정중앙일 때 위로 밀어 올리는 최대 거리(px). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout", meta = (ClampMin = "0.0"))
	float AvoidCenterLift = 110.0f;

	/** 동시에 그릴 최대 마커 수(메인 마커는 이 상한에 포함되지만 항상 첫 자리를 차지한다). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout", meta = (ClampMin = "1"))
	int32 MaxScreenMarkers = 6;

	/**
	 * 인스턴스 퀘스트(의뢰/수행/보상) 마커의 **발견 반경**(UU). 기본 6000 = 60m.
	 * 이 안에 들어와야 화면에 나타난다. 맵 전역의 의뢰가 동시에 뜨는 것을 막아
	 * "화면에 상시 떠 있는 것은 메인 하나뿐"이라는 규칙을 만든다.
	 * 메인 퀘스트 마커는 이 제한을 받지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout", meta = (ClampMin = "0.0"))
	float SideMarkerViewDistance = 6000.0f;

	/**
	 * 인스턴스 마커 라벨 앞에 붙는 말머리. 메인에는 붙지 않는다.
	 * 색·크기와 별개로 텍스트로도 구분되므로 색약 사용자에게도 안전하다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout")
	FText SideLabelPrefix = NSLOCTEXT("Retrieve.ObjectiveMarker", "SideLabelPrefix", "의뢰 · ");

	/** 인스턴스 마커에 곱하는 크기(메인보다 작게 해 시선 우선순위를 만든다). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Feedback", meta = (ClampMin = "0.1"))
	float SideMarkerScale = 0.85f;

	/** 이 거리(UU) 안이면 라벨/진행 문구를 함께 보여준다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout", meta = (ClampMin = "0.0"))
	float LabelVisibleDistance = 4000.0f;

	/** 이 거리(UU) 안부터 마커가 옅어진다(상호작용 프롬프트에 인계). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout", meta = (ClampMin = "0.0"))
	float NearFadeDistance = 500.0f;

	/** 근접 시 남기는 최소 알파. 0이면 완전히 사라진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NearFadeMinAlpha = 0.15f;

	/** 화면에서 이 픽셀 안에 겹친 마커는 라벨을 접는다(가까운 쪽만 표시). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout", meta = (ClampMin = "0.0"))
	float DeclutterRadius = 48.0f;

	/** 패널(인벤토리·월드맵 등)이 열려 있으면 마커를 숨긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Layout")
	bool bHideWhilePanelOpen = true;

	// ── 시각 피드백 ───────────────────────────────────────────────────────────
	/** 마커가 처음 등장할 때의 스케일 인 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Feedback", meta = (ClampMin = "0.0"))
	float AppearDuration = 0.45f;

	/** 등장 순간의 시작 스케일. 1보다 크면 "펑" 하고 들어온다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Feedback", meta = (ClampMin = "1.0"))
	float AppearStartScale = 2.2f;

	/** 메인/보상 마커의 상시 호흡 펄스 크기(0이면 끔). "지금 이걸 해야 한다"를 계속 상기시킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Feedback", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float IdlePulseAmplitude = 0.07f;

	/** 호흡 펄스 1주기(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Feedback", meta = (ClampMin = "0.1"))
	float IdlePulsePeriod = 1.7f;

	/** 메인 마커에 곱하는 추가 크기(다른 마커보다 눈에 띄게). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Feedback", meta = (ClampMin = "0.5"))
	float MainMarkerScale = 1.15f;

	/** 이 거리(UU) 이내면 마커가 원래 크기. 멀어질수록 DistanceScaleMin까지 작아진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Feedback", meta = (ClampMin = "0.0"))
	float DistanceScaleNearRange = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Feedback", meta = (ClampMin = "0.0"))
	float DistanceScaleFarRange = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Feedback", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float DistanceScaleMin = 0.7f;

	/** 카메라→목표 라인트레이스로 가림 여부를 판정해 옅게 그린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|ObjectiveMarker|Feedback")
	bool bTraceOcclusion = true;

	/** BP/치트에서 전체 표시를 임시로 끌 때. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|ObjectiveMarker")
	void SetMarkersSuppressed(bool bSuppressed) { bSuppressedByRequest = bSuppressed; }

	/** 모든 마커의 등장 연출을 다시 재생한다(목표 재확인 핫키/리스폰). */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|ObjectiveMarker")
	void ReplayAppearEffects();

protected:
	virtual bool NativeIsInteractable() const override { return false; }
	virtual bool NativeSupportsKeyboardFocus() const override { return false; }

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> CanvasPanel_Markers;

private:
	/** 매 프레임(또는 폴백 타이머)마다 마커를 다시 계산해 배치한다. */
	void UpdateMarkers(const FGeometry& MyGeometry);

	/** 캐시된 지오메트리로 갱신(NativeTick이 오지 않을 때의 폴백). */
	void UpdateFromCachedGeometry();

	/** 1초 내 NativeTick이 없으면 폴백 타이머로 전환. */
	void CheckTickWatchdog();

	/** BindWidget이 비어 있어도 쓸 캔버스를 찾는다(루트 → 트리 탐색). */
	UCanvasPanel* ResolveMarkerCanvas();

	/** 실제로 사용할 캔버스(바인딩 또는 폴백). */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> ResolvedCanvas;

	bool bTickReceived = false;
	bool bLoggedFirstUpdate = false;

	FTimerHandle TickWatchdogHandle;
	FTimerHandle FallbackUpdateHandle;

	/** 이번 프레임에 그릴 마커 1개의 계산 결과. */
	struct FResolvedMarker
	{
		FName MarkerId;
		ERetrieveObjectiveMarkerKind Kind = ERetrieveObjectiveMarkerKind::Side;
		FText Label;
		FText ProgressText;
		FVector2D LocalPosition = FVector2D::ZeroVector;
		float DistanceUU = 0.0f;
		float EdgeAngleDeg = 0.0f;
		bool bOffscreen = false;
		bool bApproximate = false;
		int32 SortPriority = 0;
		bool bShowLabel = false;
		bool bOccluded = false;
	};

	/** MarkerId → 마커가 처음 화면에 잡힌 시각. 등장 연출 진행도 계산용. */
	TMap<FName, double> MarkerAppearTimes;

	/** 서브시스템 마커를 화면 좌표로 변환하고 컬링/정렬/상한을 적용한다. */
	void ResolveMarkers(const FGeometry& MyGeometry, TArray<FResolvedMarker>& OutResolved) const;

	/** 겹친 마커의 라벨을 접는다. */
	void ApplyDeclutter(TArray<FResolvedMarker>& InOutResolved) const;

	/** 풀에서 위젯을 꺼내(없으면 생성) 위치/데이터를 적용한다. */
	void UpdateMarkerWidgets(const TArray<FResolvedMarker>& Resolved);

	/** 이번 프레임에 쓰이지 않은 풀 위젯을 접는다. */
	void CollapseUnusedMarkers(const TSet<FName>& ActiveIds);

	bool ShouldHideAllMarkers() const;

	void HandleObjectiveReminder(FGameplayTag Channel, const struct FRetrieveObjectiveReminderPayload& Message);

	FGameplayMessageListenerHandle ReminderHandle;

	URetrieveObjectiveMarkerSubsystem* GetMarkerSubsystem() const;

	/** MarkerId → 풀 위젯. 같은 목표는 항상 같은 위젯을 써서 애니메이션 상태를 유지한다. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<URetrieveObjectiveMarkerWidget>> MarkerWidgetPool;

	bool bSuppressedByRequest = false;
};
