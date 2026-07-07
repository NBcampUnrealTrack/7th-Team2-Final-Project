#include "Animation/AnimNotifyState_AttachNiagaraToSocket.h"

#include "Animation/AnimNotifyQueue.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

const FName UAnimNotifyState_AttachNiagaraToSocket::PatternVFXComponentTag(TEXT("Retrieve.VFX.Pattern"));
const FName UAnimNotifyState_AttachNiagaraToSocket::CounterVFXComponentTag(TEXT("Retrieve.VFX.Counter"));

FName UAnimNotifyState_AttachNiagaraToSocket::MakeAnimationVFXComponentTag(const UAnimSequenceBase* Animation)
{
	return IsValid(Animation)
		? FName(*FString::Printf(TEXT("Retrieve.VFX.Animation.%u"), Animation->GetUniqueID()))
		: NAME_None;
}

FString UAnimNotifyState_AttachNiagaraToSocket::GetNotifyName_Implementation() const
{
	return NiagaraSystem
		? FString::Printf(TEXT("Attach Niagara: %s"), *GetNameSafe(NiagaraSystem))
		: TEXT("Attach Niagara To Socket");
}

void UAnimNotifyState_AttachNiagaraToSocket::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	FEnemyPatternVFXRow ResolvedConfig;
	if (!IsValid(MeshComp) || !ResolveVFXConfig(ResolvedConfig) || !IsValid(ResolvedConfig.NiagaraSystem))
	{
		return;
	}
	UNiagaraComponent* SpawnedComponent = nullptr;
	if (ResolvedConfig.bAttachToSocket)
	{
		SpawnedComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		ResolvedConfig.NiagaraSystem,
		MeshComp,
		ResolvedConfig.SocketName,
		ResolvedConfig.LocationOffset,
		ResolvedConfig.RotationOffset,
		ResolvedConfig.AttachLocationType,
		false,
		ResolvedConfig.bAutoActivate);
	}
	else
	{
		const FTransform SocketTransform = MeshComp->GetSocketTransform(ResolvedConfig.SocketName);
		SpawnedComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			MeshComp,
			ResolvedConfig.NiagaraSystem,
			SocketTransform.TransformPosition(ResolvedConfig.LocationOffset),
			(SocketTransform.GetRotation() * ResolvedConfig.RotationOffset.Quaternion()).Rotator(),
			FVector::OneVector,
			/*bAutoDestroy=*/false,
			ResolvedConfig.bAutoActivate);
	}

	if (!IsValid(SpawnedComponent))
	{
		return;
	}

	SpawnedComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnedComponent->SetGenerateOverlapEvents(false);
	SpawnedComponent->SetCanEverAffectNavigation(false);
	AddCleanupTags(SpawnedComponent, Animation, ResolvedConfig);

	FRetrieveAttachedNiagaraRuntimeEntry RuntimeEntry;
	RuntimeEntry.Animation = Animation;
	RuntimeEntry.NiagaraComponent = SpawnedComponent;
	RuntimeEntry.NotifyReference = EventReference;
	RuntimeEntry.NotifyEvent = EventReference.GetNotify();
	RuntimeEntry.ResolvedConfig = ResolvedConfig;
	RuntimeEntry.TotalDuration = TotalDuration;
	UpdateNiagaraComponent(RuntimeEntry);

	SpawnedComponentsByMesh.FindOrAdd(MeshComp).Add(RuntimeEntry);
}

void UAnimNotifyState_AttachNiagaraToSocket::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	FRetrieveAttachedNiagaraRuntimeEntry* RuntimeEntry = FindRuntimeEntry(MeshComp, Animation, EventReference);
	if (!RuntimeEntry)
	{
		return;
	}

	RuntimeEntry->ElapsedTime += FrameDeltaTime;
	UpdateNiagaraComponent(*RuntimeEntry);
}

