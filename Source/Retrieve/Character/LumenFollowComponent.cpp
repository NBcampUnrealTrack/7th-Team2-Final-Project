#include "Character/LumenFollowComponent.h"

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

ULumenFollowComponent::ULumenFollowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
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
		BindCombatTagWatcher();
		World->GetTimerManager().SetTimer(IdleTimerHandle, this,
		                                  &ULumenFollowComponent::TickIdle, 1.0f, true);
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

	if (BoundCombatASC.IsValid() && CombatTagHandle.IsValid())
	{
		BoundCombatASC->UnregisterGameplayTagEvent(CombatTagHandle, RetrieveGameplayTags::State_Player_Combat,
		                                           EGameplayTagEventType::NewOrRemoved);
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

FVector ULumenFollowComponent::ComputeFollowOffset(const APawn* Host) const
{
	const FVector Forward = Host->GetActorForwardVector();
	const FVector Right = Host->GetActorRightVector();
	return Forward * -OffsetBack + Right * -OffsetLeft;
}

FVector ULumenFollowComponent::ComputeRetreatPosition(const APawn* Host) const
{
	// TODO: EQS
	return Host->GetActorLocation() - Host->GetActorForwardVector() * RetreatRadius;
}

void ULumenFollowComponent::RequestMoveTo(const FVector& Target)
{
	APawn* LumenPawn = Cast<APawn>(GetOwner());
	if (!LumenPawn)
	{
		return;
	}
	if (AAIController* AIController = Cast<AAIController>(LumenPawn->GetController()))
	{
		AIController->MoveToLocation(Target, 50.f, true,
		                             true, true,
		                             false, nullptr, true);
	}
}

void ULumenFollowComponent::StopMove()
{
	APawn* LumenPawn = Cast<APawn>(GetOwner());
	if (!LumenPawn)
	{
		return;
	}
	if (AAIController* AIController = Cast<AAIController>(LumenPawn->GetController()))
	{
		AIController->StopMovement();
	}
}

void ULumenFollowComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	APawn* Host = ResolveHostPawn();
	if (!Host)
	{
		return;
	}

	AActor* LumenActor = GetOwner();
	const float DistanceToHost = FVector::Distance(Host->GetActorLocation(), LumenActor->GetActorLocation());

	// 멈춰 있을 때만 호스트를 바라봄
	auto FaceHost = [&]()
	{
		const float Yaw = (Host->GetActorLocation() - LumenActor->GetActorLocation()).Rotation().Yaw;
		LumenActor->SetActorRotation(FRotator(0.f, Yaw, 0.f));
	};

	if (Mode == EFollowMode::Wait)
	{
		StopMove();
		FaceHost();
		return;
	}

	FVector DesiredTarget;
	if (Mode == EFollowMode::Follow)
	{
		if (TeleportDistance > 0.f && DistanceToHost > TeleportDistance)
		{
			const FVector Target = Host->GetActorLocation() + ComputeFollowOffset(Host);
			LumenActor->SetActorLocation(Target, false);
			LastIssuedTarget = Target;
			FaceHost();
			return;
		}
		if (DistanceToHost < FollowDistance)
		{
			StopMove();
			FaceHost();
			return;
		}
		DesiredTarget = Host->GetActorLocation() + ComputeFollowOffset(Host);
	}
	else
	{
		DesiredTarget = ComputeRetreatPosition(Host);
	}

	const float TargetDrift = FVector::Dist(DesiredTarget, LastIssuedTarget);
	if (TargetDrift > MoveTargetRefreshThreshold)
	{
		RequestMoveTo(DesiredTarget);
		LastIssuedTarget = DesiredTarget;
	}
}

void ULumenFollowComponent::BindCombatTagWatcher()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	APawn* Host = ResolveHostPawn();
	UAbilitySystemComponent* ASC = nullptr;
	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Host))
	{
		ASC = ASI->GetAbilitySystemComponent();
	}

	if (!Host || !ASC)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &ULumenFollowComponent::BindCombatTagWatcher);
		}
		return;
	}

	CombatTagHandle = ASC->RegisterGameplayTagEvent(
		                     RetrieveGameplayTags::State_Player_Combat, EGameplayTagEventType::NewOrRemoved)
	                     .AddUObject(this, &ULumenFollowComponent::OnCombatTagChanged);
	BoundCombatASC = ASC;
}

void ULumenFollowComponent::OnCombatTagChanged(const FGameplayTag /*Tag*/, int32 Count)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (Count > 0)
	{
		PreCombatMode = Mode;
		Mode = EFollowMode::RetreatCombat;
	}
	else
	{
		Mode = (PreCombatMode == EFollowMode::Wait) ? EFollowMode::Wait : EFollowMode::Follow;
	}
	OnRep_Mode();
}

void ULumenFollowComponent::HandleToggleWaitBroadcast(const FRetrieveLumenCommandPayload& /*Payload*/)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	ApplyToggleWait();
}

void ULumenFollowComponent::ApplyToggleWait()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (Mode == EFollowMode::RetreatCombat)
	{
		PreCombatMode = (PreCombatMode == EFollowMode::Wait) ? EFollowMode::Follow : EFollowMode::Wait;
		return;
	}

	Mode = (Mode == EFollowMode::Wait) ? EFollowMode::Follow : EFollowMode::Wait;
	OnRep_Mode();
}

void ULumenFollowComponent::HandleRecallBroadcast(const FRetrieveLumenCommandPayload& /*Payload*/)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (Mode == EFollowMode::RetreatCombat)
		{
			PreCombatMode = EFollowMode::Follow;
		}
		else
		{
			Mode = EFollowMode::Follow;
			OnRep_Mode();
		}

		if (APawn* Host = ResolveHostPawn())
		{
			const FVector Target = Host->GetActorLocation() + ComputeFollowOffset(Host);
			GetOwner()->SetActorLocation(Target, false);
			LastIssuedTarget = Target;
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
	const bool bCanPlayIdle = (Mode == EFollowMode::Wait) || (Mode == EFollowMode::Follow && DistanceToHost <= FollowDistance + 25.f);
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
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Lumen_Mode_Changed, Message);
	}
}
