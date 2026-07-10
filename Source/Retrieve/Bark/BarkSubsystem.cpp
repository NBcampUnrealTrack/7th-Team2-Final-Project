#include "BarkSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Audio/RetrieveMusicSubsystem.h"
#include "BarkSpeakerComponent.h"
#include "Core/RetrieveGameState.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Quest/QuestBranchComponent.h"
#include "TimerManager.h"

bool UBarkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(&InWorld);

	StepChangedHandle = MessageSubsystem.RegisterListener<FRetrieveQuestStepPayload>(
		RetrieveGameplayTags::Channel_Quest_StepChanged, this, &UBarkSubsystem::HandleStepChanged);

	CinematicHandle = MessageSubsystem.RegisterListener<FRetrieveCinematicStatePayload>(
		RetrieveGameplayTags::Channel_Cinematic_Changed, this, &UBarkSubsystem::HandleCinematicChanged);

	DialogueHandle = MessageSubsystem.RegisterListener<FRetrieveDialogueChangedPayload>(
		RetrieveGameplayTags::Channel_UI_DialogueChanged, this, &UBarkSubsystem::HandleDialogueChanged);

	InWorld.GetTimerManager().SetTimer(ScanTimerHandle, this, &UBarkSubsystem::ScanAmbient, ScanInterval,
	                                   true);
}

void UBarkSubsystem::Deinitialize()
{
	StepChangedHandle.Unregister();
	CinematicHandle.Unregister();
	DialogueHandle.Unregister();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimerHandle);
		for (FTimerHandle& Handle : StepBarkTimers)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	StepBarkTimers.Reset();
	Speakers.Reset();
	SpeakerReadyTime.Reset();
}

void UBarkSubsystem::RegisterSpeaker(UBarkSpeakerComponent* Speaker)
{
	if (Speaker)
	{
		Speakers.AddUnique(Speaker);
	}
}

void UBarkSubsystem::UnregisterSpeaker(UBarkSpeakerComponent* Speaker)
{
	Speakers.RemoveAll([Speaker](const TWeakObjectPtr<UBarkSpeakerComponent>& WeakSpeaker)
	{
		return !WeakSpeaker.IsValid() || WeakSpeaker.Get() == Speaker;
	});
	SpeakerReadyTime.Remove(Speaker);
}

// ---- Listeners ------------------------------------------------------------

void UBarkSubsystem::HandleStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message)
{
	// 각 클라이언트가 Channel.Quest.StepChanged를 로컬로 듣고 자신의 클라이언트에서 발동 (상태는 이미 복제됨)
	const UDataTable* Table = GetBarkTable();
	UWorld* World = GetWorld();
	if (!Table || !World)
	{
		return;
	}

	static const FString ContextString(TEXT("BarkOnQuestStep"));
	const TArray<FName> RowNames = Table->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FBarkRow* Row = Table->FindRow<FBarkRow>(RowName, ContextString, false);
		if (Row && Row->Trigger == EBarkTrigger::OnQuestStep && Row->KeyTag.IsValid() && Row->KeyTag == Message.StepTag
			&& IsRowEligible(RowName, *Row))
		{
			// 스텝당 한 행. 연속 대사는 그 행의 bSequentialLines로 처리
			// TODO(coop): Lumen 동기화 대사는 추후 호스트가 모든 클라이언트에 맞춰 재생. 현재는 클라이언트 로컬.
			const float Delay = Row->TriggerDelaySeconds;
			if (Delay <= 0.f)
			{
				FireStepBark(RowName);
			}
			else
			{
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(
					Handle, FTimerDelegate::CreateWeakLambda(this, [this, RowName]() { FireStepBark(RowName); }), Delay,
					false);
				StepBarkTimers.Add(Handle);
			}
			break;
		}
	}
}

void UBarkSubsystem::HandleCinematicChanged(FGameplayTag Channel, const FRetrieveCinematicStatePayload& Message)
{
	bCinematicActive = Message.bActive;
}

void UBarkSubsystem::HandleDialogueChanged(FGameplayTag Channel, const FRetrieveDialogueChangedPayload& Message)
{
	bDialogueActive = Message.bActive;
}

// ---- Ambient arbiter ------------------------------------------------------

