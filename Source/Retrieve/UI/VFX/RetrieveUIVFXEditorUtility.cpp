#include "UI/VFX/RetrieveUIVFXEditorUtility.h"

#include "Curves/CurveFloat.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "UI/VFX/RetrieveUIVFXProfile.h"

namespace
{
UCurveFloat* LoadCurve(const TCHAR* Path)
{
	return LoadObject<UCurveFloat>(nullptr, Path);
}

void SetCurveKeys(UCurveFloat* Curve, const TArray<TPair<float, float>>& Keys)
{
	if (!Curve)
	{
		return;
	}

	Curve->Modify();
	Curve->FloatCurve.Reset();

	for (const TPair<float, float>& Key : Keys)
	{
		const FKeyHandle Handle = Curve->FloatCurve.AddKey(Key.Key, Key.Value);
		Curve->FloatCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Cubic);
	}

	Curve->MarkPackageDirty();
}

FRetrieveUIVFXPreset MakePreset(
	FGameplayTag EffectTag,
	UCurveFloat* Curve,
	float Duration,
	FVector2D StartTranslation,
	FVector2D EndTranslation,
	FVector2D StartScale,
	FVector2D EndScale,
	float StartOpacity,
	float EndOpacity)
{
	FRetrieveUIVFXPreset Preset;
	Preset.EffectTag = EffectTag;
	Preset.EffectName = EffectTag.GetTagName();
	Preset.Curve = Curve;
	Preset.Duration = Duration;
	Preset.StartTranslation = StartTranslation;
	Preset.EndTranslation = EndTranslation;
	Preset.StartScale = StartScale;
	Preset.EndScale = EndScale;
	Preset.StartOpacity = StartOpacity;
	Preset.EndOpacity = EndOpacity;
	return Preset;
}
}

bool URetrieveUIVFXEditorUtility::ConfigureRecommendedUIVFXAssets()
{
	URetrieveUIVFXProfile* Profile = LoadObject<URetrieveUIVFXProfile>(
		nullptr,
		TEXT("/Game/Retrieve/UI/VFX/DA_UIVFX_Default.DA_UIVFX_Default"));
	if (!Profile)
	{
		UE_LOG(LogTemp, Error, TEXT("[Retrieve|UIVFX] DA_UIVFX_Default was not found."));
		return false;
	}

	UCurveFloat* PanelIn = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_PanelSlide_In.C_UI_PanelSlide_In"));
	UCurveFloat* PanelOut = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_PanelSlide_Out.C_UI_PanelSlide_Out"));
	UCurveFloat* GaugePulse = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_GaugePulse.C_UI_GaugePulse"));
	UCurveFloat* IconFlash = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_IconFlash.C_UI_IconFlash"));
	UCurveFloat* ButtonHover = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_ButtonHover.C_UI_ButtonHover"));
	UCurveFloat* ButtonUnhover = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_ButtonUnhover.C_UI_ButtonUnhover"));
	UCurveFloat* ButtonPress = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_ButtonPress.C_UI_ButtonPress"));
	UCurveFloat* ButtonRelease = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_ButtonRelease.C_UI_ButtonRelease"));
	UCurveFloat* TabSwitch = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_TabSwitch.C_UI_TabSwitch"));

	SetCurveKeys(PanelIn, {
		{0.00f, 0.00f},
		{0.28f, 0.82f},
		{0.58f, 1.04f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(PanelOut, {
		{0.00f, 0.00f},
		{0.30f, 0.48f},
		{0.72f, 0.92f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(GaugePulse, {
		{0.00f, 0.00f},
		{0.18f, 1.22f},
		{0.46f, 0.90f},
		{0.74f, 1.06f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(IconFlash, {
		{0.00f, 0.00f},
		{0.18f, 1.18f},
		{0.48f, 0.96f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(ButtonHover, {
		{0.00f, 0.00f},
		{0.55f, 0.90f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(ButtonUnhover, {
		{0.00f, 0.00f},
		{0.55f, 0.90f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(ButtonPress, {
		{0.00f, 0.00f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(ButtonRelease, {
		{0.00f, 0.00f},
		{0.40f, 1.08f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(TabSwitch, {
		{0.00f, 0.00f},
		{0.35f, 0.76f},
		{1.00f, 1.00f},
	});

	Profile->Modify();
	Profile->Presets.Reset();
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Panel_Open,
		PanelIn,
		0.28f,
		FVector2D(0.0f, 42.0f),
		FVector2D::ZeroVector,
		FVector2D(0.97f, 0.97f),
		FVector2D(1.0f, 1.0f),
		0.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Panel_Close,
		PanelOut,
		0.20f,
		FVector2D::ZeroVector,
		FVector2D(0.0f, 32.0f),
		FVector2D(1.0f, 1.0f),
		FVector2D(0.98f, 0.98f),
		1.0f,
		0.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Gauge_FullPulse,
		GaugePulse,
		0.34f,
		FVector2D::ZeroVector,
		FVector2D::ZeroVector,
		FVector2D(1.0f, 1.0f),
		FVector2D(1.07f, 1.07f),
		1.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Icon_ItemAdded,
		IconFlash,
		0.30f,
		FVector2D(0.0f, 12.0f),
		FVector2D::ZeroVector,
		FVector2D(0.90f, 0.90f),
		FVector2D(1.0f, 1.0f),
		0.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Button_Hover,
		ButtonHover,
		0.12f,
		FVector2D::ZeroVector,
		FVector2D(0.0f, -2.0f),
		FVector2D(1.0f, 1.0f),
		FVector2D(1.035f, 1.035f),
		1.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Button_Unhover,
		ButtonUnhover,
		0.12f,
		FVector2D(0.0f, -2.0f),
		FVector2D::ZeroVector,
		FVector2D(1.035f, 1.035f),
		FVector2D(1.0f, 1.0f),
		1.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Button_Press,
		ButtonPress,
		0.07f,
		FVector2D::ZeroVector,
		FVector2D(0.0f, 1.0f),
		FVector2D(1.035f, 1.035f),
		FVector2D(0.965f, 0.965f),
		1.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Button_Release,
		ButtonRelease,
		0.12f,
		FVector2D(0.0f, 1.0f),
		FVector2D::ZeroVector,
		FVector2D(0.965f, 0.965f),
		FVector2D(1.02f, 1.02f),
		1.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Tab_Switch,
		TabSwitch,
		0.18f,
		FVector2D(10.0f, 0.0f),
		FVector2D::ZeroVector,
		FVector2D(0.995f, 0.995f),
		FVector2D(1.0f, 1.0f),
		0.65f,
		1.0f));

	Profile->MarkPackageDirty();
	UE_LOG(LogTemp, Log, TEXT("[Retrieve|UIVFX] Recommended UI VFX profile and curves configured."));
	return true;
}
