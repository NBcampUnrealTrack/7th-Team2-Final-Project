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
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// 참고: 상호작용 프롬프트 "시선 게이트"(플레이어가 바라볼 때만 노출)는 루멘 전용이었다가
// 대화 NPC 공통 기능으로 승격되어 RetrieveDialogueComponent(루멘도 보유)로 이동했다.

namespace
{
	// ── 지면 가드 ───────────────────────────────────────────────────────────
	// 게임 시작/리스폰/빠른 이동 직후 루멘 위치의 월드 파티션 셀이 아직 로드되지 않아
	// 루멘이 땅 밑으로 꺼지는 문제 방지. 발밑에 지면이 없으면(미로딩) 이동·중력을 잠그고,
	// 지면이 로드되면 그 위로 스냅한 뒤 이동을 재개한다. 이미 추락했으면 호스트 곁으로 회수.
	// (BP_LumenCharacter에 WorldPartitionStreamingSourceComponent를 추가해 셀 로딩도 유도한다.)
	// 상태를 파일 로컬에 두는 이유: 헤더/레이아웃 변경 없이 Live Coding으로 반영 가능.
	TMap<TWeakObjectPtr<ULumenFollowComponent>, FTimerHandle> GLumenGroundGuardTimers;
	TSet<TWeakObjectPtr<AActor>> GLumenGroundFrozen;

	void EvaluateLumenGroundGuard(ULumenFollowComponent* Comp)
	{
		ACharacter* Lumen = Comp ? Cast<ACharacter>(Comp->GetOwner()) : nullptr;
		UWorld* World = Lumen ? Lumen->GetWorld() : nullptr;
		if (!World || !Lumen->HasAuthority() || Comp->IsRetired())
		{
			return;
		}
		UCharacterMovementComponent* Move = Lumen->GetCharacterMovement();

		APawn* Host = nullptr;
		if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			Host = GS->GetHostPawn();
		}

		// 1) 이미 추락했으면 호스트 곁(뒤-왼쪽 오프셋)으로 회수한다.
		//    (2000은 너무 관대해 지면 아래 1~19m 구간에 갇힌 채 방치될 수 있었다 → 800으로 강화)
		if (Host && Lumen->GetActorLocation().Z < Host->GetActorLocation().Z - 800.f)
		{
			Lumen->SetActorLocation(
				ULumenFollowComponent::ComputeSafeLandingBehindHost(Host, Lumen, Comp->GetOffsetBack(), Comp->GetOffsetLeft()),
				false, nullptr, ETeleportType::TeleportPhysics);
			if (Move)
			{
				Move->StopMovementImmediately();
			}
			UE_LOG(LogTemp, Log, TEXT("[LumenGroundGuard] %s: 호스트보다 800 이상 아래로 추락 → 호스트 곁으로 회수"),
				*Lumen->GetName());
		}

		// 2) 발밑 지면 로드 여부 확인(위 100 ~ 아래 5000 라인 트레이스)
		const FVector Location = Lumen->GetActorLocation();
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LumenGroundGuard), /*bTraceComplex*/ false, Lumen);
		if (Host)
		{
			QueryParams.AddIgnoredActor(Host);
		}
		FHitResult Hit;
		const bool bGroundLoaded = World->LineTraceSingleByChannel(
			Hit,
			Location + FVector(0.f, 0.f, 100.f),
			Location - FVector(0.f, 0.f, 5000.f),
			ECC_Visibility,
			QueryParams);

		const bool bFrozen = GLumenGroundFrozen.Contains(Lumen);
		if (!bGroundLoaded)
		{
			if (!bFrozen && Move)
			{
				// MOVE_None은 중력이 적용되지 않아 지면이 없어도 떨어지지 않는다(빠른 이동과 동일 수법).
				Move->StopMovementImmediately();
				Move->DisableMovement();
				GLumenGroundFrozen.Add(Lumen);
				UE_LOG(LogTemp, Log, TEXT("[LumenGroundGuard] %s: 발밑 지면 미로딩 → 이동/중력 잠금"),
					*Lumen->GetName());
			}

			// 얼어 있는 동안에는 매 틱 호스트 곁에 붙여 둔다.
			// 호스트 주변 셀이 가장 먼저 로드되므로 지면을 가장 빨리 되찾는 위치이고,
			// 이미 지면 아래로 가라앉아 트레이스 시작점(+100)이 지하에 묻히는 바람에
			// 영영 해동되지 못하는 상태도 방지한다(빠른 이동/리스폰 직후의 잔여 낙하 대응).
			if (Host)
			{
				Lumen->SetActorLocation(
					ULumenFollowComponent::ComputeSafeLandingBehindHost(Host, Lumen, Comp->GetOffsetBack(), Comp->GetOffsetLeft()),
					false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
		else if (bFrozen)
		{
			// 지면 로드 완료: 지면 위로 스냅(파묻힘 방지, 위 방향으로만) 후 이동 재개.
			const float HalfHeight = Lumen->GetCapsuleComponent()
				? Lumen->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
			const FVector SnapLocation = Hit.Location + FVector(0.f, 0.f, HalfHeight + 2.f);
			if (Location.Z < SnapLocation.Z)
			{
				Lumen->SetActorLocation(SnapLocation, false, nullptr, ETeleportType::TeleportPhysics);
			}
			if (Move)
			{
				Move->SetMovementMode(MOVE_Walking);
			}
			GLumenGroundFrozen.Remove(Lumen);
			UE_LOG(LogTemp, Log, TEXT("[LumenGroundGuard] %s: 지면 로드 확인 → 지면 위 스냅 + 이동 재개"),
				*Lumen->GetName());
		}
	}
}

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

		// 지면 가드: 월드 파티션 미로딩으로 인한 낙하를 감지·방지한다(권한 측 이동 제어).
		FTimerHandle& GuardHandle = GLumenGroundGuardTimers.FindOrAdd(this);
		World->GetTimerManager().SetTimer(GuardHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				EvaluateLumenGroundGuard(this);
			}),
			0.25f, true);
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

		if (FTimerHandle* GuardHandle = GLumenGroundGuardTimers.Find(this))
		{
			World->GetTimerManager().ClearTimer(*GuardHandle);
			GLumenGroundGuardTimers.Remove(this);
		}
	}
	GLumenGroundFrozen.Remove(GetOwner());

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

