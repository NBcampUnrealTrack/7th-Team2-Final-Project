#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "RetrieveCreditsWidget.generated.h"

class UScrollBox;

/** 크레딧이 마무리될 때 브로드캐스트. bWasSkipped=true면 사용자가 스킵한 것, false면 끝까지 재생됨. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRetrieveCreditsCompletedSignature, bool, bWasSkipped);

/**
 * 영화 엔딩 크레딧 스타일의 자동 스크롤 위젯.
 * URetrieveGamePanelWidget을 상속하므로 ARetrievePlayerController::OpenCreditsPanel()이 그대로 표시한다.
 *
 * 재사용: 모든 동작이 프로퍼티로 노출되어 있고(메인메뉴/게임 엔딩 등 상황별로 다르게 설정 가능),
 * OnCreditsCompleted 델리게이트로 "크레딧 종료 후 처리"(예: 엔딩에서 메인메뉴로 이동)를
 * 위젯 수정 없이 외부에서 붙일 수 있다. 다른 WBP 변형은 OpenExclusivePanel(<클래스>, ESC)로 열면 된다.
 *
 * WBP_Credits는 이 클래스를 상속해 다음만 구성하면 된다:
 *  - 루트: 불투명한 검은 배경(전체 화면).
 *  - "CreditsScrollBox"라는 이름의 UScrollBox(세로 텍스트 목록). 위/아래에 화면 높이만큼의
 *    빈 여백(Spacer)을 두면 검은 화면에서 텍스트가 올라오고 검은 화면으로 사라진다.
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveCreditsWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

public:
	/** 크레딧이 마무리될 때(자연 종료 또는 스킵) 브로드캐스트. 게임 엔딩 등 외부 흐름에서 바인딩해
	 *  "크레딧 종료 → 메인메뉴 이동" 같은 후처리를 붙일 수 있다. bWasSkipped로 스킵 여부를 구분한다. */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Credits")
	FRetrieveCreditsCompletedSignature OnCreditsCompleted;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

	/** 자동 스크롤할 크레딧 목록 컨테이너. WBP에서 이 이름(CreditsScrollBox)의 ScrollBox를 배치한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> CreditsScrollBox;

	// ── 스크롤 ────────────────────────────────────────────────
	/** 초당 스크롤 이동량(슬레이트 유닛). 양수면 텍스트가 위로 굴러 올라간다(영화 엔딩 크레딧). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Credits", meta = (ClampMin = "0.0"))
	float ScrollSpeed = 60.0f;

	/** 스크롤 시작 전 대기 시간(초). 검은 화면에서 잠시 정지 후 시작한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Credits", meta = (ClampMin = "0.0"))
	float StartDelay = 1.0f;

	/** 끝에 도달한 뒤 마무리(자동 닫기)까지 검은 화면을 유지하는 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Credits", meta = (ClampMin = "0.0"))
	float EndHoldTime = 2.0f;

	/** true면 끝에 도달하고 EndHoldTime 후 자동으로 패널을 닫는다. 엔딩에서 끄고 OnCreditsCompleted로 직접 처리해도 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Credits")
	bool bAutoCloseAtEnd = true;

	/** true면 끝에 도달해도 닫지 않고 처음으로 돌아가 반복한다(bAutoCloseAtEnd보다 우선). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Credits")
	bool bLoop = false;

	// ── 스킵(닫기) ────────────────────────────────────────────
	/** true면 ESC(또는 패널 ToggleKey)로 크레딧을 스킵/닫을 수 있다. false면 해당 입력을 삼켜 스킵을 막는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Credits|Skip")
	bool bAllowSkip = true;

	// ── 빨리 감기 ─────────────────────────────────────────────
	/** true면 FastForwardKey를 누르고 있는 동안 스크롤 속도가 FastForwardMultiplier배가 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Credits|FastForward")
	bool bAllowFastForward = true;

	/** 누르고 있는 동안 빨리 감기가 되는 키. 기본 Space Bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Credits|FastForward")
	FKey FastForwardKey = EKeys::SpaceBar;

	/** 빨리 감기 시 스크롤 속도 배수. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Credits|FastForward", meta = (ClampMin = "1.0"))
	float FastForwardMultiplier = 4.0f;

	/** 크레딧이 자연 종료(끝까지 스크롤)될 때 호출되는 BP 훅. 스킵 시에는 호출되지 않는다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Credits")
	void OnCreditsFinished();

private:
	/** ESC 또는 주입된 ToggleKey인지. */
	bool IsSkipKey(const FKey& Key) const;

	/** 마무리 처리(1회만). 자연 종료면 OnCreditsFinished도 호출하고, 항상 OnCreditsCompleted를 브로드캐스트한다. */
	void CompleteCredits(bool bSkipped);

	float ElapsedTime = 0.0f;
	float EndHoldElapsed = 0.0f;
	bool bReachedEnd = false;
	bool bCompleted = false;
	bool bFastForwarding = false;
};
