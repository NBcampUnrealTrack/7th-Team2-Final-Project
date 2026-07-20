#include "Character/LumenCharacter.h"

#include "../Lumen/LumenAIController.h"
#include "../Lumen/LumenFollowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/World/RetrieveDialogueComponent.h"
#include "Core/RetrieveGameState.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Quest/QuestBranchComponent.h"

ALumenCharacter::ALumenCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AutoReceiveInput = EAutoReceiveInput::Disabled;

	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->bUseControllerDesiredRotation = false;
		Move->RotationRate = FRotator(0.f, 540.f, 0.f);
		// 몬스터 RVO 회피 계산에서 루멘을 완전히 제외
		Move->bUseRVOAvoidance = false;
	}

	// Pawn 채널 무시 → 적 히트박스 오버랩 이벤트가 발생하지 않음 (공격 대상 식별 차단)
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	// 골렘 메시가 SK_Mannequin_PhysicsAsset(체구가 훨씬 작은 스켈레톤 기준)을 사용해
	// 자동 계산된 바운드가 실제 메시보다 작다. World Partition 스트리밍/컬링 시
	// 이 축소된 바운드로 화면 밖이라 판정돼 Shipping에서 메시 전체가 렌더링되지 않는다.
	// 바운드를 넉넉히 키우고 화면 밖에서도 본을 계속 갱신하도록 강제한다.
	if (USkeletalMeshComponent* LumenMesh = GetMesh())
	{
		LumenMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		LumenMesh->bComponentUseFixedSkelBounds = true;
		LumenMesh->SetBoundsScale(2.0f);
		LumenMesh->MarkRenderStateDirty();
	}

	FollowComponent = CreateDefaultSubobject<ULumenFollowComponent>(TEXT("FollowComponent"));
	DialogueComponent = CreateDefaultSubobject<URetrieveDialogueComponent>(TEXT("DialogueComponent"));
}

void ALumenCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		StepChangedListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveQuestStepPayload>(
			RetrieveGameplayTags::Channel_Quest_StepChanged, this, &ALumenCharacter::HandleQuestStepChanged);
	}
	
	SyncRetiredFromQuestLedger();
}

void ALumenCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (StepChangedListener.IsValid())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(StepChangedListener);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ALumenCharacter::HandleQuestStepChanged(FGameplayTag /*Channel*/, const FRetrieveQuestStepPayload& /*Message*/)
{
	SyncRetiredFromQuestLedger();
}

void ALumenCharacter::SyncRetiredFromQuestLedger()
{
	if (!HasAuthority())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	const UQuestBranchComponent* Quest = GS ? GS->GetQuestBranchComponent() : nullptr;
	if (!Quest)
	{
		return;
	}

	const bool bShouldRetire = Quest->IsStepCompleted(RetrieveGameplayTags::Quest_Step_TalkedToLumen_Castle);
	if (bShouldRetire != bRetired)
	{
		SetRetired(bShouldRetire);
	}
}

void ALumenCharacter::SetRetired(bool bInRetired, const FTransform* ParkXf)
{
	if (!HasAuthority())
	{
		return;
	}
	bRetired = bInRetired;

	if (bInRetired && ParkXf)
	{
		SetActorTransform(*ParkXf, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (ALumenAIController* AI = Cast<ALumenAIController>(GetController()))
	{
		AI->StopMovement();
		AI->SetLogicRunning(!bInRetired);
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (bInRetired)
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}
		else
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}

	if (FollowComponent)
	{
		FollowComponent->SetRetired(bInRetired);
	}
	
	// retire 후에도 새 대사로 상호작용 가능해야하므로 DialogueComponent는 그대로 둔다
}
