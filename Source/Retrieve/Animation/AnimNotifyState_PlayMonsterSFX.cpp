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

	const float PlayChance = FMath::Clamp(ResolvedConfig.PlayChance, 0.f, 1.f);
	if (PlayChance <= 0.f || (PlayChance < 1.f && FMath::FRand() >= PlayChance))
	{
		return;
	}

	const float PitchRandomMultiplierMin = FMath::Min(
		ResolvedConfig.PitchRandomMultiplierMin,
		ResolvedConfig.PitchRandomMultiplierMax);
	const float PitchRandomMultiplierMax = FMath::Max(
		ResolvedConfig.PitchRandomMultiplierMin,
		ResolvedConfig.PitchRandomMultiplierMax);
	const float PitchRandomMultiplier = FMath::IsNearlyEqual(PitchRandomMultiplierMin, PitchRandomMultiplierMax)
		? PitchRandomMultiplierMin
		: FMath::FRandRange(PitchRandomMultiplierMin, PitchRandomMultiplierMax);
	const float RandomizedPitchMultiplier = ResolvedConfig.PitchMultiplier * PitchRandomMultiplier;

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

	// bIgnoreEnd가 켜져 있으면 NotifyEnd가 관리하지 않으므로, 반복 재생도 걸 수 없다
	// (걸어봤자 멈춰줄 주체가 없어 컴포넌트가 새게 된다).
	const bool bManagedLoop = ResolvedConfig.bLoop && !bIgnoreEnd;

	UAudioComponent* SpawnedComponent = UGameplayStatics::SpawnSoundAttached(
		ResolvedConfig.Sound,
		MeshComp,
		ResolvedConfig.SocketName,
		SpawnLocation,
		SpawnRotation,
		ResolvedConfig.bFollow ? EAttachLocation::KeepRelativeOffset : EAttachLocation::KeepWorldPosition,
		false,
		ResolvedConfig.VolumeMultiplier,
		RandomizedPitchMultiplier,
		0.f,
		nullptr,
		nullptr,
		// 반복 재생 사이에 스스로 파괴되면 안 되므로 관리 대상 루프만 bAutoDestroy를 끈다.
		// NotifyEnd에서 실제로 멈추기로 결정한 시점에 다시 켠다.
		!bManagedLoop);

	if (!IsValid(SpawnedComponent))
	{
		return;
	}

	if (ResolvedConfig.FadeInTime > 0.f)
	{
		// FadeVolumeLevel은 VolumeMultiplier에 대한 비율이므로 1.0(100%)을 목표로 페이드인한다.
		SpawnedComponent->FadeIn(ResolvedConfig.FadeInTime, 1.0f);
	}

	if (bIgnoreEnd)
	{
		// NotifyEnd에서 관리하지 않는다 — 재생 길이만큼 자연 재생 후 스스로 정리된다.
		return;
	}

	if (bManagedLoop)
	{
		// bLoop는 사운드 에셋 자체의 루프 설정과 무관하게 코드가 직접 반복 재생시킨다.
		SpawnedComponent->OnAudioFinishedNative.AddUObject(this, &UAnimNotifyState_PlayMonsterSFX::HandleLoopingAudioFinished);
	}

	FRetrieveMonsterSFXRuntimeEntry RuntimeEntry;
	RuntimeEntry.Animation = Animation;
	RuntimeEntry.AudioComponent = SpawnedComponent;
	RuntimeEntry.NotifyReference = EventReference;
	RuntimeEntry.NotifyEvent = EventReference.GetNotify();
	RuntimeEntry.FadeOutTime = ResolvedConfig.FadeOutTime;
	RuntimeEntry.bIsLooping = bManagedLoop;
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

				if (RuntimeEntry.bIsLooping)
				{
					// 반복 재생 감지 핸들러를 먼저 해제하고, 정지 후 정상적으로 파괴되도록 되돌린다.
					SpawnedComponent->OnAudioFinishedNative.RemoveAll(this);
					SpawnedComponent->bAutoDestroy = true;
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

void UAnimNotifyState_PlayMonsterSFX::HandleLoopingAudioFinished(UAudioComponent* FinishedComponent)
{
	// NotifyEnd가 명시적으로 정지시키기 전까지는 재생이 끝날 때마다 처음부터 다시 재생한다.
	if (IsValid(FinishedComponent))
	{
		FinishedComponent->Play();
	}
}