void UBarkSubsystem::ScanAmbient()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (IsSuppressed())
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now < NextAmbientAllowedTime)
	{
		return;
	}

	if (IsLocalPlayerInCombat())
	{
		return;
	}

	const UDataTable* Table = GetBarkTable();
	if (!Table)
	{
		return;
	}

	static const FString ContextString(TEXT("BarkAmbientScan"));
	const TArray<FName> RowNames = Table->GetRowNames();

	// 사정거리 안 + 쿨다운 해제 + 자격 AmbientRandom 행이 1개 이상인 스피커 모으기.
	// TODO(coop): 클라이언트별 근접/스캔. NPC 앰비언트는 클라이언트별 로컬, Lumen은 호스트 동기화.
	TArray<UBarkSpeakerComponent*> Candidates;
	for (const TWeakObjectPtr<UBarkSpeakerComponent>& WeakSpeaker : Speakers)
	{
		UBarkSpeakerComponent* Speaker = WeakSpeaker.Get();
		if (!Speaker || !IsSpeakerInRange(Speaker))
		{
			continue;
		}
		if (const double* ReadyAt = SpeakerReadyTime.Find(Speaker))
		{
			if (Now < *ReadyAt)
			{
				continue;
			}
		}
		bool bHasEligible = false;
		for (const FName& RowName : RowNames)
		{
			const FBarkRow* Row = Table->FindRow<FBarkRow>(RowName, ContextString, false);
			if (Row && Row->Trigger == EBarkTrigger::AmbientRandom &&
				Row->SpeakerTag == Speaker->SpeakerTag && IsRowEligible(RowName, *Row))
			{
				bHasEligible = true;
				break;
			}
		}
		if (bHasEligible)
		{
			Candidates.Add(Speaker);
		}
	}

	if (Candidates.Num() == 0)
	{
		return;
	}

	// 한 번에 하나, 후보 중 하나 선택
	UBarkSpeakerComponent* Chosen = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

	// 해당 스피커의 자격 AmbientRandom 행들 중 하나 선택 (직전 행 회피)
	TArray<FName> EligibleRows;
	for (const FName& RowName : RowNames)
	{
		const FBarkRow* Row = Table->FindRow<FBarkRow>(RowName, ContextString, false);
		if (Row && Row->Trigger == EBarkTrigger::AmbientRandom && Row->SpeakerTag == Chosen->SpeakerTag && IsRowEligible(RowName, *Row))
		{
			EligibleRows.Add(RowName);
		}
	}
	if (EligibleRows.Num() == 0)
	{
		return;
	}
	if (EligibleRows.Num() > 1)
	{
		EligibleRows.Remove(LastFiredRow); // 즉시 반복 회피
	}

	const FName PickRow = EligibleRows[FMath::RandRange(0, EligibleRows.Num() - 1)];
	if (const FBarkRow* Picked = Table->FindRow<FBarkRow>(PickRow, ContextString, false))
	{
		FireRow(PickRow, *Picked, Chosen);
		SpeakerReadyTime.Add(Chosen, Now + Chosen->MinInterval);
		NextAmbientAllowedTime = Now + AmbientCooldown;
	}
}

// ---- Eligibility / proximity / combat ------------------------------------


bool UBarkSubsystem::IsRowEligible(FName RowName, const FBarkRow& Row) const
{
	if (Row.bPlayOnce && PlayedOnceRows.Contains(RowName))
	{
		return false;
	}
	for (const FGameplayTag& RequiredStep : Row.RequiredSteps) // 모두 완료되어야
	{
		if (!IsStepCompleted(RequiredStep))
		{
			return false;
		}
	}
	for (const FGameplayTag& ForbiddenStep : Row.ForbiddenSteps) // 하나라도 완료면 탈락
	{
		if (IsStepCompleted(ForbiddenStep))
		{
			return false;
		}
	}
	return true;
}

bool UBarkSubsystem::IsSpeakerInRange(const UBarkSpeakerComponent* Speaker) const
{
	if (!Speaker)
	{
		return false;
	}
	if (Speaker->Range <= 0.f) // 0/음수 = 항상 사정거리 안에 있음 (Lumen)
	{
		return true;
	}
	const AActor* Owner = Speaker->GetOwner();
	UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!Owner || !GameInstance)
	{
		return false;
	}
	const float RangeSq = Speaker->Range * Speaker->Range;
	for (const ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers()) // 현재 1명 // TODO(coop): 클라이언트별
	{
		const APlayerController* PC = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (Pawn && FVector::DistSquared(Pawn->GetActorLocation(), Owner->GetActorLocation()) <= RangeSq)
		{
			return true;
		}
	}
	return false;
}

bool UBarkSubsystem::IsLocalPlayerInCombat() const
{
	UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		return false;
	}

	if (const URetrieveMusicSubsystem* Music = World->GetSubsystem<URetrieveMusicSubsystem>())
	{
		if (Music->IsCombatActive())
		{
			return true;
		}
	}

	for (const ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
	{
		const APlayerController* PC = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::Get().GetAbilitySystemComponentFromActor(Pawn))
		{
			if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Combat))
			{
				return true;
			}
		}
	}
	return false;
}

// ---- Fire / triggers ------------------------------------------------------

void UBarkSubsystem::FireRow(FName RowName, const FBarkRow& Row, UBarkSpeakerComponent* ViaSpeaker)
{
	const FRetrieveBarkPayload Message = BuildPayload(Row);

	if (ViaSpeaker)
	{
		ViaSpeaker->RouteBark(Message); // AmbientRandom
	}
	else
	{
		BroadcastBarkLocal(Message); // OnQuestStep / Manual
	}

	if (Row.bPlayOnce)
	{
		PlayedOnceRows.Add(RowName);
	}
	LastFiredRow = RowName;
}

