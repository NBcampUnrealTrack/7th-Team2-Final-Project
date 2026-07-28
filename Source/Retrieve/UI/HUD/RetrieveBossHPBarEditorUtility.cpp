#include "UI/HUD/RetrieveBossHPBarEditorUtility.h"

#if WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "Types/MVVMBindingMode.h"
#include "Types/MVVMFieldVariant.h"
#include "UI/ViewModels/BossStatusViewModel.h"
#include "WidgetBlueprint.h"

FMVVMBlueprintPropertyPath URetrieveBossHPBarEditorUtility::MakeViewModelPath(
	const UWidgetBlueprint* WidgetBlueprint,
	FGuid ViewModelId,
	FName FunctionName)
{
	FMVVMBlueprintPropertyPath Path;
	Path.SetViewModelId(ViewModelId);

	if (const UFunction* Function = UBossStatusViewModel::StaticClass()->FindFunctionByName(FunctionName))
	{
		Path.AppendPropertyPath(WidgetBlueprint, UE::MVVM::FMVVMConstFieldVariant(Function));
	}

	return Path;
}

FMVVMBlueprintPropertyPath URetrieveBossHPBarEditorUtility::MakeWidgetPropertyPath(
	const UWidgetBlueprint* WidgetBlueprint,
	UWidget* Widget,
	FName PropertyName)
{
	FMVVMBlueprintPropertyPath Path;
	if (!Widget)
	{
		return Path;
	}

	if (const FProperty* Property = Widget->GetClass()->FindPropertyByName(PropertyName))
	{
		Path.SetWidgetName(Widget->GetFName());
		Path.AppendPropertyPath(WidgetBlueprint, UE::MVVM::FMVVMConstFieldVariant(Property));
	}

	return Path;
}

bool URetrieveBossHPBarEditorUtility::AddOneWayBinding(
	UMVVMEditorSubsystem* EditorSubsystem,
	UWidgetBlueprint* WidgetBlueprint,
	FGuid ViewModelId,
	FName SourceFunctionName,
	UWidget* DestinationWidget,
	FName DestinationPropertyName)
{
	if (!EditorSubsystem || !WidgetBlueprint || !DestinationWidget)
	{
		return false;
	}

	FMVVMBlueprintPropertyPath SourcePath = MakeViewModelPath(WidgetBlueprint, ViewModelId, SourceFunctionName);
	FMVVMBlueprintPropertyPath DestinationPath = MakeWidgetPropertyPath(WidgetBlueprint, DestinationWidget, DestinationPropertyName);
	if (!SourcePath.HasPaths() || !DestinationPath.HasPaths())
	{
		return false;
	}

	FMVVMBlueprintViewBinding& Binding = EditorSubsystem->AddBinding(WidgetBlueprint);
	EditorSubsystem->SetSourcePathForBinding(WidgetBlueprint, Binding, SourcePath);
	EditorSubsystem->SetDestinationPathForBinding(WidgetBlueprint, Binding, DestinationPath, false);
	EditorSubsystem->SetBindingTypeForBinding(WidgetBlueprint, Binding, EMVVMBindingMode::OneWayToDestination);
	return true;
}

#endif

bool URetrieveBossHPBarEditorUtility::ConfigureBossHPBarMVVMBindings(const FString& AssetPath)
{
#if WITH_EDITOR
	UWidgetBlueprint* WidgetBlueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		UE_LOG(LogTemp, Error, TEXT("[Retrieve|BossHP] Widget blueprint not found or has no tree: %s"), *AssetPath);
		return false;
	}

	UMVVMEditorSubsystem* EditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>() : nullptr;
	if (!EditorSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[Retrieve|BossHP] MVVMEditorSubsystem is unavailable."));
		return false;
	}

	UMVVMBlueprintView* View = EditorSubsystem->RequestView(WidgetBlueprint);
	if (!View)
	{
		UE_LOG(LogTemp, Error, TEXT("[Retrieve|BossHP] MVVM view is unavailable: %s"), *AssetPath);
		return false;
	}

	FGuid BossStatusId;
	if (const FMVVMBlueprintViewModelContext* BossStatusContext = View->FindViewModel(TEXT("BossStatus")))
	{
		BossStatusId = BossStatusContext->GetViewModelId();
	}
	else if (const FMVVMBlueprintViewModelContext* LegacyContext = View->FindViewModel(TEXT("BossStatusViewModel")))
	{
		BossStatusId = LegacyContext->GetViewModelId();
		FText RenameError;
		EditorSubsystem->RenameViewModel(WidgetBlueprint, TEXT("BossStatusViewModel"), TEXT("BossStatus"), RenameError);
	}
	else
	{
		BossStatusId = EditorSubsystem->AddViewModel(WidgetBlueprint, UBossStatusViewModel::StaticClass());
		FText RenameError;
		EditorSubsystem->RenameViewModel(WidgetBlueprint, TEXT("BossStatusViewModel"), TEXT("BossStatus"), RenameError);
	}

	if (FMVVMBlueprintViewModelContext* BossStatusContext = View->FindViewModel(BossStatusId))
	{
		BossStatusId = BossStatusContext->GetViewModelId();
		BossStatusContext->CreationType = EMVVMBlueprintViewModelContextCreationType::Manual;
		BossStatusContext->bCreateSetterFunction = true;
		BossStatusContext->bCreateGetterFunction = true;
		BossStatusContext->bOptional = true;
	}

	if (!BossStatusId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Retrieve|BossHP] BossStatus ViewModel context could not be created."));
		return false;
	}

	while (View->GetNumBindings() > 0)
	{
		View->RemoveBindingAt(0);
	}

	UWidget* NameText = WidgetBlueprint->WidgetTree->FindWidget(TEXT("TXT_Name"));
	UWidget* HPText = WidgetBlueprint->WidgetTree->FindWidget(TEXT("TXT_HP"));
	UWidget* RootPanel = WidgetBlueprint->WidgetTree->RootWidget;
	UWidget* FantasyHPBar = WidgetBlueprint->WidgetTree->FindWidget(TEXT("HUD_HealthBar_Enemy"));

	int32 AddedCount = 0;
	AddedCount += AddOneWayBinding(EditorSubsystem, WidgetBlueprint, BossStatusId, GET_FUNCTION_NAME_CHECKED(UBossStatusViewModel, GetBossName), NameText, TEXT("Text")) ? 1 : 0;
	AddedCount += AddOneWayBinding(EditorSubsystem, WidgetBlueprint, BossStatusId, GET_FUNCTION_NAME_CHECKED(UBossStatusViewModel, GetHealthText), HPText, TEXT("Text")) ? 1 : 0;
	AddedCount += AddOneWayBinding(EditorSubsystem, WidgetBlueprint, BossStatusId, GET_FUNCTION_NAME_CHECKED(UBossStatusViewModel, GetSlateVisibility), RootPanel, TEXT("Visibility")) ? 1 : 0;
	AddedCount += AddOneWayBinding(EditorSubsystem, WidgetBlueprint, BossStatusId, GET_FUNCTION_NAME_CHECKED(UBossStatusViewModel, GetDisplayedHealthFraction), FantasyHPBar, TEXT("FillAmount")) ? 1 : 0;

	WidgetBlueprint->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	UE_LOG(LogTemp, Log, TEXT("[Retrieve|BossHP] Configured %d MVVM bindings on %s."), AddedCount, *AssetPath);
	return AddedCount == 4;
#else
	return false;
#endif
}
