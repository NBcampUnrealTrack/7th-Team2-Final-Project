#include "World/RetrieveMusicZoneVolume.h"

#include "Audio/RetrieveMusicSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/Combat/CombatStanceComponent.h"

ARetrieveMusicZoneVolume::ARetrieveMusicZoneVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(200.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	Box->OnComponentBeginOverlap.AddDynamic(this, &ARetrieveMusicZoneVolume::HandleBeginOverlap);
	Box->OnComponentEndOverlap.AddDynamic(this, &ARetrieveMusicZoneVolume::HandleEndOverlap);
}

void ARetrieveMusicZoneVolume::BeginPlay()
{
	Super::BeginPlay();

	// 서브시스템이 GetAllActorsOfClass(World Partition에서 불안정)에 의존하지 않도록 자기 자신을 등록한다.
	if (URetrieveMusicSubsystem* Music = GetWorld() ? GetWorld()->GetSubsystem<URetrieveMusicSubsystem>() : nullptr)
	{
		Music->RegisterZone(this);
	}
}

void ARetrieveMusicZoneVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URetrieveMusicSubsystem* Music = GetWorld() ? GetWorld()->GetSubsystem<URetrieveMusicSubsystem>() : nullptr)
	{
		Music->UnregisterZone(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ARetrieveMusicZoneVolume::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!OtherActor || !OtherActor->FindComponentByClass<UCombatStanceComponent>() || bPlayerInside)
	{
		return;
	}

	bPlayerInside = true;
	if (URetrieveMusicSubsystem* Music = GetWorld() ? GetWorld()->GetSubsystem<URetrieveMusicSubsystem>() : nullptr)
	{
		Music->EnterMusicZone(ZoneBGM, ZoneCombatBGM, bUseCombatMusic);
	}
}

void ARetrieveMusicZoneVolume::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (!OtherActor || !OtherActor->FindComponentByClass<UCombatStanceComponent>() || !bPlayerInside)
	{
		return;
	}

	bPlayerInside = false;
	if (URetrieveMusicSubsystem* Music = GetWorld() ? GetWorld()->GetSubsystem<URetrieveMusicSubsystem>() : nullptr)
	{
		Music->ExitMusicZone();
	}
}

void ARetrieveMusicZoneVolume::RefreshPlayerOverlap()
{
	if (!Box)
	{
		return;
	}

	// 등록이 누락됐을 수 있으니 먼저 갱신한다(여기서 Begin/EndOverlap이 정상 발화하면 아래 가드로 중복 방지).
	Box->UpdateOverlaps();

	// 그래도 이벤트가 없었던 경우를 대비해 현재 겹친 액터를 직접 확인한다(이벤트 비의존).
	TArray<AActor*> Overlapping;
	Box->GetOverlappingActors(Overlapping);

	bool bPlayerNow = false;
	for (const AActor* Other : Overlapping)
	{
		if (Other && Other->FindComponentByClass<UCombatStanceComponent>())
		{
			bPlayerNow = true;
			break;
		}
	}

	URetrieveMusicSubsystem* Music = GetWorld() ? GetWorld()->GetSubsystem<URetrieveMusicSubsystem>() : nullptr;
	if (!Music)
	{
		return;
	}

	if (bPlayerNow && !bPlayerInside)
	{
		bPlayerInside = true;
		Music->EnterMusicZone(ZoneBGM, ZoneCombatBGM, bUseCombatMusic);
	}
	else if (!bPlayerNow && bPlayerInside)
	{
		bPlayerInside = false;
		Music->ExitMusicZone();
	}
}
