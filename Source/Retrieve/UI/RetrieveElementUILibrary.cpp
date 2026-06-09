#include "UI/RetrieveElementUILibrary.h"

#include "GameplayTags/RetrieveGameplayTags.h"

FLinearColor URetrieveElementUILibrary::ElementTagToColor(FGameplayTag ElementTag)
{
	if (ElementTag == RetrieveGameplayTags::Element_Fire)
	{
		return FLinearColor(1.0f, 0.45f, 0.0f, 1.0f);   // 주황 — 불
	}
	if (ElementTag == RetrieveGameplayTags::Element_Water)
	{
		return FLinearColor(0.1f, 0.6f, 1.0f, 1.0f);    // 파랑 — 물
	}
	if (ElementTag == RetrieveGameplayTags::Element_Wind)
	{
		return FLinearColor(0.35f, 1.0f, 0.35f, 1.0f);  // 초록 — 바람
	}

	// Element_None 또는 미정의 태그 → 회색 (빈 슬롯)
	return FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);
}
