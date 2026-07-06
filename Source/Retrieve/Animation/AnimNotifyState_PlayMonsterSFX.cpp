#include "Animation/AnimNotifyState_PlayMonsterSFX.h"

#include "Animation/AnimNotifyQueue.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

FString UAnimNotifyState_PlayMonsterSFX::GetNotifyName_Implementation() const
{
	return Sound
		? FString::Printf(TEXT("Play SFX: %s"), *GetNameSafe(Sound))
		: TEXT("Play Monster SFX");
}

void UAnimNotifyState_PlayMonsterSFX::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	FMonsterSFXRow ResolvedConfig;
	if (!IsValid(MeshComp) || !ResolveSFXConfig(ResolvedConfig) || !IsValid(ResolvedConfig.Sound))
	{
		return;
	}

	// bFollow=false는 스폰 시점 위치에 고정하는 용도이므로, KeepWorldPosition에 넘길
	// 월드 좌표를 현재 소켓/메시 위치로 계산한다. (기존엔 항상 FVector::ZeroVector라 월드 원점 재생됨)
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	if (!ResolvedConfig.bFollow)
	{
		const bool bHasSocket = !ResolvedConfig.SocketName.IsNone() && MeshComp->DoesSocketExist(ResolvedConfig.SocketName);
		SpawnLocation = bHasSocket ? MeshComp->GetSocketLocation(ResolvedConfig.SocketName) : MeshComp->GetComponentLocation();
		SpawnRotation = bHasSocket ? MeshComp->GetSocketRotation(ResolvedConfig.SocketName) : MeshComp->GetComponentRotation();
	}

	UAudioComponent* SpawnedComponent = UGameplayStatics::SpawnSoundAttached(
		ResolvedConfig.Sound,
		MeshComp,
		ResolvedConfig.SocketName,
		SpawnLocation,
		SpawnRotation,
		ResolvedConfig.bFollow ? EAttachLocation::KeepRelativeOffset : EAttachLocation::KeepWorldPosition,
		false,
		ResolvedConfig.VolumeMultiplier,
		ResolvedConfig.PitchMultiplier);

	if (!IsValid(SpawnedComponent))
	{
		return;
	}

	if (ResolvedConfig.FadeInTime > 0.f)
	{
		// FadeVolumeLevel은 VolumeMultiplier에 대한 비율이므로 1.0(100%)을 목표로 페이드인한다.
		SpawnedComponent->FadeIn(ResolvedConfig.FadeInTime, 1.0f);
	}

	if (!ResolvedConfig.bLoop && ResolvedConfig.FadeOutTime <= 0.f)
	{
		// 자연 종료에 맡김 — NotifyEnd에서 추적/정리할 필요 없음.
		return;
	}

	FRetrieveMonsterSFXRuntimeEntry RuntimeEntry;
	RuntimeEntry.Animation = Animation;
	RuntimeEntry.AudioComponent = SpawnedComponent;
	RuntimeEntry.NotifyReference = EventReference;
	RuntimeEntry.NotifyEvent = EventReference.GetNotify();
	RuntimeEntry.FadeOutTime = ResolvedConfig.FadeOutTime;
	SpawnedComponentsByMesh.FindOrAdd(MeshComp).Add(RuntimeEntry);
}

void UAnimNotifyState_PlayMonsterSFX::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (IsValid(MeshComp))
	{
		if (TArray<FRetrieveMonsterSFXRuntimeEntry>* RuntimeEntries = SpawnedComponentsByMesh.Find(MeshComp))
		{
			for (int32 Index = RuntimeEntries->Num() - 1; Index >= 0; --Index)
			{
				FRetrieveMonsterSFXRuntimeEntry& RuntimeEntry = (*RuntimeEntries)[Index];
				UAudioComponent* SpawnedComponent = RuntimeEntry.AudioComponent.Get();

				if (!IsValid(SpawnedComponent))
				{
					RuntimeEntries->RemoveAtSwap(Index);
					continue;
				}

				if (!DoesRuntimeEntryMatchNotify(RuntimeEntry, Animation, EventReference))
				{
					continue;
				}

				if (RuntimeEntry.FadeOutTime > 0.f)
				{
					// FadeVolumeLevel은 페이드를 시작하는 지점의 비율이므로 1.0(현재 볼륨)에서 무음으로 페이드아웃한다.
					SpawnedComponent->FadeOut(RuntimeEntry.FadeOutTime, 1.0f);
				}
				else
				{
					SpawnedComponent->Stop();
				}

				RuntimeEntries->RemoveAtSwap(Index);
				break;
			}

			if (RuntimeEntries->IsEmpty())
			{
				SpawnedComponentsByMesh.Remove(MeshComp);
			}
		}
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

bool UAnimNotifyState_PlayMonsterSFX::ResolveSFXConfig(FMonsterSFXRow& OutConfig) const
{
	if (bUseSFXDataTable)
	{
		if (SFXDataTable && !SFXRowName.IsNone())
		{
			if (const FMonsterSFXRow* Row = SFXDataTable->FindRow<FMonsterSFXRow>(SFXRowName, TEXT("ANS_PlayMonsterSFX")))
			{
				OutConfig = *Row;
				return true;
			}
		}

		return false;
	}

	OutConfig.Sound = Sound;
	OutConfig.VolumeMultiplier = VolumeMultiplier;
	OutConfig.PitchMultiplier = PitchMultiplier;
	OutConfig.SocketName = SocketName;
	OutConfig.bFollow = bFollow;
	OutConfig.bLoop = bLoop;
	OutConfig.FadeInTime = FadeInTime;
	OutConfig.FadeOutTime = FadeOutTime;
	return IsValid(OutConfig.Sound);
}

bool UAnimNotifyState_PlayMonsterSFX::DoesRuntimeEntryMatchNotify(
	const FRetrieveMonsterSFXRuntimeEntry& RuntimeEntry,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference) const
{
	if (RuntimeEntry.Animation.Get() != Animation)
	{
		return false;
	}

	const FAnimNotifyEvent* NotifyEvent = EventReference.GetNotify();
	if (!NotifyEvent || !RuntimeEntry.NotifyEvent)
	{
		return true;
	}

	if (RuntimeEntry.NotifyReference == EventReference)
	{
		return true;
	}

	if (RuntimeEntry.NotifyEvent == NotifyEvent)
	{
		return true;
	}

	return false;
}
