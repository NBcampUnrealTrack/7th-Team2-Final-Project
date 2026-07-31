#include "World/RetrieveDoorBase.h"

#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "Save/RetrieveSaveSubsystem.h"

ARetrieveDoorBase::ARetrieveDoorBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ARetrieveDoorBase::BeginPlay()
{
	Super::BeginPlay();

	bOpen = bStartOpen;
	ApplyDoorState(/*bInstant=*/true);

	if (!HasAuthority()) { return; }

	// 세이브(슬롯)에서 초기 상태 복원(스트림인/최초 로드 대응). 키 없으면 기본값 유지.
	HandleSaveLoaded();

	// 로드는 in-place라 BeginPlay가 다시 안 돈다. 로드 완료 시 다시 복원하도록 구독.
	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (!IsValid(SaveSub)) { return; }
	SaveSub->OnWorldObjectStatesChanged.AddUniqueDynamic(this, &ARetrieveDoorBase::HandleSaveLoaded);
}

void ARetrieveDoorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay는 Super를 항상 호출해야 하므로 early return 대신 단일 if만 사용.
	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (IsValid(SaveSub))
	{
		SaveSub->OnWorldObjectStatesChanged.RemoveDynamic(this, &ARetrieveDoorBase::HandleSaveLoaded);
	}

	Super::EndPlay(EndPlayReason);
}

FName ARetrieveDoorBase::GetSaveId() const
{
	return PersistentId.IsNone() ? GetFName() : PersistentId;
}

URetrieveSaveSubsystem* ARetrieveDoorBase::GetSaveSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	if (!IsValid(GI)) { return nullptr; }
	return GI->GetSubsystem<URetrieveSaveSubsystem>();
}

void ARetrieveDoorBase::PushDoorStateToSave() const
{
	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (!IsValid(SaveSub)) { return; }
	SaveSub->SetWorldObjectState(GetSaveId(), bOpen ? 1 : 0);
}

void ARetrieveDoorBase::HandleSaveLoaded()
{
	if (!HasAuthority()) { return; }

	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (!IsValid(SaveSub)) { return; }

	uint8 State = 0;
	const bool bWantOpen = SaveSub->TryGetWorldObjectState(GetSaveId(), State)
		? (State != 0)
		: bStartOpen;   // 세이브에 없으면 기본값

	// 논리값이 같더라도 BP Timeline/메시/Collision이 현재 플레이 세션의
	// 이전 표현을 유지할 수 있으므로 슬롯 상태를 항상 강제 적용한다.
	bOpen = bWantOpen;
	ApplyDoorState(/*bInstant=*/true);
}

void ARetrieveDoorBase::OpenDoor()
{
	if (!HasAuthority() || bOpen)
	{
		return;
	}
	bOpen = true;
	ApplyDoorState(false);
	PushDoorStateToSave();
}

void ARetrieveDoorBase::CloseDoor()
{
	if (!HasAuthority() || !bOpen)
	{
		return;
	}
	bOpen = false;
	ApplyDoorState(false);
	PushDoorStateToSave();
}

void ARetrieveDoorBase::ToggleDoor()
{
	if (!HasAuthority())
	{
		return;
	}
	bOpen = !bOpen;
	ApplyDoorState(false);
	PushDoorStateToSave();
}

void ARetrieveDoorBase::OnRep_bOpen()
{
	ApplyDoorState(false);
}

void ARetrieveDoorBase::ApplyDoorState(bool bInstant)
{
	if (bOpen)
	{
		OnDoorOpened(bInstant);
	}
	else
	{
		OnDoorClosed(bInstant);
	}
}

void ARetrieveDoorBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARetrieveDoorBase, bOpen);
}
