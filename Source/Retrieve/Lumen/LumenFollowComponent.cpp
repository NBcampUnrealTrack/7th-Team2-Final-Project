#include "Lumen/LumenFollowComponent.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/RetrieveGameState.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Net/UnrealNetwork.h"
#include "Player/RetrievePlayerController.h"
#include "TimerManager.h"
#include "EnvironmentQuery/EnvQueryManager.h"

ULumenFollowComponent::ULumenFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void ULumenFollowComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ULumenFollowComponent, Mode);
}

void ULumenFollowComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);

	ToggleWaitHandle = MessageSubsystem.RegisterListener<FRetrieveLumenCommandPayload>(
		RetrieveGameplayTags::Channel_Lumen_Command_ToggleWait,
		[WeakThis = TWeakObjectPtr<ULumenFollowComponent>(this)]
	(FGameplayTag, const FRetrieveLumenCommandPayload& Message)
		{
			if (ULumenFollowComponent* Comp = WeakThis.Get())
			{
				Comp->HandleToggleWaitBroadcast(Message);
			}
		});

	RecallHandle = MessageSubsystem.RegisterListener<FRetrieveLumenCommandPayload>(
		RetrieveGameplayTags::Channel_Lumen_Command_Recall,
		[WeakThis = TWeakObjectPtr<ULumenFollowComponent>(this)]
	(FGameplayTag, const FRetrieveLumenCommandPayload& Message)
		{
			if (ULumenFollowComponent* Comp = WeakThis.Get())
			{
				Comp->HandleRecallBroadcast(Message);
			}
		});

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		World->GetTimerManager().SetTimer(IdleTimerHandle, this, &ULumenFollowComponent::TickIdle, 1.0f, true);
	}
}

void ULumenFollowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
		if (ToggleWaitHandle.IsValid())
		{
			MessageSubsystem.UnregisterListener(ToggleWaitHandle);
		}
		if (RecallHandle.IsValid())
		{
			MessageSubsystem.UnregisterListener(RecallHandle);
		}
		World->GetTimerManager().ClearTimer(IdleTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

APawn* ULumenFollowComponent::ResolveHostPawn() const
{
	if (UWorld* World = GetWorld())
	{
		if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			return GS->GetHostPawn();
		}
	}
	return nullptr;
}

FVector ULumenFollowComponent::ComputeBehindLeftOffset(const AActor* Host, float InOffsetBack, float InOffsetLeft)
{
	if (!Host)
	{
		return FVector::ZeroVector;
	}
	const FVector Forward = Host->GetActorForwardVector();
	const FVector Right = Host->GetActorRightVector();
	return Forward * -InOffsetBack + Right * -InOffsetLeft;
}

// ---- State Tree write-through --------------------------------------------------------------------

void ULumenFollowComponent::SetModeFromStateTree(EFollowMode NewMode)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (Mode == NewMode)
	{
		return;
	}
	Mode = NewMode;
	OnRep_Mode();
}

// ---- Command ------------------------------------------------------------------------------------

void ULumenFollowComponent::HandleToggleWaitBroadcast(const FRetrieveLumenCommandPayload& /*Payload*/)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ApplyToggleWait();
	}
}

void ULumenFollowComponent::ApplyToggleWait()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	bWaitRequested = !bWaitRequested;
}

void ULumenFollowComponent::HandleRecallBroadcast(const FRetrieveLumenCommandPayload& /*Payload*/)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// 소환은 Wait을 해제하여 ST가 Follow로 복귀하도록 함
		bWaitRequested = false;

		// 웅크리는 동안 전투에 끌어들이지 않음. ST가 전투 종료 시 Follow를 복원함.
		if (Mode != EFollowMode::RetreatCombat)
		{
			if (APawn* Host = ResolveHostPawn())
			{
				const FVector Target = Host->GetActorLocation()
					+ ComputeBehindLeftOffset(Host, OffsetBack, OffsetLeft);
				GetOwner()->SetActorLocation(Target, false);

				const float Yaw = (Host->GetActorLocation() - Target).Rotation().Yaw;
				GetOwner()->SetActorRotation(FRotator(0.f, Yaw, 0.f));
			}
		}
	}
	else
	{
		if (ARetrievePlayerController* PC = GetWorld()->GetFirstPlayerController<ARetrievePlayerController>())
		{
			PC->Server_RequestRecallLumen();
		}
	}
}

// ---- Idle Action ----------------------------------------------------------------------------

void ULumenFollowComponent::TickIdle()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (IdleAction != EIdleMicroAction::None || Mode == EFollowMode::RetreatCombat)
	{
		return;
	}
	if (IdleMontages.Num() == 0)
	{
		return;
	}

	const APawn* Host = ResolveHostPawn();
	if (!Host)
	{
		return;
	}

	const float DistanceToHost = FVector::Distance(Host->GetActorLocation(), GetOwner()->GetActorLocation());
	const bool bCanPlayIdle = (Mode == EFollowMode::Wait) || (Mode == EFollowMode::Follow && DistanceToHost <=
		FollowDistance + 25.f);
	if (!bCanPlayIdle)
	{
		return;
	}

	if (GetLocalHostIdleSeconds() < IdleTriggerSeconds)
	{
		return;
	}

	const int32 MontageIndex = FMath::RandRange(0, IdleMontages.Num() - 1);
	IdleAction = static_cast<EIdleMicroAction>(FMath::Min(MontageIndex + 1, 3));
	MulticastPlayIdleMontage(MontageIndex);
}

void ULumenFollowComponent::MulticastPlayIdleMontage_Implementation(int32 Index)
{
	if (!IdleMontages.IsValidIndex(Index))
	{
		return;
	}
	UAnimMontage* Montage = IdleMontages[Index];
	if (!Montage)
	{
		return;
	}

	ACharacter* Lumen = Cast<ACharacter>(GetOwner());
	if (!Lumen || !Lumen->GetMesh())
	{
		return;
	}
	UAnimInstance* AnimInstance = Lumen->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	AnimInstance->Montage_Play(Montage);
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &ULumenFollowComponent::OnIdleMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
}

void ULumenFollowComponent::OnIdleMontageEnded(UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		IdleAction = EIdleMicroAction::None;
	}
}

float ULumenFollowComponent::GetLocalHostIdleSeconds() const
{
	if (const APawn* Host = ResolveHostPawn())
	{
		if (const ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(Host->GetController()))
		{
			return PC->GetSecondsSinceLastInput();
		}
	}
	return 0.f;
}

void ULumenFollowComponent::OnRep_Mode()
{
	if (UWorld* World = GetWorld())
	{
		FRetrieveLumenModePayload Message;
		Message.Mode = Mode;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Lumen_Mode_Changed,
		                                                       Message);
	}
}


// ---- EQS 안전지대 ----------------------------------------------------------------------------

void ULumenFollowComponent::RequestSafeSpotQuery()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !SafeSpotQuery)
	{
		return;
	}
	bSafeSpotValid = false;
	FEnvQueryRequest Request(SafeSpotQuery, GetOwner());
	Request.Execute(EEnvQueryRunMode::SingleResult, this, &ULumenFollowComponent::OnSafeSpotQueryFinished);
}

void ULumenFollowComponent::OnSafeSpotQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	if (Result.IsValid() && Result->IsSuccessful() && Result->Items.Num() > 0)
	{
		SafeSpot = Result->GetItemAsLocation(0);
		bSafeSpotValid = true;
	}
}
