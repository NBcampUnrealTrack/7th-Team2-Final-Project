#include "World/RetrieveQuestEncounter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/World/RetrieveDialogueComponent.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/RetrieveBonfireActor.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"
#include "Quest/RetrieveQuestDefinition.h"
#include "Quest/RetrieveQuestLinkComponent.h"
#include "Quest/RetrieveQuestObjectives.h"
#include "Save/RetrieveSaveGame.h"
#include "Save/RetrieveSaveSubsystem.h"

ARetrieveQuestEncounter::ARetrieveQuestEncounter()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ARetrieveQuestEncounter::BeginPlay()
{
	Super::BeginPlay();

	BuildFromDefinition();

	Phase = GetDefaultPhase();

	ResolveAndSpawnActors();
	ApplyNPCAppearance(PendingAppearanceIndex);

	if (AActor* NPC = GetDialogueNPC())
	{
		if (URetrieveDialogueComponent* Dialogue = NPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			Dialogue->OnDialogueClosed.AddUniqueDynamic(this, &ThisClass::HandleDialogueClosed);

			// SpeakerTag를 비워 상점/기타 데이터 토픽(구매·판매 등)이 붙지 않게 한다.
			// 퀘스트 NPC는 단계별 DefaultGreetingLines만 노출한다.
			Dialogue->SpeakerTag = FGameplayTag();
			if (!PendingNPCDisplayName.IsEmpty())
			{
				Dialogue->SpeakerDisplayName = PendingNPCDisplayName;
			}
		}
	}

	for (URetrieveQuestObjective* Objective : Objectives)
	{
		if (Objective)
		{
			Objective->OnObjectiveChanged.AddUObject(this, &ThisClass::HandleObjectiveChanged);
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSubsystem = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSubsystem->OnLoadCompleted.AddUniqueDynamic(this, &ThisClass::HandleSaveLoaded);
		}
	}

	RestoreSavedState();
	ApplyPhase();

	// NPC(RetrieveNPCCharacter)의 BeginPlay가 메시를 AnimBP로 초기화하면서 인스턴스의
	// 아이들 애니(기도)를 덮어쓸 수 있다(BeginPlay 순서 경합). 잠시 후 한 번 더 적용해
	// PIE에서 idle로 되돌아가는 것을 방지한다.
	if (PendingIdleMontage)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle ReapplyHandle;
			World->GetTimerManager().SetTimer(
				ReapplyHandle, this, &ARetrieveQuestEncounter::ApplyNPCIdleMontage, 0.5f, false);
		}
	}
}

void ARetrieveQuestEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActivateObjectives(false);

	if (AActor* NPC = GetDialogueNPC())
	{
		if (URetrieveDialogueComponent* Dialogue = NPC->FindComponentByClass<URetrieveDialogueComponent>())
		{
			Dialogue->OnDialogueClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
		}
	}

	for (URetrieveQuestObjective* Objective : Objectives)
	{
		if (Objective)
		{
			Objective->OnObjectiveChanged.RemoveAll(this);
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSubsystem = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSubsystem->OnLoadCompleted.RemoveDynamic(this, &ThisClass::HandleSaveLoaded);
		}
	}

	DestroySpawnedActors();

	Super::EndPlay(EndPlayReason);
}

