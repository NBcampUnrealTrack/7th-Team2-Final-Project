#include "UI/VFX/RetrieveUIVFXProfile.h"

bool URetrieveUIVFXProfile::GetPreset(FGameplayTag EffectTag, FRetrieveUIVFXPreset& OutPreset) const
{
	if (!EffectTag.IsValid())
	{
		return false;
	}

	for (const FRetrieveUIVFXPreset& Preset : Presets)
	{
		if (Preset.EffectTag == EffectTag || Preset.EffectName == EffectTag.GetTagName())
		{
			OutPreset = Preset;
			return true;
		}
	}

	return false;
}
