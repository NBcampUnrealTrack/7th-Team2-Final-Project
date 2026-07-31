#include "World/RetrievePuzzleActor.h"

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Data/Interaction/RetrieveInteractionResultAsset.h"
#include "Data/Interaction/RetrieveLootTableAsset.h"
#include "Data/Puzzle/RetrievePuzzleTableRow.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "Player/RetrievePlayerController.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "UI/Puzzle/RetrievePuzzlePanelWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

// InteractionManager 플러그인의 감지 컴포넌트 BP 클래스 경로(Bonfire와 동일).
namespace
{
	const TCHAR* PuzzleInteractionTargetClassPath =
		TEXT("/Game/External/InteractionManager/Blueprints/Components/Manager_InteractionTarget.Manager_InteractionTarget_C");
}

ARetrievePuzzleActor::ARetrievePuzzleActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(SceneRoot);

	InteractionComponent = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("InteractionResponse"));

	// 감지/프롬프트 컴포넌트(Manager_InteractionTarget BP)를 "InteractionTarget" 이름으로 생성.
	// → InteractionComponent가 BeginPlay에서 이 이름으로 자동 바인딩(bAutoBindInteractionManager).
	static ConstructorHelpers::FClassFinder<UActorComponent> InteractionTargetBPClass(PuzzleInteractionTargetClassPath);
	if (InteractionTargetBPClass.Succeeded())
	{
		InteractionTargetComponent = Cast<UActorComponent>(CreateDefaultSubobject(
			TEXT("InteractionTarget"),
			UActorComponent::StaticClass(),
			InteractionTargetBPClass.Class,
			/*bIsRequired*/ false,
			/*bIsTransient*/ false));
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[PuzzleActor] Manager_InteractionTarget BP 클래스를 찾지 못함 — 경로 확인: %s"),
			PuzzleInteractionTargetClassPath);
	}
}

void ARetrievePuzzleActor::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionComponent)
	{
		InteractionComponent->OnApplied.AddUniqueDynamic(this, &ARetrievePuzzleActor::HandlePuzzleInteracted);
	}
	ConfigurePersistentInteractionTarget();

	HandleSaveLoaded();   // 스트림인/최초 로드 시 bSolved 복원(없으면 false)

	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (!IsValid(SaveSub)) { return; }
	SaveSub->OnWorldObjectStatesChanged.AddUniqueDynamic(this, &ARetrievePuzzleActor::HandleSaveLoaded);
}

void ARetrievePuzzleActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay는 Super를 항상 호출해야 하므로 early return 대신 단일 if만 사용.
	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (IsValid(SaveSub))
	{
		SaveSub->OnWorldObjectStatesChanged.RemoveDynamic(this, &ARetrievePuzzleActor::HandleSaveLoaded);
	}
	Super::EndPlay(EndPlayReason);
}

void ARetrievePuzzleActor::OpenPuzzleFor(AActor* InteractionInstigator)
{
	HandlePuzzleInteracted(InteractionInstigator);
}

void ARetrievePuzzleActor::HandlePuzzleInteracted(AActor* InteractionInstigator)
{
	const FRetrievePuzzleTableRow* Row = PuzzleRow.GetRow<FRetrievePuzzleTableRow>(TEXT("ARetrievePuzzleActor::HandlePuzzleInteracted"));
	if (!Row)
	{
		return;
	}

	PendingInstigator = InteractionInstigator;

	URetrievePuzzlePanelWidget* Panel = OpenPuzzlePanel(InteractionInstigator);
	if (!Panel)
	{
		return;
	}

	ActivePuzzlePanel = Panel;
	Panel->OnPuzzleSolved.AddUniqueDynamic(this, &ARetrievePuzzleActor::HandlePuzzleSolved);
	Panel->SetupPuzzle(Row->Board);
}

