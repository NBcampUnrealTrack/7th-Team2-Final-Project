#include "World/RetrieveLeverActor.h"

#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "Save/RetrieveSaveSubsystem.h"

ARetrieveLeverActor::ARetrieveLeverActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionResponse = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("InteractionResponse"));
}

void ARetrieveLeverActor::BeginPlay()
{
	Super::BeginPlay();

	bActivated = bStartActivated;
	ApplyLeverState(/*bInstant=*/true);

	if (InteractionResponse)
	{
		InteractionResponse->OnApplied.AddUniqueDynamic(this, &ARetrieveLeverActor::HandleInteracted);
	}

	if (!HasAuthority()) { return; }

	HandleSaveLoaded();   // 스트림인/최초 로드 시 초기 상태 복원(없으면 기본값)

	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (!IsValid(SaveSub)) { return; }
	SaveSub->OnWorldObjectStatesChanged.AddUniqueDynamic(this, &ARetrieveLeverActor::HandleSaveLoaded);
}

void ARetrieveLeverActor::HandleInteracted(AActor* /*InteractionInstigator*/)
{
	if (!HasAuthority())
	{
		return;
	}
	// 래치 버튼: 이미 켜졌으면 무시.
	if (bActivated && !bToggle)
	{
		return;
	}

	bActivated = !bActivated;
	ApplyLeverState(false);
	PushLeverStateToSave();
}

void ARetrieveLeverActor::OnRep_bActivated()
{
	ApplyLeverState(false);
}

void ARetrieveLeverActor::ApplyLeverState(bool bInstant)
{
	OnLeverStateChangedBP(bActivated, bInstant);
	OnLeverChanged.Broadcast(this);
}

void ARetrieveLeverActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay는 Super를 항상 호출해야 하므로 early return 대신 단일 if만 사용.
	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (IsValid(SaveSub))
	{
		SaveSub->OnWorldObjectStatesChanged.RemoveDynamic(this, &ARetrieveLeverActor::HandleSaveLoaded);
	}
	Super::EndPlay(EndPlayReason);
}

FName ARetrieveLeverActor::GetSaveId() const
{
	return PersistentId.IsNone() ? GetFName() : PersistentId;
}

URetrieveSaveSubsystem* ARetrieveLeverActor::GetSaveSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	if (!IsValid(GI)) { return nullptr; }
	return GI->GetSubsystem<URetrieveSaveSubsystem>();
}

void ARetrieveLeverActor::PushLeverStateToSave() const
{
	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (!IsValid(SaveSub)) { return; }
	SaveSub->SetWorldObjectState(GetSaveId(), bActivated ? 1 : 0);
}

void ARetrieveLeverActor::HandleSaveLoaded()
{
	if (!HasAuthority()) { return; }

	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (!IsValid(SaveSub)) { return; }

	uint8 State = 0;
	const bool bWantActive = SaveSub->TryGetWorldObjectState(GetSaveId(), State)
		? (State != 0)
		: bStartActivated;

	bActivated = bWantActive;
	ApplyLeverState(/*bInstant=*/true);   // BP 표현과 연결된 LeverDoor를 슬롯 기준으로 항상 재평가
}

void ARetrieveLeverActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARetrieveLeverActor, bActivated);
}
