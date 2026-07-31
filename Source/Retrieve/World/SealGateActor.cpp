#include "World/SealGateActor.h"

#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Components/TimelineComponent.h"
#include "Core/RetrieveGameState.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Quest/QuestBranchComponent.h"
#include "Save/RetrieveSaveSubsystem.h"

ASealGateActor::ASealGateActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionResponse = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("InteractionResponse"));

	RequiredStep = RetrieveGameplayTags::Quest_Step_SealUnlocked;
	OpenStep = RetrieveGameplayTags::Quest_Step_SealOpened;
}

void ASealGateActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASealGateActor, bOpened);
}

void ASealGateActor::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionResponse)
	{
		InteractionResponse->OnApplied.AddUniqueDynamic(this, &ASealGateActor::HandleInteracted);
	}

	if (!HasAuthority()) { return; }

	HandleSaveLoaded();
	if (URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem())
	{
		SaveSub->OnWorldObjectStatesChanged.AddUniqueDynamic(
			this, &ASealGateActor::HandleSaveLoaded);
	}
}

void ASealGateActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem())
	{
		SaveSub->OnWorldObjectStatesChanged.RemoveDynamic(
			this, &ASealGateActor::HandleSaveLoaded);
	}

	Super::EndPlay(EndPlayReason);
}

void ASealGateActor::HandleInteracted(AActor* /*InteractionInstigator*/)
{
	if (!HasAuthority() || bOpened)
	{
		return;
	}

	UQuestBranchComponent* QuestBranchComponent = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (const ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			QuestBranchComponent = GS->GetQuestBranchComponent();
		}
	}
	if (!QuestBranchComponent)
	{
		return;
	}

	// 봉인이 해제될 때까지 (세 원소 강화 모두 완료) 개방 불가
	if (RequiredStep.IsValid() && !QuestBranchComponent->IsStepCompleted(RequiredStep))
	{
		UE_LOG(LogTemp, Log, TEXT("[SealGate] %s: '%s' 미완료, 봉인 해제 불가"), *GetName(), *RequiredStep.ToString());
		return;
	}

	if (OpenStep.IsValid())
	{
		QuestBranchComponent->CompleteStep(OpenStep);
		if (!QuestBranchComponent->IsStepCompleted(OpenStep))
		{
			UE_LOG(LogTemp, Warning, TEXT("[SealGate] %s: '%s' 완료 실패"),
				*GetName(), *OpenStep.ToString());
			return;
		}
	}

	bOpened = true;
	OnRep_bOpened();
	ForceNetUpdate();

	UE_LOG(LogTemp, Log, TEXT("[SealGate] %s: 개방됨 → '%s' 완료"), *GetName(), *OpenStep.ToString());
}

void ASealGateActor::OnRep_bOpened()
{
	if (bOpened)
	{
		OnGateOpened.Broadcast();
	}
	else
	{
		OnGateClosed.Broadcast();
	}
}

void ASealGateActor::HandleSaveLoaded()
{
	if (!HasAuthority()) { return; }

	const UWorld* World = GetWorld();
	const ARetrieveGameState* GS =
		World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	const UQuestBranchComponent* Quest =
		GS ? GS->GetQuestBranchComponent() : nullptr;

	bOpened = Quest
		&& OpenStep.IsValid()
		&& Quest->IsStepCompleted(OpenStep);
	OnRep_bOpened();

	// 기존 BP는 OnGateClosed가 연결되어 있지 않고 Timeline이 하나뿐이다.
	// 슬롯 로드 때 그 Timeline만 최종 프레임으로 맞춰 이전 슬롯의 표현이 남지 않게 한다.
	if (UTimelineComponent* GateTimeline = FindComponentByClass<UTimelineComponent>())
	{
		GateTimeline->Stop();
		GateTimeline->SetPlaybackPosition(
			bOpened ? GateTimeline->GetTimelineLength() : 0.0f,
			/*bFireEvents=*/false,
			/*bFireUpdate=*/true);
	}
	ForceNetUpdate();
}

URetrieveSaveSubsystem* ASealGateActor::GetSaveSubsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
}
