#include "Components/World/RetrieveDialogueComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/LumenCharacter.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Core/RetrieveGameState.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "NPC/NPCPatrolAIController.h"
#include "Player/RetrievePlayerController.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	// ── NPC 상호작용 프롬프트 시선 게이트 ─────────────────────────────────────
	// 대화 NPC(루멘 포함)는 플레이어 근처에 상시 존재해 상호작용 존이 계속 겹치고,
	// 등 뒤/화면 밖 대상의 프롬프트가 떠서 거슬리는 문제가 있다.
	// 플레이어 캐릭터가 몸을 돌려 바라보고(폰 전방) 화면에도 보일 때(카메라)만
	// 상호작용 존 콜리전을 켠다. 콜리전을 껐다 켜면 이미 겹쳐 있는 플레이어에게도
	// Begin/EndOverlap이 실제로 발생해 플러그인이 실제 진입/이탈과 동일하게 동작한다.
	// (Enable_Interaction BP 함수 방식은 존을 떠난 적 없는 플레이어에게 재활성이
	//  전달되지 않아 사용하지 않는다. 상태는 Live Coding 반영을 위해 파일 로컬.)
	TMap<TWeakObjectPtr<URetrieveDialogueComponent>, FTimerHandle> GNpcGazeGateTimers;
	TMap<TWeakObjectPtr<AActor>, bool> GNpcInteractionEnabled;

	/** Manager_InteractionTarget(BP)의 스피어 존 오브젝트 프로퍼티(OuterZone/InnerZone)를 읽는다. */
	USphereComponent* GetInteractionZone(UActorComponent* Manager, const TCHAR* PropertyName)
	{
		if (!Manager)
		{
			return nullptr;
		}
		if (const FObjectProperty* Prop = FindFProperty<FObjectProperty>(Manager->GetClass(), PropertyName))
		{
			return Cast<USphereComponent>(Prop->GetObjectPropertyValue_InContainer(Manager));
		}
		return nullptr;
	}

	/** NPC의 상호작용 존 콜리전을 켜고 끈다(중복 호출은 무시). */
	void SetNpcInteractionEnabled(AActor* Npc, const bool bEnable)
	{
		if (!Npc)
		{
			return;
		}
		if (const bool* Prev = GNpcInteractionEnabled.Find(Npc); Prev && *Prev == bEnable)
		{
			return;
		}

		UActorComponent* Manager = nullptr;
		for (UActorComponent* Comp : Npc->GetComponents())
		{
			if (Comp && Comp->GetFName() == TEXT("InteractionTarget"))
			{
				Manager = Comp;
				break;
			}
		}
		USphereComponent* OuterZone = GetInteractionZone(Manager, TEXT("OuterZone"));
		USphereComponent* InnerZone = GetInteractionZone(Manager, TEXT("InnerZone"));
		if (!OuterZone || !InnerZone)
		{
			return; // 존이 아직 생성 전(BeginPlay 직후 등)이면 다음 주기에 다시 시도된다.
		}

		if (bEnable)
		{
			// 진입 순서대로: Outer 먼저 → Inner. (걸어 들어올 때와 같은 이벤트 순서 보장)
			OuterZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			InnerZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
		else
		{
			// 이탈 순서대로: Inner 먼저 → Outer.
			InnerZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			OuterZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		GNpcInteractionEnabled.Add(Npc, bEnable);
	}

	/**
	 * 플레이어가 NPC를 바라보는지 판정해 상호작용을 켜고 끈다(경계 깜빡임 방지 히스테리시스).
	 * 두 조건을 모두 요구한다:
	 *  1) 캐릭터(폰) 전방 기준 — 3인칭 카메라는 캐릭터보다 뒤에 있어서, 대상이 캐릭터 등 뒤로
	 *     오면(루멘 텔레포트 등) 카메라 기준으로는 "정면"이 되는 함정이 있다. 요 회전만 의미
	 *     있으므로 2D로 판정한다.
	 *  2) 카메라 기준 — 화면 밖 대상에 프롬프트가 붙는 것을 막는다.
	 */
	void EvaluateNpcGazeGate(URetrieveDialogueComponent* Comp)
	{
		AActor* Npc = Comp ? Comp->GetOwner() : nullptr;
		UWorld* World = Npc ? Npc->GetWorld() : nullptr;
		if (!World)
		{
			return;
		}
		const APlayerController* PC = World->GetFirstPlayerController();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!PC || !PC->PlayerCameraManager || !Pawn)
		{
			return; // 로컬 화면이 없으면(데디 서버 등) 그대로 둔다.
		}

		// 대화/상점/시네마틱 등 연출 카메라 중(뷰 타깃이 플레이어 폰이 아닐 때)에는
		// 어떤 NPC의 상호작용 프롬프트도 띄우지 않는다. 연출 카메라가 프롬프트를 숨겨도
		// 이 게이트가 0.15초마다 다시 켜던 문제(대화 중 "대화하기" 잔존)의 근본 수정.
		if (PC->GetViewTarget() != Pawn)
		{
			SetNpcInteractionEnabled(Npc, false);
			return;
		}

		const FVector NpcLocation = Npc->GetActorLocation();

		// 1) 캐릭터 전방(요만 고려) 기준
		const FVector PawnForward2D = Pawn->GetActorForwardVector().GetSafeNormal2D();
		const FVector ToNpcFromPawn2D = (NpcLocation - Pawn->GetActorLocation()).GetSafeNormal2D();
		const float PawnDot = FVector::DotProduct(PawnForward2D, ToNpcFromPawn2D);

		// 2) 카메라 기준
		const FVector CamLocation = PC->PlayerCameraManager->GetCameraLocation();
		const FVector CamForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
		const float CamDot = FVector::DotProduct(CamForward, (NpcLocation - CamLocation).GetSafeNormal());

		// 켤 때는 정면 약 63도(cos 0.45) 안, 끌 때는 약 75도(cos 0.25) 밖 — 경계에서 안 떨리게.
		const bool* Prev = GNpcInteractionEnabled.Find(Npc);
		const bool bCurrentlyEnabled = Prev ? *Prev : true;
		const bool bEnable = bCurrentlyEnabled
			? (PawnDot > 0.25f && CamDot > 0.25f)
			: (PawnDot > 0.45f && CamDot > 0.45f);
		SetNpcInteractionEnabled(Npc, bEnable);
	}
}

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

		// 시선 게이트: 플레이어 캐릭터+카메라가 이 NPC를 바라볼 때만 상호작용 프롬프트를 노출한다.
		// UI는 클라이언트 로컬이므로 로컬 화면이 있는 머신에서만 돌린다.
		if (UWorld* World = GetWorld(); World && World->GetNetMode() != NM_DedicatedServer)
		{
			FTimerHandle& GazeHandle = GNpcGazeGateTimers.FindOrAdd(this);
			World->GetTimerManager().SetTimer(GazeHandle,
				FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					EvaluateNpcGazeGate(this);
				}),
				0.15f, true);
		}

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
	if (UWorld* World = GetWorld())
	{
		if (FTimerHandle* GazeHandle = GNpcGazeGateTimers.Find(this))
		{
			World->GetTimerManager().ClearTimer(*GazeHandle);
			GNpcGazeGateTimers.Remove(this);
		}
	}
	GNpcInteractionEnabled.Remove(GetOwner());

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

