#include "Data/Interaction/RetrieveInteractionPresetProfileAsset.h"

bool URetrieveInteractionPresetProfileAsset::GetPreset(FName PresetId, FRetrieveInteractionPresetData& OutPreset) const
{
	if (PresetId.IsNone())
	{
		return false;
	}

	for (const FRetrieveInteractionPresetData& Preset : Presets)
	{
		if (Preset.PresetId == PresetId)
		{
			OutPreset = Preset;
			return true;
		}
	}

	return false;
}
