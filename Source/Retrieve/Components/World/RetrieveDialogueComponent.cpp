#include "Components/World/RetrieveDialogueComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimInstance.h"
#include "Character/LumenCharacter.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Core/RetrieveGameState.h"
#include "NPC/NPCPatrolAIController.h"
#include "Player/RetrievePlayerController.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

URetrieveDialogueComponent::URetrieveDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveDialogueComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		// SkeletalMeshComponent 캐시 + 유휴 애니메이션 자동 재생
		CachedMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
		if (CachedMesh && IdleAnimation)
		{
			CachedMesh->PlayAnimation(IdleAnimation, true);
		}

		// InteractionTarget FinishMethod를 ReactivateAfterCompleted로 런타임 강제 설정
		ConfigureInteractionTarget();

		if (!bAutoBindResponseComponent || bBoundToResponseComponent)
		{
			return;
		}
		if (URetrieveInteractionResponseComponent* ResponseComponent =
			Owner->FindComponentByClass<URetrieveInteractionResponseComponent>())
		{
			ResponseComponent->OnApplied.AddDynamic(this, &URetrieveDialogueComponent::HandleInteract);
			bBoundToResponseComponent = true;
		}
	}
}

void URetrieveDialogueComponent::ConfigureInteractionTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// BonfireActor::ConfigurePersistentInteractionTarget과 동일한 방식
	// PersistentFinishMethodValue = 3 (ReactivateAfterCompleted)
	static constexpr uint8 PersistentFinishMethodValue = 3;

	TArray<UActorComponent*> Components;
	Owner->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (!Comp || Comp->GetFName() != FName(TEXT("InteractionTarget"))) continue;

		UClass* CompClass = Comp->GetClass();
		bool bFinishSet = false;

		if (FByteProperty* ByteProp = FindFProperty<FByteProperty>(CompClass, FName(TEXT("FinishMethod"))))
		{
			ByteProp->SetPropertyValue_InContainer(Comp, PersistentFinishMethodValue);
			bFinishSet = true;
		}
		else if (FEnumProperty* EnumProp = FindFProperty<FEnumProperty>(CompClass, FName(TEXT("FinishMethod"))))
		{
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
				EnumProp->ContainerPtrToValuePtr<void>(Comp),
				static_cast<int64>(PersistentFinishMethodValue));
			bFinishSet = true;
		}

		if (FFloatProperty* FloatProp = FindFProperty<FFloatProperty>(CompClass, FName(TEXT("ReactivationDuration"))))
		{
			FloatProp->SetPropertyValue_InContainer(Comp, 0.0f);
		}

		UE_LOG(LogTemp, Log,
			TEXT("[DialogueComp] %s: InteractionTarget FinishMethod=%d(%s), ReactivationDuration=0"),
			*Owner->GetName(),
			PersistentFinishMethodValue,
			bFinishSet ? TEXT("설정 완료") : TEXT("프로퍼티 없음"));
		break;
	}
}

void URetrieveDialogueComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bBoundToResponseComponent)
	{
		if (AActor* Owner = GetOwner())
		{
			if (URetrieveInteractionResponseComponent* Response =
				Owner->FindComponentByClass<URetrieveInteractionResponseComponent>())
			{
				Response->OnApplied.RemoveDynamic(this, &URetrieveDialogueComponent::HandleInteract);
			}
		}
		bBoundToResponseComponent = false;
	}
	Super::EndPlay(EndPlayReason);
}

void URetrieveDialogueComponent::HandleInteract(AActor* Instigator)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	OpenConversationFor(Instigator);
}

void URetrieveDialogueComponent::PlayGreetingAnimation()
{
	if (!CachedMesh) return;

	// 몽타주는 AnimBP Slot으로 재생되어 상태머신(Idle/Run 블렌드)을 그대로 두므로,
	// 순찰 중 걸음이 끊기지 않는 Villager 같은 NPC에 우선 사용한다.
	if (GreetingMontage)
	{
		if (UAnimInstance* AnimInstance = CachedMesh->GetAnimInstance())
		{
			AnimInstance->Montage_Play(GreetingMontage);
			return;
		}
	}

	UAnimSequenceBase* AnimToPlay = TalkingAnimation ? TalkingAnimation.Get() : IdleAnimation.Get();
	if (AnimToPlay)
	{
		CachedMesh->PlayAnimation(AnimToPlay, true);
	}
}