FVector ULumenFollowComponent::ComputeSafeLandingBehindHost(const AActor* Host, const ACharacter* LumenCharacter,
	float InOffsetBack, float InOffsetLeft)
{
	if (!Host)
	{
		return LumenCharacter ? LumenCharacter->GetActorLocation() : FVector::ZeroVector;
	}

	FVector Landing = Host->GetActorLocation() + ComputeBehindLeftOffset(Host, InOffsetBack, InOffsetLeft);

	const float HalfHeight = (LumenCharacter && LumenCharacter->GetCapsuleComponent())
		? LumenCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 90.f;

	if (UWorld* World = Host->GetWorld())
	{
		// 착지 XY의 실제 지면에 캡슐을 앉힌다. 호스트 Z를 그대로 쓰면 경사지(오르막)에서
		// 캡슐이 지형에 파묻힌 채 배치되고, 깊이 박히면 밀어내기 한계를 넘어 허리까지 낀다.
		FCollisionQueryParams Query(SCENE_QUERY_STAT(LumenSafeLanding), /*bTraceComplex*/ false);
		Query.AddIgnoredActor(Host);
		if (LumenCharacter)
		{
			Query.AddIgnoredActor(LumenCharacter);
		}

		FHitResult Hit;
		const FVector Start = Landing + FVector(0.f, 0.f, 300.f);
		const FVector End = Landing - FVector(0.f, 0.f, 600.f);
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query)
			&& Hit.ImpactNormal.Z > 0.35f)
		{
			Landing.Z = Hit.ImpactPoint.Z + HalfHeight + 2.f;
			return Landing;
		}
	}

	// 지면을 못 찾으면(미로딩 등) 호스트 높이 + 여유 — 이후는 지면 가드가 처리한다.
	Landing.Z = Host->GetActorLocation().Z + 50.f;
	return Landing;
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

// ---- Retire --------------------------------------------------------------------------------------

void ULumenFollowComponent::SetRetired(bool bInRetired)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bRetired == bInRetired)
	{
		return;
	}
	bRetired = bInRetired;

	if (!bRetired)
	{
		bWaitRequested = false;
	}
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
		if (bRetired)
		{
			return;
		}

		// 소환은 Wait을 해제하여 ST가 Follow로 복귀하도록 함
		bWaitRequested = false;

		// 웅크리는 동안 전투에 끌어들이지 않음. ST가 전투 종료 시 Follow를 복원함.
		if (Mode != EFollowMode::RetreatCombat)
		{
			if (APawn* Host = ResolveHostPawn())
			{
				// 경사지에서 파묻히지 않도록 착지점을 지면에 투영한다.
				const FVector Target = ComputeSafeLandingBehindHost(
					Host, Cast<ACharacter>(GetOwner()), OffsetBack, OffsetLeft);
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
	if (!GetOwner() || !GetOwner()->HasAuthority() || bRetired)
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