void ARetrieveQuestEncounter::ResolveAndSpawnActors()
{
	RuntimeActors.Reset();
	ManuallyLinkedRoles.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 1) 인스턴스 오버라이드 먼저 등록(레벨에 직접 배치한 액터 사용).
	for (const TPair<FName, TObjectPtr<AActor>>& Pair : LinkedActorOverrides)
	{
		if (!Pair.Key.IsNone() && IsValid(Pair.Value))
		{
			RuntimeActors.Add(Pair.Key, Pair.Value);
			ManuallyLinkedRoles.Add(Pair.Key);
		}
	}

	// 2) QuestLinkComponent 태그로 지정한 배치 액터 등록(수동 배치).
	RegisterManuallyLinkedActors();

	// 3) 자동 스폰이 꺼져 있으면 여기서 끝(오직 배치 액터만 사용).
	if (bDisableAutoSpawn)
	{
		return;
	}

	// 4) 오버라이드/태그로 채워지지 않은 Role만 SpawnRequests로 스폰.
	for (const FRetrieveQuestSpawnRequest& Request : SpawnRequests)
	{
		if (Request.Role.IsNone() || RuntimeActors.Contains(Request.Role) || !Request.ActorClass)
		{
			continue;
		}

		// NPC와 퀘스트 물건은 절대 자동 생성하지 않는다(항상 레벨에 수동 배치).
		// 자동 스폰 시 메시가 랜덤이고 위치 조정이 어려워 중복/불편을 유발하므로,
		// 이 둘은 QuestLinkComponent 태그(또는 LinkedActorOverrides)로만 연결한다.
		// 목적지 마커(보이지 않음)·해방 상인 등 나머지는 자동 스폰을 유지한다.
		if (Request.Role == DialogueNPCRole || Request.Role.ToString().StartsWith(TEXT("QuestItem_")))
		{
			continue;
		}

		// 기준 트랜스폼: 모닥불 우선(해방 상인) 또는 인카운터.
		FTransform BaseTransform = GetActorTransform();
		if (Request.bAtNearestBonfire)
		{
			if (const AActor* Bonfire = FindNearestBonfire())
			{
				BaseTransform = Bonfire->GetActorTransform();
			}
		}
		const FTransform SpawnTransform =
			FTransform(Request.RelativeRotation, Request.RelativeLocation) * BaseTransform;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (AActor* Spawned = World->SpawnActor<AActor>(Request.ActorClass, SpawnTransform, SpawnParams))
		{
			if (Request.bRandomizeItemMesh && QuestItemMeshPool.Num() > 0)
			{
				if (UStaticMeshComponent* MeshComp = Spawned->FindComponentByClass<UStaticMeshComponent>())
				{
					const int32 PickIndex = FMath::RandRange(0, QuestItemMeshPool.Num() - 1);
					if (UStaticMesh* PickedMesh = QuestItemMeshPool[PickIndex].LoadSynchronous())
					{
						MeshComp->SetStaticMesh(PickedMesh);
					}
				}
			}
			if (Request.bSnapToGround)
			{
				SnapActorToGround(Spawned);
			}
			RuntimeActors.Add(Request.Role, Spawned);
			SpawnedActors.Add(Spawned);
		}
	}
}

void ARetrieveQuestEncounter::RegisterManuallyLinkedActors()
{
	UWorld* World = GetWorld();
	if (!World || EncounterId.IsNone())
	{
		// EncounterId가 없으면 태그를 매칭할 수 없다(태그 컴포넌트가 이 값으로 조인).
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == this)
		{
			continue;
		}

		const URetrieveQuestLinkComponent* Link =
			Candidate->FindComponentByClass<URetrieveQuestLinkComponent>();
		if (!Link || Link->TargetEncounterId != EncounterId)
		{
			continue;
		}

		const FName RoleName = Link->ResolveRoleName(DialogueNPCRole);
		if (RoleName.IsNone() || RuntimeActors.Contains(RoleName))
		{
			// 이미 오버라이드/앞선 태그가 채운 Role은 건너뛴다.
			continue;
		}

		RuntimeActors.Add(RoleName, Candidate);
		ManuallyLinkedRoles.Add(RoleName);
	}
}

