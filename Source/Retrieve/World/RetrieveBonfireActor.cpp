                                                                       #include "World/RetrieveBonfireActor.h"
#include "Components/World/RetrieveMapIconComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Components/Player/StaminaComponent.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Player/RetrievePlayerController.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "UI/Bonfire/BonfireMenuWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Core/RetrieveGameState.h"
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
	// 비주얼 상태는 상태 변경 시점(활성화/세이브 복원/Begin Play 등)에만 갱신하므로
	// 매 프레임 틱이 필요 없다.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	ArrivalPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrivalPoint"));
	ArrivalPoint->SetupAttachment(RootComponent);
	// 모닥불 메시/불꽃과 겹치지 않도록 충분히 앞쪽에 둔다(빠른 이동 도착 지점).
	ArrivalPoint->SetRelativeLocation(FVector(280.0f, 0.0f, 0.0f));
	ArrivalPoint->SetArrowColor(FLinearColor::Yellow);

	MapIconComponent = CreateDefaultSubobject<URetrieveMapIconComponent>(TEXT("MapIconComponent"));
	// UActorComponent 상속이므로 SetupAttachment 불필요 — 오너 액터 위치를 자동 참조
	MapIconComponent->IconType = ERetrieveMapIconType::Bonfire;
	MapIconComponent->bShowOnMinimap = false; // 활성화 전까지 숨김

	InteractionComponent = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("InteractionComponent"));
	// 화톳불은 1회성 픽업이 아니므로 절대 자기 자신을 파괴하지 않는다.
	InteractionComponent->bDestroyOwnerOnApplied = false;
	BonfireMenuClass = TSoftClassPtr<UBonfireMenuWidget>(
		FSoftObjectPath(TEXT("/Game/Retrieve/UI/Bonfire/WBP_BonfireMenu.WBP_BonfireMenu_C")));

	// 불꽃 VFX — 비활성화 상태에서는 꺼져 있다가 ActivateBonfire() 호출 시 켜진다.
	FireVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FireVFXComponent"));
	FireVFXComponent->SetupAttachment(RootComponent);
	FireVFXComponent->SetAutoActivate(false); // 비활성화 상태로 시작

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FireVFXFinder(
		TEXT("/Game/External/PolygonDarkFortress/DarkFortress/FX/NS_Fire_01"));
	if (FireVFXFinder.Succeeded())
	{
		FireVFXSystem = FireVFXFinder.Object;
		FireVFXComponent->SetAsset(FireVFXFinder.Object);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BonfireActor] NS_Fire_01 에셋을 찾지 못함 — 경로 확인 필요"));
	}

	// 휴식 시 캐릭터 몸 주변에 재생할 회복 VFX 기본값.
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> HealVFXFinder(
		TEXT("/Game/External/PolygonParticleFX/Particles/NS_Heal_02"));
	if (HealVFXFinder.Succeeded())
	{
		HealVFXSystem = HealVFXFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BonfireActor] NS_Heal_02 에셋을 찾지 못함 — 경로 확인 필요"));
	}

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

void ARetrieveBonfireActor::PostLoad()
{
	Super::PostLoad();

	ApplyBonfireVisualState();
}

void ARetrieveBonfireActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyBonfireVisualState();
}

void ARetrieveBonfireActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	ApplyBonfireVisualState();
}

void ARetrieveBonfireActor::BeginPlay()
{
	Super::BeginPlay();
	EnsureBonfireId();

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

	TryRestoreActivationFromSave();
	ApplyBonfireVisualState();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ARetrieveBonfireActor::HandleDeferredVisualStateSync));
	}
}

void ARetrieveBonfireActor::EnsureBonfireId()
{
	if (!BonfireId.IsNone())
	{
		return;
	}

	// 레벨에 배치된 액터 이름은 동일 레벨 안에서 고유하며 PIE를 다시 시작해도 유지된다.
	// 디자이너가 ID를 지정하지 않은 경우에도 저장 기능이 완전히 막히지 않도록 사용한다.
	BonfireId = GetFName();
	UE_LOG(LogTemp, Warning,
		TEXT("[BonfireActor] BonfireId가 비어 있어 액터 이름으로 자동 지정: %s"),
		*BonfireId.ToString());
}

