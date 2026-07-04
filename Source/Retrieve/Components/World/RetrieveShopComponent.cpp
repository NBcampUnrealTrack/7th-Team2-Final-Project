#include "Components/World/RetrieveShopComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Shop/RetrieveShopDefinitionAsset.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/UnrealType.h"

namespace
{
	/** owner에서 이름이 "InteractionTarget"인 컴포넌트를 찾아 InteractionEnabled를 reflection으로 토글한다.
	 *  상점 UI가 열려있는 동안 이 NPC의 상호작용 프롬프트를 숨기고, 닫히면 복원하는 데 사용된다. */
	void SetInteractionTargetEnabled(AActor* Owner, bool bEnabled)
	{
		if (!Owner)
		{
			return;
		}

		TArray<UActorComponent*> Comps;
		Owner->GetComponents(Comps);
		for (UActorComponent* Comp : Comps)
		{
			if (Comp && Comp->GetFName() == TEXT("InteractionTarget"))
			{
				if (FBoolProperty* EnabledProp =
					FindFProperty<FBoolProperty>(Comp->GetClass(), TEXT("InteractionEnabled")))
				{
					EnabledProp->SetPropertyValue_InContainer(Comp, bEnabled);
				}
				break;
			}
		}
	}
}

URetrieveShopComponent::URetrieveShopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void URetrieveShopComponent::BeginPlay()
{
	Super::BeginPlay();

	RollRotatingStock();

	if (UWorld* World = GetWorld())
	{
		RestListenerHandle = UGameplayMessageSubsystem::Get(World)
			.RegisterListener<FRetrievePlayerRestedPayload>(
				RetrieveGameplayTags::Channel_Player_Rested,
				[WeakThis = TWeakObjectPtr<URetrieveShopComponent>(this)]
				(FGameplayTag, const FRetrievePlayerRestedPayload&)
				{
					if (URetrieveShopComponent* Comp = WeakThis.Get())
						Comp->RollRotatingStock();
				});
	}
}

void URetrieveShopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem::Get(World).UnregisterListener(RestListenerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void URetrieveShopComponent::RollRotatingStock()
{
	CachedRotatingRows.Reset();

	if (!ShopDefinition || !ShopDefinition->RotatingPoolTable || ShopDefinition->RotatingSlotCount <= 0)
		return;

	// RotatingRowFilter가 있으면 그 목록만, 없으면 테이블 전체
	TArray<FName> Pool = ShopDefinition->RotatingRowFilter.Num() > 0
		? ShopDefinition->RotatingRowFilter
		: ShopDefinition->RotatingPoolTable->GetRowNames();

	// Fisher-Yates shuffle
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Pool.Swap(i, j);
	}

	const int32 Count = FMath::Min(ShopDefinition->RotatingSlotCount, Pool.Num());
	CachedRotatingRows.Append(Pool.GetData(), Count);
}

void URetrieveShopComponent::OpenShop(AActor* InstigatorActor)
{
	if (!ShopDefinition)
	{
		UE_LOG(LogTemp, Warning, TEXT("RetrieveShopComponent: ShopDefinition이 비어 있습니다."));
		return;
	}

	APlayerController* PC = nullptr;
	if (APawn* Pawn = Cast<APawn>(InstigatorActor))
	{
		PC = Cast<APlayerController>(Pawn->GetController());
	}

	if (!PC)
	{
		PC = InstigatorActor ? InstigatorActor->GetWorld()->GetFirstPlayerController() : nullptr;
	}

	// 상점 UI가 열려있는 동안 이 NPC의 상호작용 프롬프트를 숨긴다 (닫힐 때 PlayerController가 복원).
	SetInteractionTargetEnabled(GetOwner(), false);

	OnShopOpenRequested.Broadcast(ShopDefinition, PC);
}
