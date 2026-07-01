#include "BarkSpeakerComponent.h"

#include "BarkSubsystem.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"

UBarkSpeakerComponent::UBarkSpeakerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBarkSpeakerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UBarkSubsystem* Bark = World->GetSubsystem<UBarkSubsystem>())
		{
			Bark->RegisterSpeaker(this);
		}
	}
}

void UBarkSpeakerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UBarkSubsystem* Bark = World->GetSubsystem<UBarkSubsystem>())
		{
			Bark->UnregisterSpeaker(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UBarkSpeakerComponent::RouteBark(const FRetrieveBarkPayload& Payload)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 싱글플레이(현재): 항상 로컬 발행
	UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_UI_BarkRequested, Payload);
	
	// TODO(coop): bSynchronized && GetOwner()->HasAuthority()이면 위 로컬 발행을 호스트-권한 멀티캐스트로 대체:
	// UFUNCTION(NetMulticast, Unreliable) void Multicast_Bark(FRetrieveBarkPayload Payload);
	// 각 클라이언트가 받아 위와 동일하게 로컬 발행(모든 클라이언트가 Lumen 대사를 함께 듣는다).
	// 비동기 NPC 대사는 클라이언트별 로컬이므로 동기화하지 않는다(각 클라이언트가 자기 근접만 스캔).
}
