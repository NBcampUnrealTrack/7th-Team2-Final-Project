#include "Components/World/RetrieveDialogueComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Character/LumenCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Core/RetrieveGameState.h"
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
		if (Entry.TopicId == TopicId && Entry.Animation)
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
			return;
		}
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
