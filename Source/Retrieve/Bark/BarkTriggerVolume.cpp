#include "BarkTriggerVolume.h"

#include "BarkSubsystem.h"
#include "Components/BoxComponent.h"

ABarkTriggerVolume::ABarkTriggerVolume()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(200.f));
	Box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Box->SetGenerateOverlapEvents(true);
	Box->OnComponentBeginOverlap.AddDynamic(this, &ABarkTriggerVolume::HandleBeginOverlap);
}

void ABarkTriggerVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                            const FHitResult& Sweep)
{
	if (bOnce && bFired)
	{
		return;
	}
	// 로컬 플레이어 폰만 (Manual barks는 클라이언트별 로컬).
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsLocallyControlled() || !Pawn->IsPlayerControlled())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UBarkSubsystem* Bark = World->GetSubsystem<UBarkSubsystem>())
		{
			Bark->RequestBarkByKey(BarkKeyTag);
			bFired = true;
		}
	}
}
