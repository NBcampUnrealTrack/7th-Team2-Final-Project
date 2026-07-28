#include "Data/RetrieveMapIconRegistry.h"

const FRetrieveMapIconRow& URetrieveMapIconRegistry::FindRow(ERetrieveMapIconType IconType) const
{
	if (IconDataTable)
	{
		const FName RowName = IconTypeToRowName(IconType);
		if (const FRetrieveMapIconRow* Row = IconDataTable->FindRow<FRetrieveMapIconRow>(RowName, TEXT("")))
		{
			return *Row;
		}
	}
	return FallbackRow;
}

FName URetrieveMapIconRegistry::IconTypeToRowName(ERetrieveMapIconType IconType)
{
	// ERetrieveMapIconType::POI → "POI"
	const UEnum* EnumClass = StaticEnum<ERetrieveMapIconType>();
	if (!EnumClass)
	{
		return NAME_None;
	}

	const FString EnumStr = EnumClass->GetNameStringByValue(static_cast<int64>(IconType));
	return FName(*EnumStr);
}
