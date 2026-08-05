#include "UI/HUD/RetrieveObjectiveMarkerWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

FLinearColor URetrieveObjectiveMarkerWidget::GetColorForKind(ERetrieveObjectiveMarkerKind Kind) const
{
	switch (Kind)
	{
	case ERetrieveObjectiveMarkerKind::Main:
		return MainColor;
	case ERetrieveObjectiveMarkerKind::TurnIn:
		return TurnInColor;
	case ERetrieveObjectiveMarkerKind::Offer:
		return OfferColor;
	case ERetrieveObjectiveMarkerKind::MainOptional:
		return MainOptionalColor;
	case ERetrieveObjectiveMarkerKind::Side:
	default:
		return SideColor;
	}
}

void URetrieveObjectiveMarkerWidget::PlayAppearEffect()
{
	if (AppearAnimationName.IsNone())
	{
		return;
	}

	// 위젯 애니메이션 목록은 런타임에 생성 클래스가 들고 있다.
	// WBP에 같은 이름의 애니메이션이 있을 때만 재생한다(없어도 조용히 넘어간다).
	const UWidgetBlueprintGeneratedClass* BPClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass());
	if (!BPClass)
	{
		return;
	}

	for (UWidgetAnimation* Animation : BPClass->Animations)
	{
		if (Animation && Animation->GetFName() == AppearAnimationName)
		{
			PlayAnimation(Animation, 0.0f, FMath::Max(AppearAnimationLoops, 0));
			return;
		}
	}
}

void URetrieveObjectiveMarkerWidget::ApplyVisual(const FRetrieveObjectiveMarkerVisual& InVisual)
{
	// 아이콘이 UImage가 아니라 서브 위젯(FantasyWarrior 오브젝티브 아이콘)인 경우가 많아,
	// 종류별 색은 마커 위젯 전체 틴트로 적용한다. 알파는 RenderOpacity로 따로 준다
	// (틴트 알파와 곱해져 두 번 적용되는 것을 피하기 위해 여기서는 A=1 유지).
	FLinearColor Color = GetColorForKind(InVisual.Kind);
	Color.A = 1.0f;
	SetColorAndOpacity(Color);

	if (Image_Icon)
	{
		Image_Icon->SetColorAndOpacity(Color);
	}

	if (Image_KindIcon)
	{
		// 종류별 텍스처가 지정된 경우에만 표시(미지정이면 기본 다이아만 남는다).
		UTexture2D* KindTexture = nullptr;
		if (const TObjectPtr<UTexture2D>* Found = KindIcons.Find(InVisual.Kind))
		{
			KindTexture = Found->Get();
		}

		// 기본 다이아 아이콘(플러그인 서브 위젯). 종류 아이콘을 쓸 때는 이걸 숨겨서
		// 색이 아니라 "모양"으로 구분되게 한다 — 겹쳐 그리면 같은 색이라 묻혀 보인다.
		// BindWidget 대신 이름 조회를 쓰는 이유: 프로퍼티 추가는 구조 변경이라
		// Live Coding 재인스턴싱 위험이 있어서(이 세션에서 실제로 크래시가 났다).
		UWidget* DefaultIcon = GetWidgetFromName(TEXT("HUD_ICON"));

		if (KindTexture)
		{
			// bMatchSize=false: 슬롯에 지정한 크기를 그대로 쓴다(텍스처 256px에 끌려가지 않게).
			Image_KindIcon->SetBrushFromTexture(KindTexture, false);
			Image_KindIcon->SetVisibility(ESlateVisibility::HitTestInvisible);

			if (DefaultIcon)
			{
				DefaultIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
			Image_KindIcon->SetVisibility(ESlateVisibility::Collapsed);

			if (DefaultIcon)
			{
				DefaultIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		}
	}

	if (Image_Arrow)
	{
		// 화살표는 화면 밖일 때만. 회전은 목표 방향(도) 그대로.
		Image_Arrow->SetVisibility(InVisual.bOffscreen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (InVisual.bOffscreen)
		{
			Image_Arrow->SetColorAndOpacity(Color);
			Image_Arrow->SetRenderTransformAngle(InVisual.EdgeAngleDeg);
		}
	}

	if (Text_Distance)
	{
		// "12m" 표기. 소수점은 버리고, 천 단위 구분(1,234m)은 쓰지 않는다.
		Text_Distance->SetText(FText::FromString(
			FString::Printf(TEXT("%dm"), FMath::FloorToInt(InVisual.DistanceMeters))));
		Text_Distance->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (Text_Label)
	{
		const bool bShow = InVisual.bShowLabel && !InVisual.Label.IsEmptyOrWhitespace();
		Text_Label->SetText(InVisual.Label);
		Text_Label->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Text_Progress)
	{
		const bool bShow = InVisual.bShowLabel && !InVisual.ProgressText.IsEmptyOrWhitespace();
		Text_Progress->SetText(InVisual.ProgressText);
		Text_Progress->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 근접 페이드 + 지역 단위 흐림 + 가림 흐림을 알파 한 곳에 모은다.
	float Alpha = InVisual.Opacity;
	if (InVisual.bApproximate)
	{
		Alpha *= ApproximateAlphaScale;
	}
	if (InVisual.bOccluded)
	{
		Alpha *= OccludedAlphaScale;
	}
	SetRenderOpacity(Alpha);

	// 등장 스케일 인 + 메인 마커 호흡 펄스 + 거리 스케일(레이어에서 합산된 값).
	SetRenderScale(FVector2D(InVisual.ScaleMultiplier, InVisual.ScaleMultiplier));

	OnMarkerUpdated(InVisual);
}