void ARetrieveQuestEncounter::SnapActorToGround(AActor* Actor)
{
	UWorld* World = GetWorld();
	if (!IsValid(Actor) || !World)
	{
		return;
	}

	const FVector Origin = Actor->GetActorLocation();
	const FVector TraceStart = Origin + FVector(0.f, 0.f, 500.f);
	const FVector TraceEnd = Origin - FVector(0.f, 0.f, 5000.f);

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(QuestGroundSnap), false, this);
	TraceParams.AddIgnoredActor(Actor);
	for (AActor* Existing : SpawnedActors)
	{
		TraceParams.AddIgnoredActor(Existing);
	}

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
	{
		return;
	}

	// 액터 원점에서 바닥까지의 거리(캐릭터=캡슐 반높이, 그 외=바운드 하단)를 계산해
	// 발/바닥이 지면에 딱 닿도록 배치한다.
	float BottomOffset = 0.f;
	if (const ACharacter* Character = Cast<ACharacter>(Actor))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			BottomOffset = Capsule->GetScaledCapsuleHalfHeight();
		}
	}
	else
	{
		// 시각 스태틱 메시의 바닥을 기준으로 삼는다.
		// (상호작용 감지 콜리전 같은 큰 논비주얼 콜리전은 제외해 공중에 뜨는 것을 방지)
		bool bFoundMesh = false;
		float LowestMeshZ = Origin.Z;

		TArray<UStaticMeshComponent*> MeshComponents;
		Actor->GetComponents<UStaticMeshComponent>(MeshComponents);
		for (const UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			if (!MeshComponent || !MeshComponent->GetStaticMesh())
			{
				continue;
			}
			const FBoxSphereBounds ComponentBounds = MeshComponent->Bounds;
			const float ComponentBottomZ = ComponentBounds.Origin.Z - ComponentBounds.BoxExtent.Z;
			if (!bFoundMesh || ComponentBottomZ < LowestMeshZ)
			{
				LowestMeshZ = ComponentBottomZ;
				bFoundMesh = true;
			}
		}

		BottomOffset = bFoundMesh ? (Origin.Z - LowestMeshZ) : 0.f;
	}

	FVector NewLocation = Origin;
	NewLocation.Z = Hit.ImpactPoint.Z + BottomOffset;
	Actor->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
}