void UAnimNotifyState_AttachNiagaraToSocket::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (IsValid(MeshComp))
	{
		if (TArray<FRetrieveAttachedNiagaraRuntimeEntry>* RuntimeEntries = SpawnedComponentsByMesh.Find(MeshComp))
		{
			for (int32 Index = RuntimeEntries->Num() - 1; Index >= 0; --Index)
			{
				FRetrieveAttachedNiagaraRuntimeEntry& RuntimeEntry = (*RuntimeEntries)[Index];
				UNiagaraComponent* SpawnedComponent = RuntimeEntry.NiagaraComponent.Get();

				if (!IsValid(SpawnedComponent))
				{
					RuntimeEntries->RemoveAtSwap(Index);
					continue;
				}

				if (!DoesRuntimeEntryMatchNotify(RuntimeEntry, Animation, EventReference))
				{
					continue;
				}

				CleanupNiagaraComponent(SpawnedComponent, RuntimeEntry.ResolvedConfig);
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

bool UAnimNotifyState_AttachNiagaraToSocket::ResolveVFXConfig(FEnemyPatternVFXRow& OutConfig) const
{
	if (bUseVFXDataTable)
	{
		if (VFXDataTable && !VFXRowName.IsNone())
		{
			if (const FEnemyPatternVFXRow* Row = VFXDataTable->FindRow<FEnemyPatternVFXRow>(VFXRowName, TEXT("ANS_AttachNiagaraToSocket")))
			{
				OutConfig = *Row;
				return true;
			}
		}

		return false;
	}

	OutConfig.NiagaraSystem = NiagaraSystem;
	OutConfig.SocketName = SocketName;
	OutConfig.bAttachToSocket = bAttachToSocket; 
	OutConfig.LocationOffset = LocationOffset;
	OutConfig.RotationOffset = RotationOffset;
	OutConfig.AttachLocationType = AttachLocationType;
	OutConfig.bAutoActivate = bAutoActivate;
	OutConfig.StartScale = Scale;
	OutConfig.PeakScale = Scale;
	OutConfig.EndScale = Scale;
	OutConfig.CleanupGroup = CleanupGroup;
	OutConfig.CustomCleanupTag = CustomCleanupTag;
	OutConfig.CleanupSlot = CleanupSlot;
	OutConfig.bDeactivateOnEnd = bDeactivateOnEnd;
	OutConfig.bDestroyOnEnd = bDestroyOnEnd;
	return IsValid(OutConfig.NiagaraSystem);
}

void UAnimNotifyState_AttachNiagaraToSocket::AddCleanupTags(
	UNiagaraComponent* NiagaraComponent,
	const UAnimSequenceBase* Animation,
	const FEnemyPatternVFXRow& Config) const
{
	if (!IsValid(NiagaraComponent))
	{
		return;
	}

	const FName GroupTag = GetCleanupGroupTag(Config);
	if (!GroupTag.IsNone())
	{
		NiagaraComponent->ComponentTags.AddUnique(GroupTag);

		const FName SlotTag = GetCleanupSlotTag(Config);
		if (!SlotTag.IsNone())
		{
			NiagaraComponent->ComponentTags.AddUnique(SlotTag);
		}

		const FName AnimationTag = MakeAnimationVFXComponentTag(Animation);
		if (!AnimationTag.IsNone())
		{
			NiagaraComponent->ComponentTags.AddUnique(AnimationTag);
		}
	}
}

void UAnimNotifyState_AttachNiagaraToSocket::CleanupNiagaraComponent(UNiagaraComponent* NiagaraComponent, const FEnemyPatternVFXRow& Config) const
{
	if (!IsValid(NiagaraComponent))
	{
		return;
	}

	if (Config.bDeactivateOnEnd)
	{
		if (Config.bDestroyOnEnd)
		{
			// 남아있는 파티클이 자연스럽게 소멸할 때까지 기다렸다가 엔진이 알아서 정리하게 한다.
			// (Deactivate 직후 바로 DestroyComponent를 부르면 Deactivate가 무의미해짐)
			NiagaraComponent->SetAutoDestroy(true);
		}
		NiagaraComponent->Deactivate();
	}
	else if (Config.bDestroyOnEnd)
	{
		NiagaraComponent->DestroyComponent();
	}
}

void UAnimNotifyState_AttachNiagaraToSocket::UpdateNiagaraComponent(FRetrieveAttachedNiagaraRuntimeEntry& RuntimeEntry) const
{
	UNiagaraComponent* NiagaraComponent = RuntimeEntry.NiagaraComponent.Get();
	if (!IsValid(NiagaraComponent))
	{
		return;
	}

	const FEnemyPatternVFXRow& Config = RuntimeEntry.ResolvedConfig;
	const float TotalDuration = FMath::Max(RuntimeEntry.TotalDuration, KINDA_SMALL_NUMBER);
	const float ElapsedTime = FMath::Clamp(RuntimeEntry.ElapsedTime, 0.f, TotalDuration);
	const float FadeOutStartTime = FMath::Max(Config.FadeInTime, TotalDuration - Config.FadeOutTime);

	float PhaseAlpha = 1.f;
	if (Config.FadeInTime > KINDA_SMALL_NUMBER && ElapsedTime < Config.FadeInTime)
	{
		PhaseAlpha = FMath::Clamp(ElapsedTime / Config.FadeInTime, 0.f, 1.f);
	}
	else if (Config.FadeOutTime > KINDA_SMALL_NUMBER && ElapsedTime >= FadeOutStartTime)
	{
		PhaseAlpha = 1.f - FMath::Clamp((ElapsedTime - FadeOutStartTime) / Config.FadeOutTime, 0.f, 1.f);
	}

	if (Config.bUpdateScaleOverTime)
	{
		FVector TargetScale = Config.PeakScale;
		if (Config.FadeInTime > KINDA_SMALL_NUMBER && ElapsedTime < Config.FadeInTime)
		{
			TargetScale = FMath::Lerp(Config.StartScale, Config.PeakScale, PhaseAlpha);
		}
		else if (Config.FadeOutTime > KINDA_SMALL_NUMBER && ElapsedTime >= FadeOutStartTime)
		{
			TargetScale = FMath::Lerp(Config.EndScale, Config.PeakScale, PhaseAlpha);
		}

		NiagaraComponent->SetRelativeScale3D(TargetScale);
	}
	else
	{
		NiagaraComponent->SetRelativeScale3D(Config.StartScale);
	}

	if (Config.bUpdateAlphaOverTime && !Config.AlphaParameterName.IsNone())
	{
		float TargetAlpha = Config.PeakAlpha;
		if (Config.FadeInTime > KINDA_SMALL_NUMBER && ElapsedTime < Config.FadeInTime)
		{
			TargetAlpha = FMath::Lerp(Config.StartAlpha, Config.PeakAlpha, PhaseAlpha);
		}
		else if (Config.FadeOutTime > KINDA_SMALL_NUMBER && ElapsedTime >= FadeOutStartTime)
		{
			TargetAlpha = FMath::Lerp(Config.EndAlpha, Config.PeakAlpha, PhaseAlpha);
		}

		NiagaraComponent->SetFloatParameter(Config.AlphaParameterName, TargetAlpha);
	}
}

FName UAnimNotifyState_AttachNiagaraToSocket::GetCleanupGroupTag(const FEnemyPatternVFXRow& Config) const
{
	switch (Config.CleanupGroup)
	{
	case ERetrieveAttachedNiagaraCleanupGroup::Pattern:
		return PatternVFXComponentTag;

	case ERetrieveAttachedNiagaraCleanupGroup::Counter:
		return CounterVFXComponentTag;

	case ERetrieveAttachedNiagaraCleanupGroup::Custom:
		return Config.CustomCleanupTag;

	case ERetrieveAttachedNiagaraCleanupGroup::None:
	default:
		return NAME_None;
	}
}

FName UAnimNotifyState_AttachNiagaraToSocket::GetCleanupSlotTag(const FEnemyPatternVFXRow& Config) const
{
	if (Config.CleanupSlot <= 0 || Config.CleanupSlot > 5)
	{
		return NAME_None;
	}

	switch (Config.CleanupGroup)
	{
	case ERetrieveAttachedNiagaraCleanupGroup::Pattern:
		return FName(*FString::Printf(TEXT("Retrieve.VFX.Pattern.%d"), Config.CleanupSlot));

	case ERetrieveAttachedNiagaraCleanupGroup::Counter:
		return FName(*FString::Printf(TEXT("Retrieve.VFX.Counter.%d"), Config.CleanupSlot));

	case ERetrieveAttachedNiagaraCleanupGroup::Custom:
	case ERetrieveAttachedNiagaraCleanupGroup::None:
	default:
		return NAME_None;
	}
}

FRetrieveAttachedNiagaraRuntimeEntry* UAnimNotifyState_AttachNiagaraToSocket::FindRuntimeEntry(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!IsValid(MeshComp))
	{
		return nullptr;
	}

	TArray<FRetrieveAttachedNiagaraRuntimeEntry>* RuntimeEntries = SpawnedComponentsByMesh.Find(MeshComp);
	if (!RuntimeEntries)
	{
		return nullptr;
	}

	for (FRetrieveAttachedNiagaraRuntimeEntry& RuntimeEntry : *RuntimeEntries)
	{
		if (!RuntimeEntry.NiagaraComponent.IsValid())
		{
			continue;
		}

		if (DoesRuntimeEntryMatchNotify(RuntimeEntry, Animation, EventReference))
		{
			return &RuntimeEntry;
		}
	}

	return nullptr;
}

bool UAnimNotifyState_AttachNiagaraToSocket::DoesRuntimeEntryMatchNotify(
	const FRetrieveAttachedNiagaraRuntimeEntry& RuntimeEntry,
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