void ARetrieveBonfireActor::HandleInteractionApplied(AActor* InteractionInstigator)
{
	ActivateBonfire();

	// 휴식 = 체력·스태미나 풀 회복. InteractionInstigator는 Pawn 또는 PlayerController로 전달될 수 있다.
	APawn* RestingPawn = Cast<APawn>(InteractionInstigator);
	if (!RestingPawn)
	{
		if (const APlayerController* PC = Cast<APlayerController>(InteractionInstigator))
		{
			RestingPawn = PC->GetPawn();
		}
	}
	if (RestingPawn)
	{
		if (URetrieveHealthComponent* HealthComp = RestingPawn->FindComponentByClass<URetrieveHealthComponent>())
		{
			HealthComp->ResetHealth();
		}
		if (UStaminaComponent* StaminaComp = RestingPawn->FindComponentByClass<UStaminaComponent>())
		{
			StaminaComp->ResetStamina();
		}

		// 회복됐다는 시각적 피드백 — 캐릭터 몸 주변에 회복 VFX 재생.
		PlayHealVFXOnPawn(RestingPawn);
	}

	// 이 모닥불을 리스폰 체크포인트로 기록(호스트 권한. OnApplied는 호스트 전용), 이미 불이 켜진 모닥불에 재휴식해도 갱신됨.
	if (UWorld* World = GetWorld())
	{
		if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			GS->SetLastCheckpointBonfire(BonfireId);
		}
	}

	// 모든 ShopComponent에 순환 재고 갱신 신호 전송
	if (UWorld* World = GetWorld())
	{
		FRetrievePlayerRestedPayload Payload;
		Payload.Instigator = InteractionInstigator;
		Payload.BonfireId = BonfireId;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RetrieveGameplayTags::Channel_Player_Rested, Payload);
	}

	const float MenuOpenDelay = GetBonfireMenuOpenDelay();
	if (MenuOpenDelay <= KINDA_SMALL_NUMBER)
	{
		TryOpenBonfireMenu(InteractionInstigator);
		return;
	}

	PendingMenuInstigator = InteractionInstigator;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BonfireMenuOpenTimerHandle);
		World->GetTimerManager().SetTimer(
			BonfireMenuOpenTimerHandle,
			this,
			&ARetrieveBonfireActor::OpenPendingBonfireMenu,
			MenuOpenDelay,
			false);
	}
	else
	{
		TryOpenBonfireMenu(InteractionInstigator);
	}
}

void ARetrieveBonfireActor::PlayHealVFXOnPawn(APawn* Pawn) const
{
	if (!HealVFXSystem || !Pawn)
	{
		return;
	}

	USceneComponent* AttachTarget = Pawn->GetRootComponent();
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			AttachTarget = Mesh;
		}
	}
	if (!AttachTarget)
	{
		return;
	}

	UNiagaraComponent* HealVFXComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		HealVFXSystem,
		AttachTarget,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		/*bAutoDestroy*/ true);

	if (!HealVFXComp)
	{
		return;
	}

	HealVFXComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealVFXComp->SetGenerateOverlapEvents(false);

	// SpawnRate 기반 이미터가 EmitterState 설정에 따라 무한 루프될 수 있으므로,
	// 일정 시간 뒤 강제로 Deactivate하여 새 파티클 생성을 멈춘다(기존 파티클은 자연 소멸).
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UNiagaraComponent> WeakHealVFXComp = HealVFXComp;
		FTimerHandle HealVFXStopTimer;
		World->GetTimerManager().SetTimer(
			HealVFXStopTimer,
			FTimerDelegate::CreateLambda([WeakHealVFXComp]()
			{
				if (WeakHealVFXComp.IsValid())
				{
					WeakHealVFXComp->Deactivate();
				}
			}),
			HealVFXDuration,
			false);
	}
}

void ARetrieveBonfireActor::OpenPendingBonfireMenu()
{
	TryOpenBonfireMenu(PendingMenuInstigator.Get());
	PendingMenuInstigator.Reset();
}