void ARetrieveQuestEncounter::DestroySpawnedActors()
{
	for (AActor* Actor : SpawnedActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	SpawnedActors.Reset();
}

void ARetrieveQuestEncounter::BuildFromDefinition()
{
	if (QuestDefId.IsNone() || !QuestDefTable)
	{
		return;
	}

	const FRetrieveQuestDefRow* Row =
		QuestDefTable->FindRow<FRetrieveQuestDefRow>(QuestDefId, TEXT("RetrieveQuestEncounter"));
	if (!Row)
	{
		return;
	}

	QuestTitle = Row->Title;
	bRequiresOffer = Row->bRequiresOffer;
	OfferDialogueLines = Row->OfferDialogueLines;
	InProgressDialogueLines = Row->InProgressDialogueLines;
	ReadyToTurnInDialogueLines = Row->ReadyToTurnInDialogueLines;
	CompletedDialogueLines = Row->CompletedDialogueLines;
	GoldReward = Row->GoldReward;
	ItemRewards = Row->ItemRewards;
	PendingAppearanceIndex = Row->AppearanceIndex;
	PendingNPCDisplayName = Row->NPCDisplayName;

	Objectives.Reset();
	SpawnRequests.Reset();

	// 아이들 몽타주(기도 등) 저장.
	PendingIdleMontage = Row->NPCIdleMontage.LoadSynchronous();
	PendingIdleMontagePhases = Row->NPCIdleMontagePhases;

	// NPC 스폰 요청.
	if (QuestNPCClass)
	{
		FRetrieveQuestSpawnRequest NpcReq;
		NpcReq.Role = DialogueNPCRole;
		NpcReq.ActorClass = QuestNPCClass;
		NpcReq.RelativeLocation = Row->NPCSpawnOffset;
		NpcReq.RelativeRotation = Row->NPCSpawnRotation;
		// 완료 시 숨김이면 완료 전 단계에서만 표시.
		if (Row->bHideNPCOnComplete)
		{
			NpcReq.VisiblePhases = {
				ERetrieveQuestPhase::Offered,
				ERetrieveQuestPhase::InProgress,
				ERetrieveQuestPhase::ReadyToTurnIn };
		}
		SpawnRequests.Add(NpcReq);
	}

	// 완료 시 해방할 상인(상점) 스폰 — Completed 단계에만 노출.
	if (UClass* MerchantClass = Row->MerchantClass.LoadSynchronous())
	{
		FRetrieveQuestSpawnRequest MerchantReq;
		MerchantReq.Role = TEXT("Merchant");
		MerchantReq.ActorClass = MerchantClass;
		MerchantReq.RelativeLocation = Row->MerchantSpawnOffset;
		MerchantReq.VisiblePhases = { ERetrieveQuestPhase::Completed };
		MerchantReq.bAtNearestBonfire = Row->bMerchantAtNearestBonfire;
		SpawnRequests.Add(MerchantReq);
	}

	int32 AutoIndex = 0;
	for (const FRetrieveQuestObjectiveDef& Def : Row->Objectives)
	{
		URetrieveQuestObjective* NewObjective = nullptr;

		switch (Def.Kind)
		{
		case EQuestObjectiveKind::ClearSpawnGroup:
		{
			UQuestObjective_ClearSpawnGroup* O = NewObject<UQuestObjective_ClearSpawnGroup>(this);
			// 인스턴스 오버라이드가 있으면 우선(스포너 태그만 맞추면 연동).
			O->SpawnGroupId = SpawnGroupIdOverride.IsValid() ? SpawnGroupIdOverride : Def.SpawnGroupId;
			NewObjective = O;
			break;
		}
		case EQuestObjectiveKind::AcquireItem:
		{
			UQuestObjective_AcquireItem* O = NewObject<UQuestObjective_AcquireItem>(this);
			O->ItemId = Def.ItemId;
			O->ItemCategoryTag = Def.ItemCategoryTag;
			O->RequiredCount = Def.RequiredCount;
			O->bConsumeOnTurnIn = Def.bConsumeOnTurnIn;
			NewObjective = O;
			break;
		}
		case EQuestObjectiveKind::CollectWorldItem:
		{
			const FName ItemRole = *FString::Printf(TEXT("QuestItem_%d"), AutoIndex);
			UQuestObjective_CollectWorldItem* O = NewObject<UQuestObjective_CollectWorldItem>(this);
			O->ItemActorRole = ItemRole;
			NewObjective = O;

			if (QuestItemClass)
			{
				FRetrieveQuestSpawnRequest ItemReq;
				ItemReq.Role = ItemRole;
				ItemReq.ActorClass = QuestItemClass;
				ItemReq.RelativeLocation = Def.TargetSpawnOffset;
				ItemReq.VisiblePhases = { ERetrieveQuestPhase::InProgress };
				ItemReq.bRandomizeItemMesh = true;
				SpawnRequests.Add(ItemReq);
			}
			break;
		}
		case EQuestObjectiveKind::ReachLocation:
		{
			const FName DestRole = *FString::Printf(TEXT("Destination_%d"), AutoIndex);
			UQuestObjective_ReachLocation* O = NewObject<UQuestObjective_ReachLocation>(this);
			O->DestinationRole = DestRole;
			O->AcceptanceRadius = Def.AcceptanceRadius;
			NewObjective = O;

			// 오프셋 지점에 보이지 않는 마커(빈 AActor)를 스폰해 목적지로 사용.
			// 오프셋이 0이면 마커도 인카운터 위치가 되어 인카운터 지점 도달이 목표가 된다.
			FRetrieveQuestSpawnRequest DestReq;
			DestReq.Role = DestRole;
			DestReq.ActorClass = AActor::StaticClass();
			DestReq.RelativeLocation = Def.TargetSpawnOffset;
			SpawnRequests.Add(DestReq);
			break;
		}
		}

		if (NewObjective)
		{
			NewObjective->Description = Def.Description;
			NewObjective->CompletionToastMessage = Def.CompletionToastMessage;
			Objectives.Add(NewObjective);
		}
		++AutoIndex;
	}
}

void ARetrieveQuestEncounter::ApplyNPCAppearance(int32 AppearanceIndex)
{
	if (AppearanceIndex < 0 || !AppearancePool)
	{
		return;
	}

	// 수동 배치한 NPC는 사용자가 고른 메시를 유지한다(외형 풀로 덮어쓰지 않음).
	if (ManuallyLinkedRoles.Contains(DialogueNPCRole))
	{
		return;
	}

	const FRetrieveQuestNPCAppearance* Appearance = AppearancePool->Get(AppearanceIndex);
	if (!Appearance)
	{
		return;
	}

	ACharacter* NPCCharacter = Cast<ACharacter>(GetDialogueNPC());
	USkeletalMeshComponent* Mesh = NPCCharacter ? NPCCharacter->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	if (USkeletalMesh* SkeletalMesh = Appearance->SkeletalMesh.LoadSynchronous())
	{
		Mesh->SetSkeletalMeshAsset(SkeletalMesh);
	}

	for (int32 SlotIndex = 0; SlotIndex < Appearance->OverrideMaterials.Num(); ++SlotIndex)
	{
		if (UMaterialInterface* Material = Appearance->OverrideMaterials[SlotIndex].LoadSynchronous())
		{
			Mesh->SetMaterial(SlotIndex, Material);
		}
	}

	// 메시를 런타임 교체하면 AnimBP 인스턴스가 리셋되어 T포즈로 멈춘다.
	// 외형별 AnimBP가 있으면 그것으로, 없으면 기존 AnimBP를 강제 재초기화한다.
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	if (!Appearance->AnimClass.IsNull())
	{
		if (UClass* AnimInstanceClass = Appearance->AnimClass.LoadSynchronous())
		{
			Mesh->SetAnimInstanceClass(AnimInstanceClass);
		}
	}
	else
	{
		Mesh->InitAnim(true);
	}
}

AActor* ARetrieveQuestEncounter::GetLinkedActor(FName InRole) const
{
	return RuntimeActors.FindRef(InRole);
}

AActor* ARetrieveQuestEncounter::FindNearestBonfire() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FVector Origin = GetActorLocation();
	float BestDistanceSquared = TNumericLimits<float>::Max();
	ARetrieveBonfireActor* BestBonfire = nullptr;

	for (TActorIterator<ARetrieveBonfireActor> It(World); It; ++It)
	{
		const float DistanceSquared = FVector::DistSquared(Origin, It->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestBonfire = *It;
		}
	}

	return BestBonfire;
}

