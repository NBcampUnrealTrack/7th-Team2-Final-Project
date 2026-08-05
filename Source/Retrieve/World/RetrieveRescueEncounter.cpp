#include "World/RetrieveRescueEncounter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/World/RetrieveDialogueComponent.h"
#include "Enemy/SpawnerBase.h"
#include "EngineUtils.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Save/RetrieveSaveGame.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "Subsystems/RetrieveObjectiveMarkerSubsystem.h"
#include "World/RetrieveBonfireActor.h"

ARetrieveRescueEncounter::ARetrieveRescueEncounter()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ARetrieveRescueEncounter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	if (ACharacter* CaptiveCharacter = Cast<ACharacter>(RescueNPC))
	{
		if (USkeletalMeshComponent* Mesh = CaptiveCharacter->GetMesh())
		{
			if (UAnimSequence* PreviewLoop = LoadObject<UAnimSequence>(
				nullptr, TEXT("/Game/Retrieve/Character/Animations/Rescue/A_MOD_IDL_Pray_Kneeling_Loop_Masc.A_MOD_IDL_Pray_Kneeling_Loop_Masc")))
			{
				Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				Mesh->SetAnimation(PreviewLoop);
				Mesh->Play(true);
			}
		}
	}
#endif
}

void ARetrieveRescueEncounter::BeginPlay()
{
	Super::BeginPlay();

	if (!SpawnGroupId.IsValid() && EnemySpawner)
	{
		SpawnGroupId = EnemySpawner->SpawnGroupId;
	}

	if (RescueNPC)
	{
		if (URetrieveDialogueComponent* Dialogue = RescueNPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			Dialogue->OnDialogueClosed.AddUniqueDynamic(this, &ThisClass::HandleRescueDialogueClosed);
		}
	}

	ClearedHandle = UGameplayMessageSubsystem::Get(this).RegisterListener<FSpawnGroupClearedPayload>(
		RetrieveGameplayTags::Channel_Enemy_SpawnGroupCleared,
		this, &ThisClass::HandleSpawnGroupCleared);

	if (UGameInstance* GI = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSubsystem = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSubsystem->OnLoadCompleted.AddUniqueDynamic(this, &ThisClass::HandleSaveLoaded);
		}
	}

	RestoreSavedState();
	ApplyState();
}

void ARetrieveRescueEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	URetrieveObjectiveMarkerSubsystem::RemoveMarkersByPrefix(
		GetWorld(), (EncounterId.IsNone() ? GetName() : EncounterId.ToString()) + TEXT("_"));

	if (UGameInstance* GI = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSubsystem = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSubsystem->OnLoadCompleted.RemoveDynamic(this, &ThisClass::HandleSaveLoaded);
		}
	}

	if (RescueNPC)
	{
		if (URetrieveDialogueComponent* Dialogue = RescueNPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			Dialogue->OnDialogueClosed.RemoveDynamic(this, &ThisClass::HandleRescueDialogueClosed);
		}
	}

	if (ClearedHandle.IsValid())
	{
		UGameplayMessageSubsystem::Get(this).UnregisterListener(ClearedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ARetrieveRescueEncounter::HandleSpawnGroupCleared(
	FGameplayTag Channel, const FSpawnGroupClearedPayload& Payload)
{
	if (!HasAuthority() || State != ERetrieveRescueEncounterState::EnemiesAlive
		|| !SpawnGroupId.IsValid() || Payload.SpawnGroupId != SpawnGroupId)
	{
		return;
	}

	// 같은 그룹 태그를 여러 스포너가 공유할 수 있다(예: 튜토리얼 늑대와 포로 구출의 적이
	// 둘 다 TutorialWolves). 태그만 보면 남의 스포너가 비워졌을 때도 구출이 완료돼 버린다.
	// 내 스포너를 지정해 뒀다면 그 스포너에서 온 신호인지까지 확인한다.
	if (EnemySpawner && Payload.Spawner != EnemySpawner)
	{
		return;
	}

	SetState(ERetrieveRescueEncounterState::AwaitingRescueDialogue, true);
}

void ARetrieveRescueEncounter::HandleRescueDialogueClosed(AActor* PlayerActor)
{
	if (!HasAuthority() || State != ERetrieveRescueEncounterState::AwaitingRescueDialogue || !IsValid(PlayerActor))
	{
		return;
	}

	UInventoryComponent* Inventory = PlayerActor->FindComponentByClass<UInventoryComponent>();
	if (GoldReward > 0 && (!Inventory || !Inventory->AddCurrency(GoldReward)))
	{
		return;
	}

	SetState(ERetrieveRescueEncounterState::RewardClaimed, true);
	SetState(ERetrieveRescueEncounterState::MerchantAvailable, true);
}

void ARetrieveRescueEncounter::ResetForNewGame()
{
	State = ERetrieveRescueEncounterState::EnemiesAlive;
	ApplyState();
}

void ARetrieveRescueEncounter::HandleSaveLoaded()
{
	State = ERetrieveRescueEncounterState::EnemiesAlive;
	RestoreSavedState();
	ApplyState();
}

void ARetrieveRescueEncounter::RestoreSavedState()
{
	if (EncounterId.IsNone())
	{
		return;
	}

	const UGameInstance* GI = GetGameInstance();
	const URetrieveSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	const URetrieveSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	const FRetrieveRescueEncounterSaveData* Saved = SaveGame ? SaveGame->RescueEncounters.Find(EncounterId) : nullptr;
	if (!Saved)
	{
		return;
	}

	if (Saved->bMerchantUnlocked)
	{
		State = ERetrieveRescueEncounterState::MerchantAvailable;
	}
	else if (Saved->bRewardClaimed)
	{
		State = ERetrieveRescueEncounterState::RewardClaimed;
	}
	else if (Saved->bEnemiesDefeated)
	{
		State = ERetrieveRescueEncounterState::AwaitingRescueDialogue;
	}
}

void ARetrieveRescueEncounter::PersistState()
{
	if (EncounterId.IsNone())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	URetrieveSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	URetrieveSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	if (!SaveGame || !SaveSubsystem)
	{
		return;
	}

	FRetrieveRescueEncounterSaveData& Saved = SaveGame->RescueEncounters.FindOrAdd(EncounterId);
	Saved.bEnemiesDefeated = State != ERetrieveRescueEncounterState::EnemiesAlive;
	Saved.bRewardClaimed = State == ERetrieveRescueEncounterState::RewardClaimed
		|| State == ERetrieveRescueEncounterState::MerchantAvailable;
	Saved.bMerchantUnlocked = State == ERetrieveRescueEncounterState::MerchantAvailable;
	SaveSubsystem->FlushWorldState();
}

void ARetrieveRescueEncounter::ApplyState()
{
	if (ACharacter* CaptiveCharacter = Cast<ACharacter>(RescueNPC))
	{
		if (UCharacterMovementComponent* Movement = CaptiveCharacter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}

		if (AController* Controller = CaptiveCharacter->GetController())
		{
			Controller->StopMovement();
		}

		if (USkeletalMeshComponent* Mesh = CaptiveCharacter->GetMesh())
		{
			Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				// static 원시 포인터 캐싱은 몽타주가 GC 수거된 뒤 댕글링 → 패키지에서 Montage_Play 시
				// 쓰레기 주소 역참조로 ACCESS_VIOLATION 크래시. GC 루트가 아니므로 매 호출 시 로드한다
				// (이미 로드돼 있으면 빠른 조회, 재생 중엔 AnimInstance가 몽타주를 참조해 유지됨).
				UAnimMontage* CaptiveMontage = LoadObject<UAnimMontage>(
					nullptr, TEXT("/Game/Retrieve/Character/Animations/Rescue/AM_RescuePray_Kneeling.AM_RescuePray_Kneeling"));
				UAnimMontage* ExitMontage = LoadObject<UAnimMontage>(
					nullptr, TEXT("/Game/Retrieve/Character/Animations/Rescue/AM_RescuePray_Kneeling_Exit.AM_RescuePray_Kneeling_Exit"));

				if (State == ERetrieveRescueEncounterState::EnemiesAlive && CaptiveMontage)
				{
					AnimInstance->Montage_Play(CaptiveMontage);
					AnimInstance->Montage_SetNextSection(NAME_Default, NAME_Default, CaptiveMontage);
				}
				else if (State == ERetrieveRescueEncounterState::AwaitingRescueDialogue && ExitMontage)
				{
					if (CaptiveMontage)
					{
						AnimInstance->Montage_Stop(0.1f, CaptiveMontage);
					}
					AnimInstance->Montage_Play(ExitMontage);
				}
			}
		}
	}

	const bool bMerchantAvailable = State == ERetrieveRescueEncounterState::MerchantAvailable;
	SetActorAvailable(MerchantNPC, bMerchantAvailable);
	SetActorAvailable(RescueNPC, !bMerchantAvailable);

	if (RescueNPC)
	{
		if (URetrieveDialogueComponent* Dialogue = RescueNPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			const bool bCanThank = State == ERetrieveRescueEncounterState::AwaitingRescueDialogue
				|| State == ERetrieveRescueEncounterState::RewardClaimed;
			Dialogue->DefaultGreetingLines = bCanThank ? ThankYouDialogueLines : HelpDialogueLines;
		}
	}

	RefreshObjectiveMarkers();

	OnStateChanged.Broadcast(State);
	ReceiveRescueStateChanged(State);
}

void ARetrieveRescueEncounter::RefreshObjectiveMarkers()
{
	// 구출형은 수락 대화가 없으므로 "적을 처치해 구출" 자체가 수행 단계다.
	// 발견 반경 안에 들어와야 표시되므로 처음부터 화면에 떠 있지 않는다.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString Prefix = (EncounterId.IsNone() ? GetName() : EncounterId.ToString()) + TEXT("_");
	URetrieveObjectiveMarkerSubsystem::RemoveMarkersByPrefix(World, Prefix);

	const FText Title = QuestTitle.IsEmptyOrWhitespace()
		? NSLOCTEXT("RetrieveQuest", "RescueTitle", "포로 구출")
		: QuestTitle;

	switch (State)
	{
	case ERetrieveRescueEncounterState::EnemiesAlive:
	{
		// 적이 남아 있으면 적 무리를, 스포너가 없으면 포로 위치를 가리킨다.
		AActor* Target = EnemySpawner ? static_cast<AActor*>(EnemySpawner.Get())
		                              : static_cast<AActor*>(RescueNPC.Get());
		FText Progress = NSLOCTEXT("RetrieveQuest", "RescueFight", "적 처치");
		if (EnemySpawner)
		{
			const int32 Live = EnemySpawner->GetLiveSpawnCount();
			if (Live > 0)
			{
				Progress = FText::Format(
					NSLOCTEXT("RetrieveQuest", "RescueRemaining", "남은 적 {0}"), FText::AsNumber(Live));
			}
		}
		URetrieveObjectiveMarkerSubsystem::RegisterActorMarker(
			World, FName(*(Prefix + TEXT("Obj"))), ERetrieveObjectiveMarkerKind::Side,
			Title, Target, Progress);
		break;
	}

	case ERetrieveRescueEncounterState::AwaitingRescueDialogue:
		URetrieveObjectiveMarkerSubsystem::RegisterActorMarker(
			World, FName(*(Prefix + TEXT("TurnIn"))), ERetrieveObjectiveMarkerKind::TurnIn,
			Title, RescueNPC, NSLOCTEXT("RetrieveQuest", "RescueTalk", "말을 걸어 보세요"), 150.0f, 50);
		break;

	default:
		break; // 보상 수령 이후 — 마커 없음
	}
}

void ARetrieveRescueEncounter::SetActorAvailable(AActor* Actor, bool bAvailable) const
{
	if (!IsValid(Actor))
	{
		return;
	}

	Actor->SetActorHiddenInGame(!bAvailable);
	Actor->SetActorEnableCollision(bAvailable);
	Actor->SetActorTickEnabled(bAvailable);
}

void ARetrieveRescueEncounter::SetState(ERetrieveRescueEncounterState NewState, bool bPersist)
{
	if (State == NewState)
	{
		return;
	}

	State = NewState;
	ApplyState();
	if (bPersist)
	{
		PersistState();
	}
}

void ARetrieveRescueEncounter::FindNearestLoadedBonfire()
{
	const FVector Origin = EnemySpawner ? EnemySpawner->GetActorLocation() : GetActorLocation();
	float BestDistanceSquared = TNumericLimits<float>::Max();
	ARetrieveBonfireActor* BestBonfire = nullptr;

	for (TActorIterator<ARetrieveBonfireActor> It(GetWorld()); It; ++It)
	{
		const float DistanceSquared = FVector::DistSquared(Origin, It->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestBonfire = *It;
		}
	}

	DestinationBonfire = BestBonfire;
}