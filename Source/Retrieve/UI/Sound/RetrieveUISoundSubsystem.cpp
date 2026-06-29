#include "UI/Sound/RetrieveUISoundSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundBase.h"
#include "UI/Sound/RetrieveUISoundPreset.h"
#include "UI/Sound/RetrieveUISoundRegistry.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"

void URetrieveUISoundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FString RegistryPath;
	if (GConfig->GetString(
		TEXT("/Script/Retrieve.RetrieveUISoundSubsystem"),
		TEXT("DefaultRegistryPath"),
		RegistryPath,
		GGameIni))
	{
		ActiveRegistry = LoadObject<URetrieveUISoundRegistry>(nullptr, *RegistryPath);
	}
}

void URetrieveUISoundSubsystem::SetActiveRegistry(URetrieveUISoundRegistry* NewRegistry)
{
	ActiveRegistry = NewRegistry;
}

const URetrieveUISoundPreset* URetrieveUISoundSubsystem::ResolvePreset(const URetrieveUIVFXWidget* Widget) const
{
	if (!Widget)
	{
		return nullptr;
	}

	if (Widget->bOverrideSoundPreset && Widget->SoundPresetOverride)
	{
		return Widget->SoundPresetOverride;
	}

	if (!ActiveRegistry)
	{
		return nullptr;
	}

	UClass* WidgetClass = Widget->GetClass();
	for (int32 Depth = 0; WidgetClass && Depth < 3; ++Depth, WidgetClass = WidgetClass->GetSuperClass())
	{
		const TSubclassOf<URetrieveUIVFXWidget> WidgetSubclass(WidgetClass);
		if (const TObjectPtr<URetrieveUISoundPreset>* FoundPreset = ActiveRegistry->ClassPresets.Find(WidgetSubclass))
		{
			if (*FoundPreset)
			{
				return FoundPreset->Get();
			}
		}
	}

	return ActiveRegistry->FallbackPreset;
}

USoundBase* URetrieveUISoundSubsystem::ResolveSound(
	const URetrieveUIVFXWidget* Widget,
	ERetrieveUISoundEvent Event) const
{
	if (const URetrieveUISoundPreset* Preset = ResolvePreset(Widget))
	{
		return Preset->GetSoundForEvent(Event);
	}

	return nullptr;
}

void URetrieveUISoundSubsystem::PlayUISound(
	const URetrieveUIVFXWidget* Widget,
	ERetrieveUISoundEvent Event) const
{
	if (!Widget)
	{
		return;
	}

	const URetrieveUISoundPreset* Preset = ResolvePreset(Widget);
	if (!Preset)
	{
		return;
	}

	USoundBase* Sound = Preset->GetSoundForEvent(Event);
	if (!Sound)
	{
		return;
	}

	const float Volume = FMath::Max(0.0f, Preset->VolumeMultiplier);
	UGameplayStatics::PlaySound2D(Widget, Sound, Volume);
}
