#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RetrieveInteractionTypeAsset.generated.h"

class UAnimMontage;
class UTexture2D;
class UUserWidget;

/**
 * 상호작용 종류(픽업·상자 열기·채광·전리품 등)별 공용 설정 DataAsset.
 *
 * ─ 이 에셋 하나로 관리되는 것들 ───────────────────────────────────────────
 *   · 프롬프트 텍스트 ("아이템 획득" / "상자 열기" 등)
 *   · 상호작용 방식 (Tap / Hold + Hold 지속 시간)
 *   · 프롬프트 위젯 비주얼 (아이콘, 강조색, 위젯 클래스)
 *   · 완료 시 재생되는 AnimMontage
 *
 * ─ 동작 흐름 ─────────────────────────────────────────────────────────────
 *   BeginPlay → ResponseComponent.TryAutoBindInteractionManager
 *     → ApplyTypeAssetToManagerInternal
 *       → HoldSeconds / InteractionType / InteractionText
 *       → PromptIcon / PromptAccentColor / WidgetClassOverride
 *         (Manager 프로퍼티 이름을 MgrProp_* 필드로 매핑)
 *
 * ─ 상용 에셋마다 프로퍼티 이름이 다를 경우 ────────────────────────────────
 *   "Interaction|Widget|상용에셋 연동" 카테고리의 MgrProp_* 이름을
 *   실제 Manager BP 컴포넌트의 프로퍼티 이름과 맞게 수정하면 됩니다.
 *   찾지 못한 프로퍼티는 경고 없이 스킵됩니다(Verbose 로그만 출력).
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveInteractionTypeAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ── 프롬프트 텍스트 ────────────────────────────────────────────────────
	/**
	 * Manager_InteractionTarget 프롬프트에 표시되는 행동 레이블.
	 * 예: "아이템 획득" / "상자 열기" / "채광" / "전리품 획득"
	 * BeginPlay 시 Manager_InteractionTarget.InteractionText[0]에 자동 적용됨.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Prompt",
		meta = (DisplayName = "프롬프트 텍스트"))
	FText DisplayText = INVTEXT("상호작용");

	// ── Hold 설정 ──────────────────────────────────────────────────────────
	/**
	 * true = Hold (누르고 있기) → 게이지가 차오르는 시각 피드백
	 * false = Tap (단타) → 0.25s 빠른 Hold로 단발 피드백 효과
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Hold",
		meta = (DisplayName = "Hold 방식 사용"))
	bool bHoldInteraction = false;

	/**
	 * Hold 지속 시간(초). bHoldInteraction = true일 때만 의미 있음.
	 * 권장값:
	 *   단발 피드백(픽업/전리품) : 0.25
	 *   상자 열기               : 1.5
	 *   채광                   : 2.0
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Hold",
		meta = (DisplayName = "Hold 지속 시간(초)", EditCondition = "bHoldInteraction", ClampMin = "0.05"))
	float HoldDuration = 1.0f;

	// ── 애니메이션 ─────────────────────────────────────────────────────────
	/**
	 * 상호작용 완료 시 Instigator(플레이어 캐릭터)에게 재생되는 Anim Montage.
	 * None이면 재생 안 함.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Animation",
		meta = (DisplayName = "완료 애니메이션 몽타주"))
	TObjectPtr<UAnimMontage> InteractionMontage;

	// ── 프롬프트 위젯 비주얼 ─────────────────────────────────────────────
	/**
	 * 상호작용 종류를 나타내는 아이콘 텍스처.
	 * 예: 열쇠 아이콘(문), 손 아이콘(픽업), 곡괭이 아이콘(채광)
	 *
	 * Manager 컴포넌트의 아이콘 프로퍼티(MgrProp_Icon에 지정된 이름)에
	 * BeginPlay 시 자동 적용된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Widget|비주얼",
		meta = (DisplayName = "프롬프트 아이콘"))
	TObjectPtr<UTexture2D> PromptIcon;

	/**
	 * 프롬프트 강조색 — Hold 게이지, 테두리, 키 힌트 배경 등에 사용.
	 * 상호작용 종류별로 색을 구분하면 플레이어가 한눈에 파악할 수 있다.
	 *
	 * 권장:
	 *   픽업(아이템)  : 금색  (0.78, 0.63, 0.13)
	 *   상자 열기     : 청록색 (0.20, 0.70, 0.90)
	 *   채광          : 주황색 (0.90, 0.45, 0.10)
	 *   전리품/몬스터 : 붉은색 (0.85, 0.15, 0.15)
	 *   대화/문       : 흰색   (1.00, 1.00, 1.00)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Widget|비주얼",
		meta = (DisplayName = "강조색"))
	FLinearColor PromptAccentColor = FLinearColor(0.78f, 0.63f, 0.13f, 1.0f);

	/**
	 * 이 상호작용 타입에서만 사용할 커스텀 위젯 클래스.
	 * None이면 Manager의 기본 위젯 클래스 그대로 사용.
	 *
	 * 예: 대화 상호작용은 다른 디자인의 위젯 사용, 채광은 원형 게이지 위젯 등.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|Widget|비주얼",
		meta = (DisplayName = "커스텀 위젯 클래스 (None = 기본)"))
	TSoftClassPtr<UUserWidget> WidgetClassOverride;

	// ── 상용 에셋 연동 (Advanced) ─────────────────────────────────────────
	/**
	 * 아래 MgrProp_* 값들은 Manager_InteractionTarget BP 컴포넌트에서
	 * 아이콘·색상·위젯 클래스를 받는 프로퍼티의 이름입니다.
	 *
	 * ※ 상용 에셋이 업데이트되거나 다른 에셋으로 교체할 때
	 *   이 이름만 바꾸면 C++ 수정 없이 연동을 유지할 수 있습니다.
	 * ※ Manager에 해당 프로퍼티가 없으면 조용히 스킵합니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "Interaction|Widget|상용에셋 연동 (Advanced)",
		meta = (DisplayName = "아이콘 프로퍼티 이름"))
	FName MgrProp_Icon = TEXT("InteractionIcon");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "Interaction|Widget|상용에셋 연동 (Advanced)",
		meta = (DisplayName = "색상 프로퍼티 이름"))
	FName MgrProp_Color = TEXT("InteractionColor");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "Interaction|Widget|상용에셋 연동 (Advanced)",
		meta = (DisplayName = "위젯 클래스 프로퍼티 이름"))
	FName MgrProp_WidgetClass = TEXT("InteractionWidget");
};