AActor* ARetrieveQuestEncounter::GetDialogueNPC() const
{
	return GetLinkedActor(DialogueNPCRole);
}

ERetrieveQuestPhase ARetrieveQuestEncounter::GetDefaultPhase() const
{
	return bRequiresOffer ? ERetrieveQuestPhase::Offered : ERetrieveQuestPhase::InProgress;
}

void ARetrieveQuestEncounter::HandleDialogueClosed(AActor* PlayerActor)
{
	if (!HasAuthority() || !IsValid(PlayerActor))
	{
		return;
	}

	switch (Phase)
	{
	case ERetrieveQuestPhase::Offered:
		SetPhase(ERetrieveQuestPhase::InProgress, true);
		break;

	case ERetrieveQuestPhase::ReadyToTurnIn:
		if (!CanTurnInAll(PlayerActor))
		{
			return;
		}
		for (URetrieveQuestObjective* Objective : Objectives)
		{
			if (Objective)
			{
				Objective->OnTurnIn(PlayerActor);
			}
		}
		GrantRewards(PlayerActor);
		SetPhase(ERetrieveQuestPhase::Completed, true);
		break;

	default:
		break;
	}
}

void ARetrieveQuestEncounter::HandleObjectiveChanged()
{
	if (Phase != ERetrieveQuestPhase::InProgress)
	{
		return;
	}

	if (AreAllObjectivesComplete())
	{
		SetPhase(ERetrieveQuestPhase::ReadyToTurnIn, true);
	}
	else
	{
		// 다중 목표에서 일부만 완료된 진행 상태도 영속화한다.
		PersistState();
	}
}