bool URetrieveDialogueComponent::GrantWeightedReward(const TArray<FRetrieveDialogueItemReward>& Pool, AActor* Instigator) const
{
	float TotalWeight = 0.f;
	for (const FRetrieveDialogueItemReward& Reward : Pool)
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
	for (const FRetrieveDialogueItemReward& Reward : Pool)
	{
		Roll -= FMath::Max(0.f, Reward.Weight);
		if (Roll <= 0.f)
		{
			return Inventory->AddItem(Reward.ItemId, Reward.ItemCategoryTag, Reward.Quantity);
		}
	}
	return false;
}

FGameplayTag URetrieveDialogueComponent::ResolveSpeakerTagForSave() const
{
	if (SpeakerTag.IsValid())
	{
		return SpeakerTag;
	}
	if (const ALumenCharacter* Lumen = Cast<ALumenCharacter>(GetOwner()))
	{
		return Lumen->SpeakerTag;
	}
	return FGameplayTag();
}

int32 URetrieveDialogueComponent::GetRewardGrantCount(bool bRpsBet) const
{
	const FGameplayTag Key = ResolveSpeakerTagForSave();
	if (Key.IsValid())
	{
		if (const UGameInstance* GI = GetOwner() ? GetOwner()->GetGameInstance() : nullptr)
		{
			if (const URetrieveSaveSubsystem* SaveSub = GI->GetSubsystem<URetrieveSaveSubsystem>())
			{
				return SaveSub->GetNpcRewardGrantCount(Key, bRpsBet);
			}
		}
	}
	// SpeakerTag 미설정 NPC 등은 세션 카운터로 폴백(저장은 안 되지만 상한은 지켜진다).
	return bRpsBet ? RpsRewardGrantCount : ItemRewardGrantCount;
}

