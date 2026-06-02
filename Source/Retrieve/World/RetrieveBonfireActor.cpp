#include "World/RetrieveBonfireActor.h"
#include "Components/RetrieveMapIconComponent.h"
#include "Components/RetrieveInteractionResponseComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

// InteractionManager 플러그인의 감지 컴포넌트 BP 클래스 경로.
// 플러그인 위치가 바뀌면 이 경로만 수정하면 된다.
namespace
{
	const TCHAR* InteractionTargetClassPath =
		TEXT("/Game/External/InteractionManager/Blueprints/Components/Manager_InteractionTarget.Manager_InteractionTarget_C");

}

ARetrieveBonfireActor::ARetrieveBonfireActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	ArrivalPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrivalPoint"));
	ArrivalPoint->SetupAttachment(RootComponent);
	ArrivalPoint->SetRelativeLocation(FVector(120.0f, 0.0f, 0.0f));
	ArrivalPoint->SetArrowColor(FLinearColor::Yellow);

	MapIconComponent = CreateDefaultSubobject<URetrieveMapIconComponent>(TEXT("MapIconComponent"));
	// UActorComponent 상속이므로 SetupAttachment 불필요 — 오너 액터 위치를 자동 참조
	MapIconComponent->IconType = ERetrieveMapIconType::Bonfire;
	MapIconComponent->bShowOnMinimap = false; // 활성화 전까지 숨김

	InteractionComponent = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("InteractionComponent"));
	// 화톳불은 1회성 픽업이 아니므로 절대 자기 자신을 파괴하지 않는다.
	InteractionComponent->bDestroyOwnerOnApplied = false;
	BonfireMenuClass = TSoftClassPtr<UUserWidget>(
		FSoftObjectPath(TEXT("/Game/Retrieve/UI/Bonfire/WBP_BonfireMenu.WBP_BonfireMenu_C")));

	// 감지/프롬프트 컴포넌트(Manager_InteractionTarget BP)를 "InteractionTarget" 이름으로 생성.
	// → 기존 상호작용 액터와 동일한 흐름. InteractionComponent가 BeginPlay에서 이 이름으로 자동 바인딩.
	static ConstructorHelpers::FClassFinder<UActorComponent> InteractionTargetBPClass(InteractionTargetClassPath);
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
			TEXT("[BonfireActor] Manager_InteractionTarget BP 클래스를 찾지 못함 — 경로 확인 필요: %s"),
			InteractionTargetClassPath);
	}
}

void ARetrieveBonfireActor::BeginPlay()
{
	Super::BeginPlay();

	// 화톳불은 1회성 픽업이 아니므로 절대 자기 자신을 파괴하지 않는다.
	// 생성자 기본값은 BP 인스턴스에 저장된 값에 덮어써질 수 있으므로
	// 런타임에서 한 번 더 강제로 false를 보장한다.
	if (InteractionComponent)
	{
		InteractionComponent->bDestroyOwnerOnApplied = false;
		InteractionComponent->OnApplied.AddUniqueDynamic(
			this, &ARetrieveBonfireActor::HandleInteractionApplied);
	}
	if (MapIconComponent)
	{
		MapIconComponent->bShowOnMinimap = false;
	}
	ConfigurePersistentInteractionTarget();

	if (BonfireId.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BonfireActor] BonfireId가 비어 있음 — %s. 반드시 고유한 ID를 설정하세요."),
			*GetName());
	}

	TryRestoreActivationFromSave();
}

void ARetrieveBonfireActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsValid(ActiveBonfireMenu) || !ActiveBonfireMenu->IsInViewport())
	{
		RestoreGameInputAfterMenuClosed();
	}
}

void ARetrieveBonfireActor::HandleInteractionApplied(AActor* InteractionInstigator)
{
	ActivateBonfire();
	TryOpenBonfireMenu(InteractionInstigator);
}

bool ARetrieveBonfireActor::TryOpenBonfireMenu(AActor* InteractionInstigator)
{
	APlayerController* PC = Cast<APlayerController>(InteractionInstigator);
	if (!PC)
	{
		if (const APawn* Pawn = Cast<APawn>(InteractionInstigator))
		{
			PC = Cast<APlayerController>(Pawn->GetController());
		}
	}
	if (!PC)
	{
		PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	}

	UClass* MenuClass = BonfireMenuClass.LoadSynchronous();
	if (!PC || !MenuClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BonfireActor] Failed to open bonfire menu - PC=%s MenuClass=%s"),
			PC ? TEXT("Valid") : TEXT("None"),
			MenuClass ? *MenuClass->GetName() : TEXT("None"));
		return false;
	}

	if (IsValid(ActiveBonfireMenu) && ActiveBonfireMenu->IsInViewport())
	{
		return true;
	}

	ActiveBonfireMenu = CreateWidget<UUserWidget>(PC, MenuClass);
	if (!ActiveBonfireMenu)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BonfireActor] Failed to create bonfire menu widget"));
		return false;
	}

	if (FNameProperty* BonfireIdProp =
		FindFProperty<FNameProperty>(MenuClass, FName(TEXT("BonfireId"))))
	{
		BonfireIdProp->SetPropertyValue_InContainer(ActiveBonfireMenu, BonfireId);
	}

	ActiveBonfireMenu->AddToViewport(100);
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ActiveBonfireMenu->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
	BonfireMenuPlayerController = PC;
	SetActorTickEnabled(true);

	UE_LOG(LogTemp, Log,
		TEXT("[BonfireActor] Bonfire menu opened - BonfireId=%s"),
		*BonfireId.ToString());
	return true;
}

