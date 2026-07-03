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

bool URetrieveUISoundSubsystem::PlayContextUISound(
	const URetrieveUIVFXWidget* Widget,
	FGameplayTag ContextTag,
	ERetrieveUISoundEvent FallbackEvent) const
{
	if (!Widget)
	{
		return false;
	}

	const URetrieveUISoundPreset* Preset = ResolvePreset(Widget);
	if (!Preset)
	{
		return false;
	}

	const URetrieveUISoundPreset* ContextPreset = Preset;
	const FRetrieveUIContextSound* ContextSound = ContextPreset->FindContextSound(ContextTag);
	if (!ContextSound && ActiveRegistry && ActiveRegistry->FallbackPreset && ActiveRegistry->FallbackPreset != Preset)
	{
		ContextPreset = ActiveRegistry->FallbackPreset;
		ContextSound = ContextPreset->FindContextSound(ContextTag);
	}

	if (ContextSound)
	{
		if (ContextSound->Sound)
		{
			const float Volume = FMath::Max(0.0f, ContextPreset->VolumeMultiplier * ContextSound->VolumeMultiplier);
			const float Pitch = FMath::Max(0.01f, ContextSound->PitchMultiplier);
			UGameplayStatics::PlaySound2D(Widget, ContextSound->Sound, Volume, Pitch);
			return true;
		}
	}

	if (USoundBase* FallbackSound = Preset->GetSoundForEvent(FallbackEvent))
	{
		UGameplayStatics::PlaySound2D(Widget, FallbackSound, FMath::Max(0.0f, Preset->VolumeMultiplier));
		return true;
	}

	return false;
}
