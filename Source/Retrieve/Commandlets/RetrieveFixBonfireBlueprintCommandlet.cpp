#include "Commandlets/RetrieveFixBonfireBlueprintCommandlet.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "FileHelpers.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

#if WITH_EDITOR
namespace
{
	bool IsFireVFXSetActiveNode(const UK2Node_CallFunction* CallNode)
	{
		if (!CallNode || CallNode->FunctionReference.GetMemberName() != GET_FUNCTION_NAME_CHECKED(UActorComponent, SetActive))
		{
			return false;
		}

		const UEdGraphPin* SelfPin = CallNode->FindPin(TEXT("self"));
		if (!SelfPin)
		{
			return false;
		}

		for (const UEdGraphPin* LinkedPin : SelfPin->LinkedTo)
		{
			const UK2Node_VariableGet* VariableGet = LinkedPin
				? Cast<UK2Node_VariableGet>(LinkedPin->GetOwningNode())
				: nullptr;
			if (VariableGet && VariableGet->VariableReference.GetMemberName() == TEXT("FireVFXComponent"))
			{
				return true;
			}
		}

		return false;
	}
}
#endif

URetrieveFixBonfireBlueprintCommandlet::URetrieveFixBonfireBlueprintCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 URetrieveFixBonfireBlueprintCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	constexpr const TCHAR* BonfireBlueprintPath = TEXT("/Game/Retrieve/Blueprints/BP_BonFire.BP_BonFire");
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, BonfireBlueprintPath);
	if (!Blueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[BonfireFix] Failed to load %s"), BonfireBlueprintPath);
		return 1;
	}

	bool bRemovedAny = false;
	const FName LegacyVariableNames[] =
	{
		TEXT("bIsActive"),
		TEXT("IsActive"),
		TEXT("Is Active")
	};

	for (const FName LegacyVariableName : LegacyVariableNames)
	{
		if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, LegacyVariableName) != INDEX_NONE)
		{
			FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, LegacyVariableName);
			UE_LOG(LogTemp, Display, TEXT("[BonfireFix] Removed legacy variable: %s"), *LegacyVariableName.ToString());
			bRemovedAny = true;
		}
	}

	int32 RemovedActivateBonfireNodes = 0;
	int32 RemovedFireVFXSetActiveNodes = 0;
	TArray<UEdGraphNode*> NodesToRemove;

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
			if (!CallNode)
			{
				continue;
			}

			const FName FunctionName = CallNode->FunctionReference.GetMemberName();
			if (FunctionName == TEXT("ActivateBonfire"))
			{
				NodesToRemove.Add(Node);
				++RemovedActivateBonfireNodes;
				continue;
			}

			if (IsFireVFXSetActiveNode(CallNode))
			{
				NodesToRemove.Add(Node);
				++RemovedFireVFXSetActiveNodes;
			}
		}
	}

	for (UEdGraphNode* Node : NodesToRemove)
	{
		FBlueprintEditorUtils::RemoveNode(Blueprint, Node, /*bDontRecompile*/ true);
		bRemovedAny = true;
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UPackage* Package = Blueprint->GetOutermost();
	if (!UEditorLoadingAndSavingUtils::SavePackages({ Package }, false))
	{
		UE_LOG(LogTemp, Error, TEXT("[BonfireFix] Failed to save %s"), *Package->GetName());
		return 2;
	}

	UE_LOG(LogTemp, Display,
		TEXT("[BonfireFix] BP_BonFire saved. Changed=%s RemovedActivateBonfireNodes=%d RemovedFireVFXSetActiveNodes=%d"),
		bRemovedAny ? TEXT("true") : TEXT("false"),
		RemovedActivateBonfireNodes,
		RemovedFireVFXSetActiveNodes);
	return 0;
#else
	UE_LOG(LogTemp, Error, TEXT("[BonfireFix] This commandlet requires an editor build."));
	return 1;
#endif
}