float ARetrieveBonfireActor::GetBonfireMenuOpenDelay() const
{
	if (InteractionComponent)
	{
		const float AnimationDuration =
			InteractionComponent->GetEffectiveInteractionAnimationDuration();
		if (AnimationDuration > KINDA_SMALL_NUMBER)
		{
			return AnimationDuration;
		}
	}

	return FMath::Max(0.0f, BonfireMenuFallbackOpenDelay);
}

bool ARetrieveBonfireActor::TryOpenBonfireMenu(AActor* InteractionInstigator) const
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

	ARetrievePlayerController* RetrievePC = Cast<ARetrievePlayerController>(PC);
	UClass* MenuClass = BonfireMenuClass.LoadSynchronous();

	if (!RetrievePC || !MenuClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BonfireActor] Failed to open bonfire menu - PC=%s MenuClass=%s"),
			PC ? TEXT("Valid") : TEXT("None"),
			MenuClass ? *MenuClass->GetName() : TEXT("None"));
		return false;
	}

	// 커서·입력 모드 관리는 PlayerController의 OpenExclusivePanel에 완전히 위임한다.
	// 같은 패널이 이미 열려있으면 OpenExclusivePanel이 닫아주므로 별도 처리 불필요.
	RetrievePC->OpenExclusivePanel(MenuClass, FKey());

	// 패널 생성 후 BonfireId를 주입한다.
	if (UBonfireMenuWidget* BonfirePanel = Cast<UBonfireMenuWidget>(RetrievePC->GetActivePanel()))
	{
		BonfirePanel->BonfireId = BonfireId;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[BonfireActor] Bonfire menu opened via PlayerController - BonfireId=%s"),
		*BonfireId.ToString());
	return true;
}

void ARetrieveBonfireActor::ConfigurePersistentInteractionTarget() const
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
	if (bIsActivated)
	{
		ApplyBonfireVisualState();
		return false;
	}

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

	ApplyActivatedState(true);

	// 점화한 모닥불도 리스폰 체크포인트로 기록한다("마지막 사용 모닥불" 의미).
	// (기존에는 휴식 시에만 기록되어, 점화만 한 뒤 죽으면 시작 지역으로 리스폰되는 문제가 있었다.)
	if (ARetrieveGameState* GS = GetWorld() ? GetWorld()->GetGameState<ARetrieveGameState>() : nullptr)
	{
		GS->SetLastCheckpointBonfire(BonfireId);
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
				ApplyActivatedState(false);
				UE_LOG(LogTemp, Log,
					TEXT("[BonfireActor] 저장 파일에서 활성화 복원 — BonfireId=%s"),
					*BonfireId.ToString());
			}
		}
	}
}

void ARetrieveBonfireActor::ApplyActivatedState(bool bRegisterWithSave)
{
	ApplyBonfireVisualState();

	if (!bRegisterWithSave)
	{
		return;
	}

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
}

void ARetrieveBonfireActor::ApplyBonfireVisualState()
{
	if (FireVFXComponent)
	{
		if (FireVFXSystem && FireVFXComponent->GetAsset() != FireVFXSystem)
		{
			FireVFXComponent->SetAsset(FireVFXSystem);
		}

		FireVFXComponent->SetHiddenInGame(!bIsActivated, true);
		FireVFXComponent->SetVisibility(bIsActivated, true);

		if (FireVFXComponent->IsRegistered())
		{
			// 등록 이후에는 SetAutoActivate가 효과가 없고 경고만 찍히므로(매 Tick 호출 시 로그 스팸)
			// 상태는 Activate/DeactivateImmediate로 직접 구동한다.
			if (bIsActivated)
			{
				FireVFXComponent->Activate(true);
			}
			else
			{
				FireVFXComponent->DeactivateImmediate();
			}
		}
		else
		{
			// 아직 등록 전(PostLoad/OnConstruction)에만 AutoActivate가 초기 상태를 결정한다.
			FireVFXComponent->SetAutoActivate(bIsActivated);
		}
	}

	if (MapIconComponent)
	{
		MapIconComponent->bShowOnMinimap = bIsActivated;
	}
}

void ARetrieveBonfireActor::HandleDeferredVisualStateSync()
{
	ApplyBonfireVisualState();
}
