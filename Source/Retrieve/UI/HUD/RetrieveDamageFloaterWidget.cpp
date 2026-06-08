// Fill out your copyright notice in the Description page of Project Settings.


#include "RetrieveDamageFloaterWidget.h"

#include "Components/TextBlock.h"

void URetrieveDamageFloaterWidget::Activate(float DamageValue, float ScaleMultiplier, const FLinearColor& Color)
{
	// 데이터 주입
	if (IsValid(DamageText))
	{
		DamageText->SetText(FText::AsNumber(FMath::RoundToInt(DamageValue)));
		DamageText->SetColorAndOpacity(FSlateColor(Color));
		DamageText->SetRenderScale(FVector2D(ScaleMultiplier, ScaleMultiplier));
	}
	// 비주얼은 WBP가 담당(상승, 페이드 애니)
	PlayFloaterAnim();
}

void URetrieveDamageFloaterWidget::NotifyFinished()
{
	OnFinished.ExecuteIfBound(this);
}
