#include "Components/RetrieveMapIconComponent.h"
#include "Subsystems/RetrieveMapSubsystem.h"

URetrieveMapIconComponent::URetrieveMapIconComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveMapIconComponent::OnRegister()
{
	Super::OnRegister();

	// World Partition 지원: 셀이 스트리밍 로드될 때 OnRegister가 호출됨.
	// BeginPlay에만 의존하면 언로드 셀에서 복귀 시 등록이 누락된다.
	// 단, 에디터 모드(GIsEditor && !GIsPlayInEditorWorld)에서는 스킵.
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		RegisterWithMapSubsystem();
	}
}

void URetrieveMapIconComponent::OnUnregister()
{
	UnregisterFromMapSubsystem();
	Super::OnUnregister();
}

void URetrieveMapIconComponent::BeginPlay()
{
	Super::BeginPlay();
	// OnRegister에서 이미 등록됐지만, 혹시 월드가 아직 없던 시점이었다면 재시도.
	RegisterWithMapSubsystem();
}

void URetrieveMapIconComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromMapSubsystem();
	Super::EndPlay(EndPlayReason);
}


void URetrieveMapIconComponent::RegisterWithMapSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (URetrieveMapSubsystem* Sub = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			// 이미 등록된 경우 중복 방지 (OnRegister + BeginPlay 양쪽에서 호출될 수 있음)
			const TArray<TObjectPtr<URetrieveMapIconComponent>>& Existing = Sub->GetIcons();
			if (!Existing.Contains(this))
			{
				Sub->RegisterIcon(this);
			}
		}
	}
}

void URetrieveMapIconComponent::UnregisterFromMapSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (URetrieveMapSubsystem* Sub = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			Sub->UnregisterIcon(this);
		}
	}
}