void URetrieveDialogueComponent::IncrementRewardGrantCount(bool bRpsBet)
{
	// 세션 폴백 카운터는 항상 함께 증가시킨다.
	(bRpsBet ? RpsRewardGrantCount : ItemRewardGrantCount)++;

	const FGameplayTag Key = ResolveSpeakerTagForSave();
	if (Key.IsValid())
	{
		if (UGameInstance* GI = GetOwner() ? GetOwner()->GetGameInstance() : nullptr)
		{
			if (URetrieveSaveSubsystem* SaveSub = GI->GetSubsystem<URetrieveSaveSubsystem>())
			{
				SaveSub->IncrementNpcRewardGrantCount(Key, bRpsBet);
			}
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

	// 대화 횟수와 무관하게 이 NPC가 줄 수 있는 랜덤 보상은 MaxItemRewardGrants회로 고정한다.
	// (지급 횟수는 저장 파일 기준 — 게임을 재시작해도 유지)
	if (MaxItemRewardGrants > 0 && GetRewardGrantCount(false) >= MaxItemRewardGrants)
	{
		return false;
	}

	if (FMath::FRand() > ItemRewardChance)
	{
		return false;
	}

	if (GrantWeightedReward(ItemRewardPool, Instigator))
	{
		IncrementRewardGrantCount(false);
		return true;
	}
	return false;
}

bool URetrieveDialogueComponent::CanOfferRpsBet() const
{
	return bEnableRpsBet
		&& (MaxRpsRewardGrants <= 0 || GetRewardGrantCount(true) < MaxRpsRewardGrants);
}

bool URetrieveDialogueComponent::HandleRpsBetTopic(FGameplayTag TopicId, APawn* PlayerPawn)
{
	using namespace RetrieveGameplayTags;

	const bool bIsStart = TopicId == Dialogue_Bet_Start;
	int32 PlayerPick = INDEX_NONE; // 0=가위 1=바위 2=보
	if (TopicId == Dialogue_Bet_Scissors) { PlayerPick = 0; }
	else if (TopicId == Dialogue_Bet_Rock) { PlayerPick = 1; }
	else if (TopicId == Dialogue_Bet_Paper) { PlayerPick = 2; }

	if (!bIsStart && PlayerPick == INDEX_NONE)
	{
		return false; // 내기 토픽이 아님 → 기존 DT_Dialogue 흐름으로
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	if (!Owner || !Owner->HasAuthority() || !GS)
	{
		return true; // 내기 토픽이지만 처리 불가 — 삼켜서 잘못된 DT 조회를 막는다
	}

	// 내기 대사의 화자명 보장(대화 중이면 이미 설정돼 있지만 안전하게 재설정).
	GS->SetActiveSpeaker(SpeakerDisplayName);

	static const TCHAR* PickNames[] = { TEXT("가위"), TEXT("바위"), TEXT("보") };
	const auto MakeChoiceTopics = []()
	{
		TArray<FRetrieveDialogueTopic> Choices;
		Choices.Add(FRetrieveDialogueTopic{ Dialogue_Bet_Scissors, FText::FromString(TEXT("가위")), true, ETopicKind::Story });
		Choices.Add(FRetrieveDialogueTopic{ Dialogue_Bet_Rock,     FText::FromString(TEXT("바위")), true, ETopicKind::Story });
		Choices.Add(FRetrieveDialogueTopic{ Dialogue_Bet_Paper,    FText::FromString(TEXT("보")),   true, ETopicKind::Story });
		return Choices;
	};

	if (bIsStart)
	{
		RpsWinStreak = 0;
		if (!CanOfferRpsBet())
		{
			GS->RequestDialogue({ FText::FromString(TEXT("미안하지만 자네에게 줄 수 있는 건 이미 다 줬다네. 내기는 그만하지.")) }, {}, true);
			return true;
		}
		GS->RequestDialogue({ FText::FromString(TEXT("좋지! 가위바위보 세 판을 연속으로 이기면 좋은 걸 주겠네. 자, 무엇을 내겠나?")) },
			MakeChoiceTopics(), true);
		return true;
	}

	// 판정: (플레이어 - NPC + 3) % 3 → 0=무승부, 1=플레이어 승, 2=NPC 승 (가위0<바위1<보2 순환)
	const int32 NpcPick = FMath::RandRange(0, 2);
	const int32 Outcome = (PlayerPick - NpcPick + 3) % 3;

	if (Outcome == 0) // 무승부 — 연승 유지, 다시
	{
		GS->RequestDialogue({ FText::FromString(FString::Printf(
			TEXT("나도 %s! 비겼으니 다시 가세. (현재 %d연승)"), PickNames[NpcPick], RpsWinStreak)) },
			MakeChoiceTopics(), true);
		return true;
	}

	if (Outcome == 2) // 패배 — 연승 리셋, 재도전 가능
	{
		RpsWinStreak = 0;
		GS->RequestDialogue({ FText::FromString(FString::Printf(
			TEXT("나는 %s! 이번엔 내가 이겼군. 연승이 끊겼으니 처음부터일세!"), PickNames[NpcPick])) },
			MakeChoiceTopics(), true);
		return true;
	}

	// 승리
	++RpsWinStreak;
	if (RpsWinStreak < 3)
	{
		GS->RequestDialogue({ FText::FromString(FString::Printf(
			TEXT("나는 %s… 자네가 이겼군! (%d연승) 계속 가 보게."), PickNames[NpcPick], RpsWinStreak)) },
			MakeChoiceTopics(), true);
		return true;
	}

	// 3연승 — 보상 지급(최대 MaxRpsRewardGrants회, 저장 파일 기준 영속)
	RpsWinStreak = 0;
	IncrementRewardGrantCount(true);
	GrantWeightedReward(RpsRewardPool, PlayerPawn);
	const bool bCanBetAgain = CanOfferRpsBet();
	GS->RequestDialogue({
			FText::FromString(FString::Printf(TEXT("나는 %s… 세상에, 3연승이라니!"), PickNames[NpcPick])),
			FText::FromString(bCanBetAgain
				? TEXT("약속대로 보상을 받게. 실력이 좋으니 한 번 더 도전해도 좋네!")
				: TEXT("약속대로 보상을 받게. 이걸로 내가 줄 수 있는 건 마지막일세!"))
		},
		bCanBetAgain ? MakeChoiceTopics() : TArray<FRetrieveDialogueTopic>(), true);
	return true;
}

void URetrieveDialogueComponent::OpenConversationFor(AActor* Instigator)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	// 내기 연승은 대화 세션 단위 — 대화를 새로 시작하면 리셋한다.
	RpsWinStreak = 0;

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

	const FText ResolvedSpeaker = SpeakerDisplayName;

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