void ARetrieveBonfireActor::RestoreGameInputAfterMenuClosed()
{
	if (APlayerController* PC = BonfireMenuPlayerController.Get())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;

		UE_LOG(LogTemp, Log,
			TEXT("[BonfireActor] Game input restored after bonfire menu closed - BonfireId=%s"),
			*BonfireId.ToString());
	}

	ActiveBonfireMenu = nullptr;
	BonfireMenuPlayerController = nullptr;
	SetActorTickEnabled(false);
}

void ARetrieveBonfireActor::ConfigurePersistentInteractionTarget()
{
	if (!InteractionTargetComponent)
	{
		return;
	}

	UClass* TargetClass = InteractionTargetComponent->GetClass();

	bool bFinishMethodConfigured = false;

	if (FByteProperty* ByteProp =
		FindFProperty<FByteProperty>(TargetClass, FName(TEXT("FinishMethod"))))
	{
		ByteProp->SetPropertyValue_InContainer(
			InteractionTargetComponent, PersistentFinishMethodValue);
		bFinishMethodConfigured = true;
	}
	else if (FEnumProperty* EnumProp =
		FindFProperty<FEnumProperty>(TargetClass, FName(TEXT("FinishMethod"))))
	{
		EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
			EnumProp->ContainerPtrToValuePtr<void>(InteractionTargetComponent),
			static_cast<int64>(PersistentFinishMethodValue));
		bFinishMethodConfigured = true;
	}

	if (FFloatProperty* DurationProp =
		FindFProperty<FFloatProperty>(TargetClass, FName(TEXT("ReactivationDuration"))))
	{
		DurationProp->SetPropertyValue_InContainer(InteractionTargetComponent, 0.0f);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[BonfireActor] Persistent interaction target configured - BonfireId=%s FinishMethod=%s"),
		*BonfireId.ToString(),
		bFinishMethodConfigured ? TEXT("ReactivateAfterCompleted") : TEXT("PropertyNotFound"));
}

bool ARetrieveBonfireActor::ActivateBonfire()
{
	// 이미 활성화 상태 → 첫 활성화 아님 (false). 메뉴는 즉시 열면 됨.
	if (bIsActivated) { return false; }

	// [멀티플레이 확장 지점]
	// 화톳불 활성화는 서버에서만 처리해야 상태가 모든 클라이언트에 일관되게 복제됨.
	// 멀티 적용 시 클라이언트 호출은 Server RPC로 래핑:
	//   UFUNCTION(Server, Reliable) void ServerActivateBonfire();
	//   if (!HasAuthority()) { ServerActivateBonfire(); return; }
	// bIsActivated는 ReplicatedUsing=OnRep_IsActivated 로 선언해 클라이언트 UI 자동 갱신.
	const ENetMode NetMode = GetNetMode();
	const bool bIsStandalone = (NetMode == NM_Standalone);
	if (!bIsStandalone && !HasAuthority())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BonfireActor] ActivateBonfire: 클라이언트에서 직접 호출됨 — 서버에서만 호출하세요. BonfireId=%s"),
			*BonfireId.ToString());
		return false;
	}

	bIsActivated = true;

	// 맵 아이콘 표시
	if (MapIconComponent)
	{
		MapIconComponent->bShowOnMinimap = true;
	}

	// SaveSubsystem에 활성화 등록
	if (UGameInstance* GI = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSub = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			const FTransform ArrivalTransform = IsValid(ArrivalPoint)
				? ArrivalPoint->GetComponentTransform()
				: GetActorTransform();

			SaveSub->MarkBonfireActivated(BonfireId, ArrivalTransform, DisplayName.ToString());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BonfireActor] 화톳불 활성화 — BonfireId=%s"), *BonfireId.ToString());

	// 첫 활성화 → HUD 알림용 델리게이트 브로드캐스트
	OnBonfireActivated.Broadcast(DisplayName, BonfireId);

	return true;
}

void ARetrieveBonfireActor::TryRestoreActivationFromSave()
{
	if (BonfireId.IsNone()) { return; }

	if (UGameInstance* GI = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSub = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			if (SaveSub->IsBonfireActivated(BonfireId))
			{
				// 파일에 이미 활성화 기록 → 시각/상태만 복원 (중복 등록 방지)
				bIsActivated = true;
				if (MapIconComponent)
				{
					MapIconComponent->bShowOnMinimap = true;
				}
				UE_LOG(LogTemp, Log,
					TEXT("[BonfireActor] 저장 파일에서 활성화 복원 — BonfireId=%s"),
					*BonfireId.ToString());
			}
		}
	}
}