FRetrieveBarkPayload UBarkSubsystem::BuildPayload(const FBarkRow& Row)
{
	FRetrieveBarkPayload Message;
	Message.SpeakerTag = Row.SpeakerTag;
	Message.SpeakerName = Row.SpeakerName;
	Message.Duration = Row.Duration;
	Message.Cue = Row.Cue;

	if (Row.Lines.Num() > 0)
	{
		TArray<int32> Candidates;
		for (int32 i = 0; i < Row.Lines.Num(); ++i)
		{
			if (!Row.Lines[i].ToString().Equals(LastLine))
			{
				Candidates.Add(i);
			}
		}
		const int32 Pick = (Candidates.Num() > 0)
			                   ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)]
			                   : FMath::RandRange(0, Row.Lines.Num() - 1);
		Message.Line = Row.Lines[Pick];
		LastLine = Message.Line.ToString();
	}
	return Message;
}

FRetrieveBarkPayload UBarkSubsystem::BuildPayloadForLine(const FBarkRow& Row, const FText& Line) const
{
	FRetrieveBarkPayload Message;
	Message.SpeakerTag = Row.SpeakerTag;
	Message.SpeakerName = Row.SpeakerName;
	Message.Duration = Row.Duration;
	Message.Cue = Row.Cue;
	Message.Line = Line;
	return Message;
}

void UBarkSubsystem::FireStepBark(FName RowName)
{
	const UDataTable* Table = GetBarkTable();
	if (!Table)
	{
		return;
	}
	static const FString ContextString(TEXT("BarkStepFire"));
	const FBarkRow* Row = Table->FindRow<FBarkRow>(RowName, ContextString, false);
	if (!Row)
	{
		return;
	}
	if (!IsRowEligible(RowName, *Row))
	{
		return;
	}

	if (Row->bSequentialLines && Row->Lines.Num() > 0)
	{
		// 여러 줄을 한 번에 순서대로 큐에 적재
		for (int32 i = 0; i < Row->Lines.Num(); ++i)
		{
			FRetrieveBarkPayload Message = BuildPayloadForLine(*Row, Row->Lines[i]);
			if (i != 0)
			{
				Message.Cue = nullptr; // 오디오 큐는 첫 줄에서만 재생 (행에 큐가 하나뿐임...)
			}
			BroadcastBarkLocal(Message);
		}
		if (Row->bPlayOnce)
		{
			PlayedOnceRows.Add(RowName);
		}
		LastFiredRow = RowName;
		LastLine = Row->Lines.Last().ToString();
	}
	else
	{
		// 단일 줄: 기존 경로 그대로 (무작위 한 줄 + bPlayOnce/LastFired 처리)
		FireRow(RowName, *Row, nullptr);
	}
}

void UBarkSubsystem::BroadcastBarkLocal(const FRetrieveBarkPayload& Payload) const
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_UI_BarkRequested, Payload);
	}
}

void UBarkSubsystem::RequestBarkById(FName RowName)
{
	const UDataTable* Table = GetBarkTable();
	if (!Table || RowName.IsNone())
	{
		return;
	}
	static const FString ContextString(TEXT("BarkRequestById"));
	if (const FBarkRow* Row = Table->FindRow<FBarkRow>(RowName, ContextString, true))
	{
		if (IsRowEligible(RowName, *Row))
		{
			FireRow(RowName, *Row, nullptr);
		}
	}
}

void UBarkSubsystem::RequestBarkByKey(FGameplayTag KeyTag)
{
	const UDataTable* Table = GetBarkTable();
	if (!Table || !KeyTag.IsValid())
	{
		return;
	}
	static const FString ContextString(TEXT("BarkRequestByKey"));
	const TArray<FName> RowNames = Table->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		const FBarkRow* Row = Table->FindRow<FBarkRow>(RowName, ContextString, false);
		if (Row && Row->Trigger == EBarkTrigger::Manual && Row->KeyTag == KeyTag && IsRowEligible(RowName, *Row))
		{
			FireRow(RowName, *Row, nullptr);
			return; // 첫 자격 행 하나
		}
	}
}

// ---- GameState helpers ----------------------------------------------------

const UDataTable* UBarkSubsystem::GetBarkTable() const
{
	const UWorld* World = GetWorld();
	const ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	return GS ? GS->GetBarkTable() : nullptr;
}

bool UBarkSubsystem::IsStepCompleted(FGameplayTag StepTag) const
{
	const UWorld* World = GetWorld();
	const ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	const UQuestBranchComponent* Quest = GS ? GS->GetQuestBranchComponent() : nullptr;
	return Quest ? Quest->IsStepCompleted(StepTag) : false;
}
