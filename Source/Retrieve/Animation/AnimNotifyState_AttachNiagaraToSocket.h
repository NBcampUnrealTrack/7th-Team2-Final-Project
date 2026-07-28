#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Data/RetrieveDataTableTypes.h"
#include "AnimNotifyState_AttachNiagaraToSocket.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UDataTable;
struct FAnimNotifyEvent;

struct FRetrieveAttachedNiagaraRuntimeEntry
{
	TWeakObjectPtr<UAnimSequenceBase> Animation;
	TWeakObjectPtr<UNiagaraComponent> NiagaraComponent;
	FAnimNotifyEventReference NotifyReference;
	const FAnimNotifyEvent* NotifyEvent = nullptr;
	FEnemyPatternVFXRow ResolvedConfig;
	float ElapsedTime = 0.f;
	float TotalDuration = 0.f;
};

UCLASS(DisplayName="Attach Niagara To Socket")
class RETRIEVE_API UAnimNotifyState_AttachNiagaraToSocket : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	static const FName PatternVFXComponentTag;
	static const FName CounterVFXComponentTag;
	static FName MakeAnimationVFXComponentTag(const UAnimSequenceBase* Animation);

	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

private:
	bool ResolveVFXConfig(FEnemyPatternVFXRow& OutConfig) const;
	void AddCleanupTags(UNiagaraComponent* NiagaraComponent, const UAnimSequenceBase* Animation, const FEnemyPatternVFXRow& Config) const;
	void CleanupNiagaraComponent(UNiagaraComponent* NiagaraComponent, const FEnemyPatternVFXRow& Config) const;
	void UpdateNiagaraComponent(FRetrieveAttachedNiagaraRuntimeEntry& RuntimeEntry) const;
	FName GetCleanupGroupTag(const FEnemyPatternVFXRow& Config) const;
	FName GetCleanupSlotTag(const FEnemyPatternVFXRow& Config) const;
	FRetrieveAttachedNiagaraRuntimeEntry* FindRuntimeEntry(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference);
	bool DoesRuntimeEntryMatchNotify(
		const FRetrieveAttachedNiagaraRuntimeEntry& RuntimeEntry,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) const;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Data")
	bool bUseVFXDataTable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Data", meta=(EditCondition="bUseVFXDataTable", EditConditionHides))
	TObjectPtr<UDataTable> VFXDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Data", meta=(EditCondition="bUseVFXDataTable", EditConditionHides))
	FName VFXRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	FName SocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	bool bAttachToSocket = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	TEnumAsByte<EAttachLocation::Type> AttachLocationType = EAttachLocation::KeepRelativeOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	bool bAutoActivate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct|Cleanup", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	ERetrieveAttachedNiagaraCleanupGroup CleanupGroup = ERetrieveAttachedNiagaraCleanupGroup::Pattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct|Cleanup", meta=(EditCondition="!bUseVFXDataTable && CleanupGroup == ERetrieveAttachedNiagaraCleanupGroup::Custom", EditConditionHides))
	FName CustomCleanupTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct|Cleanup", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides, ClampMin="0", ClampMax="5", UIMin="0", UIMax="5"))
	int32 CleanupSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct|End", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	bool bDeactivateOnEnd = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara|Direct|End", meta=(EditCondition="!bUseVFXDataTable", EditConditionHides))
	bool bDestroyOnEnd = true;

private:
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FRetrieveAttachedNiagaraRuntimeEntry>> SpawnedComponentsByMesh;
};