bool ARetrieveQuestEncounter::AreAllObjectivesComplete() const
{
	bool bAny = false;
	for (const URetrieveQuestObjective* Objective : Objectives)
	{
		if (!Objective)
		{
			continue;
		}
		bAny = true;
		if (!Objective->IsComplete())
		{
			return false;
		}
	}
	return bAny;
}

bool ARetrieveQuestEncounter::CanTurnInAll(AActor* Player) const
{
	for (const URetrieveQuestObjective* Objective : Objectives)
	{
		if (Objective && !Objective->CanTurnIn(Player))
		{
			return false;
		}
	}
	return true;
}

void ARetrieveQuestEncounter::SetPhase(ERetrieveQuestPhase NewPhase, bool bPersist)
{
	if (Phase == NewPhase)
	{
		return;
	}

	Phase = NewPhase;
	ApplyPhase();
	if (bPersist)
	{
		PersistState();
	}
}

void ARetrieveQuestEncounter::ApplyPhase()
{
	ActivateObjectives(Phase == ERetrieveQuestPhase::InProgress);
	UpdateDialogueLines();
	ApplyActorVisibility();
	ApplyNPCIdleMontage();

	OnPhaseChanged.Broadcast(Phase);
	ReceiveQuestPhaseChanged(Phase);
}

void ARetrieveQuestEncounter::ApplyNPCIdleMontage()
{
	if (!PendingIdleMontage)
	{
		return;
	}

	ACharacter* NPCCharacter = Cast<ACharacter>(GetDialogueNPC());
	USkeletalMeshComponent* Mesh = NPCCharacter ? NPCCharacter->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	const bool bShouldPlay = PendingIdleMontagePhases.Contains(Phase);

	if (bShouldPlay)
	{
		// 몽타주 슬롯이 NPC의 AnimBP(예: ABP_ShopNPC)에서 평가되지 않으면 Montage_Play를 해도
		// 아무것도 보이지 않는다. 그래서 몽타주의 첫 애니메이션 시퀀스를 꺼내 싱글 노드 모드로
		// 직접 루프 재생한다(구출 포로 BP_RescueNPC_Start_01과 동일한 방식, 슬롯 무관하게 항상 표시).
		UAnimSequenceBase* PoseSeq = nullptr;
		if (PendingIdleMontage->SlotAnimTracks.Num() > 0)
		{
			const FAnimTrack& Track = PendingIdleMontage->SlotAnimTracks[0].AnimTrack;
			if (Track.AnimSegments.Num() > 0)
			{
				PoseSeq = Track.AnimSegments[0].GetAnimReference();
			}
		}
		if (PoseSeq)
		{
			Mesh->PlayAnimation(PoseSeq, /*bLooping=*/true);
		}
		else
		{
			// 몽타주에서 시퀀스를 못 꺼내면, 인스턴스에 설정된 싱글 노드 애니(AnimationData)를
			// 그대로 재생하도록 모드만 되돌린다.
			Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Mesh->Play(true);
		}
	}
	else
	{
		// 원래 AnimBP 구동으로 복귀(예: 처치 완료 후 포로가 일어섬).
		if (Mesh->GetAnimationMode() == EAnimationMode::AnimationSingleNode)
		{
			Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Mesh->InitAnim(true);
		}
	}
}

void ARetrieveQuestEncounter::ActivateObjectives(bool bActivate)
{
	if (bActivate == bObjectivesActive)
	{
		return;
	}
	bObjectivesActive = bActivate;

	for (URetrieveQuestObjective* Objective : Objectives)
	{
		if (!Objective)
		{
			continue;
		}
		if (bActivate)
		{
			Objective->ActivateObjective(this);
		}
		else
		{
			Objective->DeactivateObjective();
		}
	}
}