URetrievePuzzlePanelWidget* ARetrievePuzzleActor::OpenPuzzlePanel(AActor* InteractionInstigator)
{
	// 상호작용한 플레이어의 컨트롤러 → 없으면 첫 로컬 PC로 폴백.
	ARetrievePlayerController* PC = nullptr;
	if (const APawn* Pawn = Cast<APawn>(InteractionInstigator))
	{
		PC = Cast<ARetrievePlayerController>(Pawn->GetController());
	}
	if (!PC)
	{
		PC = Cast<ARetrievePlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
	}
	if (!PC)
	{
		return nullptr;
	}

	UClass* PanelClass = PuzzlePanelClass.LoadSynchronous();
	if (!PanelClass)
	{
		return nullptr;
	}

	PC->OpenExclusivePanel(PanelClass, PanelToggleKey);

	URetrievePuzzlePanelWidget* Panel = Cast<URetrievePuzzlePanelWidget>(PC->GetActivePanel());

	// OpenExclusivePanel은 Game-and-UI 모드 → 위젯이 소비 안 한 입력이 게임으로 샌다.
	// 퍼즐을 푸는 동안엔 입력을 UI에 완전히 묶는다(패널이 닫히면 CloseActivePanel이 GameOnly로 복원).
	if (Panel)
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(Panel->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
	return Panel;
}

void ARetrievePuzzleActor::HandlePuzzleSolved()
{
	// 1회성 보상: 이미 풀어 보상을 준 적이 있으면 재지급하지 않음.
	if (bSolved)
	{
		return;
	}
	bSolved = true;

	// 세이브에 기록(로드 후 보상 재지급 방지 상태 지속).
	if (URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem())
	{
		SaveSub->SetWorldObjectState(GetSaveId(), 1);
	}

	AActor* InteractionInstigator = PendingInstigator.Get();
	ApplySolveResults(InteractionInstigator);
	OnPuzzleSolved.Broadcast(InteractionInstigator);
}

FName ARetrievePuzzleActor::GetSaveId() const
{
	return PersistentId.IsNone() ? GetFName() : PersistentId;
}

URetrieveSaveSubsystem* ARetrievePuzzleActor::GetSaveSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	if (!IsValid(GI)) { return nullptr; }
	return GI->GetSubsystem<URetrieveSaveSubsystem>();
}

void ARetrievePuzzleActor::HandleSaveLoaded()
{
	URetrieveSaveSubsystem* SaveSub = GetSaveSubsystem();
	if (!IsValid(SaveSub)) { return; }

	uint8 State = 0;
	bSolved = SaveSub->TryGetWorldObjectState(GetSaveId(), State) && (State != 0);
}

void ARetrievePuzzleActor::ApplySolveResults(AActor* InteractionInstigator)
{
	const FRetrievePuzzleTableRow* Row = PuzzleRow.GetRow<FRetrievePuzzleTableRow>(TEXT("ARetrievePuzzleActor::ApplySolveResults"));
	if (!Row)
	{
		return;
	}

	// 로컬/싱글 우선: 푼 클라이언트에서 직접 적용. MP면 서버 권위로 위임 필요.
	for (const TObjectPtr<URetrieveInteractionResultAsset>& Result : Row->SolveResults)
	{
		if (Result)
		{
			Result->ApplyResult(this, InteractionInstigator, this);
		}
	}

	// 루트테이블 보상: 굴려서 instigator 인벤토리에 지급(ResponseComponent의 DirectLootTable과 동일 방식).
	if (Row->SolveLootTables.Num() > 0)
	{
		UInventoryComponent* Inventory =
			InteractionInstigator ? InteractionInstigator->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (Inventory)
		{
			FRandomStream Stream;
			Stream.GenerateNewSeed();
			for (const TObjectPtr<URetrieveLootTableAsset>& Table : Row->SolveLootTables)
			{
				if (!Table)
				{
					continue;
				}
				const TArray<FRetrievePickupEntry> Drops = Table->RollLoot(Stream);
				for (const FRetrievePickupEntry& Drop : Drops)
				{
					if (Drop.ItemId.IsNone())
					{
						continue;
					}
					Inventory->AddItem(Drop.ItemId, Drop.ItemCategoryTag, Drop.Quantity);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Puzzle] %s: SolveLootTables 지정됐지만 instigator에 InventoryComponent 없음"), *GetName());
		}
	}
}

void ARetrievePuzzleActor::ConfigurePersistentInteractionTarget() const
{
	if (!InteractionTargetComponent)
	{
		return;
	}

	UClass* TargetClass = InteractionTargetComponent->GetClass();

	// 완료 후에도 다시 상호작용(재오픈) 가능하도록 FinishMethod / ReactivationDuration 설정.
	if (FByteProperty* ByteProp = FindFProperty<FByteProperty>(TargetClass, FName(TEXT("FinishMethod"))))
	{
		ByteProp->SetPropertyValue_InContainer(InteractionTargetComponent, PersistentFinishMethodValue);
	}
	else if (FEnumProperty* EnumProp = FindFProperty<FEnumProperty>(TargetClass, FName(TEXT("FinishMethod"))))
	{
		EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
			EnumProp->ContainerPtrToValuePtr<void>(InteractionTargetComponent),
			static_cast<int64>(PersistentFinishMethodValue));
	}

	if (FFloatProperty* DurationProp = FindFProperty<FFloatProperty>(TargetClass, FName(TEXT("ReactivationDuration"))))
	{
		DurationProp->SetPropertyValue_InContainer(InteractionTargetComponent, 0.0f);
	}
}
