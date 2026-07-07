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
	bool bIsLooping = false;
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
	void HandleLoopingAudioFinished(UAudioComponent* FinishedComponent);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Data")
	bool bUseSFXDataTable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Data", meta=(EditCondition="bUseSFXDataTable", EditConditionHides))
	TObjectPtr<UDataTable> SFXDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Data", meta=(EditCondition="bUseSFXDataTable", EditConditionHides))
	FName SFXRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Direct", meta=(EditCondition="!bUseSFXDataTable", EditConditionHides))
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Direct", meta=(EditCondition="!bUseSFXDataTable", EditConditionHides, ClampMin="0.0", ClampMax="20.0"))
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

	/** true면 이 배치에서는 NotifyEnd가 관리하지 않고 재생 길이만큼 자연 재생 후 종료한다.
	 * 사운드 자체의 속성이 아니라 이 Notify 배치가 End 시점에 개입할지를 정하는 옵션이라
	 * SFXDataTable 사용 여부와 무관하게 항상 노출한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SFX|Playback")
	bool bIgnoreEnd = false;

private:
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FRetrieveMonsterSFXRuntimeEntry>> SpawnedComponentsByMesh;
};