void ARetrieveQuestEncounter::UpdateDialogueLines()
{
	AActor* NPC = GetDialogueNPC();
	URetrieveDialogueComponent* Dialogue = NPC ? NPC->FindComponentByClass<URetrieveDialogueComponent>() : nullptr;
	if (!Dialogue)
	{
		return;
	}

	switch (Phase)
	{
	case ERetrieveQuestPhase::Offered:
		Dialogue->DefaultGreetingLines = OfferDialogueLines;
		break;
	case ERetrieveQuestPhase::InProgress:
		Dialogue->DefaultGreetingLines = InProgressDialogueLines;
		break;
	case ERetrieveQuestPhase::ReadyToTurnIn:
		Dialogue->DefaultGreetingLines = ReadyToTurnInDialogueLines;
		break;
	case ERetrieveQuestPhase::Completed:
		Dialogue->DefaultGreetingLines = CompletedDialogueLines;
		break;
	}
}

void ARetrieveQuestEncounter::ApplyActorVisibility()
{
	for (const FRetrieveQuestSpawnRequest& Request : SpawnRequests)
	{
		AActor* Actor = GetLinkedActor(Request.Role);
		if (!IsValid(Actor))
		{
			continue;
		}

		// VisiblePhases가 비어 있으면 항상 표시.
		const bool bVisible = Request.VisiblePhases.Num() == 0 || Request.VisiblePhases.Contains(Phase);
		Actor->SetActorHiddenInGame(!bVisible);
		Actor->SetActorEnableCollision(bVisible);
		Actor->SetActorTickEnabled(bVisible);
	}
}

bool ARetrieveQuestEncounter::GrantRewards(AActor* Player)
{
	UInventoryComponent* Inventory = Player ? Player->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return false;
	}

	if (GoldReward > 0)
	{
		Inventory->AddCurrency(GoldReward);
	}
	for (const FRetrieveQuestItemReward& Reward : ItemRewards)
	{
		if (!Reward.ItemId.IsNone() && Reward.Quantity > 0)
		{
			Inventory->AddItem(Reward.ItemId, Reward.ItemCategoryTag, Reward.Quantity);
		}
	}
	return true;
}

void ARetrieveQuestEncounter::ResetForNewGame()
{
	Phase = GetDefaultPhase();
	for (URetrieveQuestObjective* Objective : Objectives)
	{
		if (Objective)
		{
			Objective->RestoreProgress(TArray<uint8>());
		}
	}
	ApplyPhase();
}

void ARetrieveQuestEncounter::HandleSaveLoaded()
{
	Phase = GetDefaultPhase();
	ActivateObjectives(false);
	RestoreSavedState();
	ApplyPhase();
}

void ARetrieveQuestEncounter::RestoreSavedState()
{
	if (EncounterId.IsNone())
	{
		return;
	}

	const UGameInstance* GI = GetGameInstance();
	const URetrieveSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	const URetrieveSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	const FRetrieveQuestSaveData* Saved = SaveGame ? SaveGame->QuestEncounters.Find(EncounterId) : nullptr;
	if (!Saved)
	{
		return;
	}

	Phase = static_cast<ERetrieveQuestPhase>(Saved->Phase);

	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		if (Objectives[Index] && Saved->Objectives.IsValidIndex(Index))
		{
			Objectives[Index]->RestoreProgress(Saved->Objectives[Index].Bytes);
		}
	}
}

void ARetrieveQuestEncounter::PersistState()
{
	if (EncounterId.IsNone())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	URetrieveSaveSubsystem* SaveSubsystem = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	URetrieveSaveGame* SaveGame = SaveSubsystem ? SaveSubsystem->GetCurrentSaveGame() : nullptr;
	if (!SaveSubsystem || !SaveGame)
	{
		return;
	}

	FRetrieveQuestSaveData& Data = SaveGame->QuestEncounters.FindOrAdd(EncounterId);
	Data.Phase = static_cast<uint8>(Phase);
	Data.Objectives.Reset();
	Data.Objectives.Reserve(Objectives.Num());
	for (const URetrieveQuestObjective* Objective : Objectives)
	{
		FRetrieveQuestObjectiveSaveData& ObjData = Data.Objectives.AddDefaulted_GetRef();
		if (Objective)
		{
			Objective->SerializeProgress(ObjData.Bytes);
		}
	}

	SaveSubsystem->FlushWorldState();
}