void URetrieveDialogueComponent::PlayTopicAnimation(FGameplayTag TopicId)
{
	if (!CachedMesh) return;
	for (const FNPCDialogueAnimEntry& Entry : TopicAnimations)
	{
		if (Entry.TopicId != TopicId)
		{
			continue;
		}

		if (Entry.Montage)
		{
			if (UAnimInstance* AnimInstance = CachedMesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(Entry.Montage);
			}
			return;
		}

		if (Entry.Animation)
		{
			CachedMesh->PlayAnimation(Entry.Animation, false);
			if (Entry.bAutoReturnToTalking)
			{
				if (UWorld* World = GetWorld())
				{
					World->GetTimerManager().ClearTimer(AnimReturnTimerHandle);
					World->GetTimerManager().SetTimer(
						AnimReturnTimerHandle,
						FTimerDelegate::CreateUObject(this, &URetrieveDialogueComponent::PlayGreetingAnimation),
						Entry.ReturnDelay,
						false);
				}
			}
		}
		return;
	}
}

void URetrieveDialogueComponent::ReturnToIdle()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AnimReturnTimerHandle);
	}
	if (CachedMesh && IdleAnimation)
	{
		CachedMesh->PlayAnimation(IdleAnimation, true);
	}

	// 순찰 중이던 NPC라면 대화 종료 후 순찰 재개
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (ANPCPatrolAIController* PatrolAIC = Cast<ANPCPatrolAIController>(OwnerPawn->GetController()))
		{
			PatrolAIC->Reactivate();
		}
	}
}

bool URetrieveDialogueComponent::TryGrantItemReward(AActor* Instigator)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if (ItemRewardPool.Num() == 0 || ItemRewardChance <= 0.f)
	{
		return false;
	}

	if (FMath::FRand() > ItemRewardChance)
	{
		return false;
	}

	float TotalWeight = 0.f;
	for (const FRetrieveDialogueItemReward& Reward : ItemRewardPool)
	{
		TotalWeight += FMath::Max(0.f, Reward.Weight);
	}

	if (TotalWeight <= 0.f)
	{
		return false;
	}

	UInventoryComponent* Inventory = Instigator ? Instigator->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return false;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (const FRetrieveDialogueItemReward& Reward : ItemRewardPool)
	{
		Roll -= FMath::Max(0.f, Reward.Weight);
		if (Roll <= 0.f)
		{
			return Inventory->AddItem(Reward.ItemId, Reward.ItemCategoryTag, Reward.Quantity);
		}
	}

	return false;
}

void URetrieveDialogueComponent::OpenConversationFor(AActor* Instigator)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	APawn* InstigatorPawn = Cast<APawn>(Instigator);
	if (!InstigatorPawn)
	{
		return;
	}

	// 순찰 중인 NPC라면 대화 중 걸어다니지 않도록 정지
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (ANPCPatrolAIController* PatrolAIC = Cast<ANPCPatrolAIController>(OwnerPawn->GetController()))
		{
			PatrolAIC->Deactivate();
		}
	}

	// TODO: 일반화하기. Cast를 IRetrieverConversationSpeaker로 교체할것.
	FText ResolvedSpeaker = SpeakerDisplayName;
	if (const ALumenCharacter* Lumen = Cast<ALumenCharacter>(GetOwner()))
	{
		if (!Lumen->DisplayName.IsEmpty())
		{
			ResolvedSpeaker = Lumen->DisplayName;
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			GS->SetActiveSpeaker(ResolvedSpeaker);
		}
	}

	if (ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(InstigatorPawn->GetController()))
	{
		PC->Client_OpenConversation(GetOwner());
	}
}
