#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Data/RetrieveDataTableTypes.h"
#include "AnimNotifyState_PlayMonsterSFX.generated.h"

class UAudioComponent;
class USoundBase;
class UDataTable;
struct FAnimNotifyEvent;

struct FRetrieveMonsterSFXRuntimeEntry
{
	TWeakObjectPtr<UAnimSequenceBase> Animation;
	TWeakObjectPtr<UAudioComponent> AudioComponent;
	FAnimNotifyEventReference NotifyReference;
	const FAnimNotifyEvent* NotifyEvent = nullptr;
	float FadeOutTime = 0.f;
};

UCLASS(DisplayName="Play Monster SFX")
class RETRIEVE_API UAnimNotifyState_PlayMonsterSFX : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

private:
	bool ResolveSFXConfig(FMonsterSFXRow& OutConfig) const;
	bool DoesRuntimeEntryMatchNotify(
		const FRetrieveMonsterSFXRuntimeEntry& RuntimeEntry,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Data")
	bool bUseSFXDataTable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Data", meta=(EditCondition="bUseSFXDataTable", EditConditionHides))
	TObjectPtr<UDataTable> SFXDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Data", meta=(EditCondition="bUseSFXDataTable", EditConditionHides))
	FName SFXRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Direct", meta=(EditCondition="!bUseSFXDataTable", EditConditionHides))
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Direct", meta=(EditCondition="!bUseSFXDataTable", EditConditionHides, ClampMin="0.0", ClampMax="2.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Direct", meta=(EditCondition="!bUseSFXDataTable", EditConditionHides, ClampMin="0.5", ClampMax="2.0"))
	float PitchMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Direct", meta=(EditCondition="!bUseSFXDataTable", EditConditionHides))
	FName SocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Direct", meta=(EditCondition="!bUseSFXDataTable", EditConditionHides))
	bool bFollow = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Direct", meta=(EditCondition="!bUseSFXDataTable", EditConditionHides))
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Direct", meta=(EditCondition="!bUseSFXDataTable", EditConditionHides, ClampMin="0.0"))
	float FadeInTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Direct", meta=(EditCondition="!bUseSFXDataTable", EditConditionHides, ClampMin="0.0"))
	float FadeOutTime = 0.f;

private:
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FRetrieveMonsterSFXRuntimeEntry>> SpawnedComponentsByMesh;
};
