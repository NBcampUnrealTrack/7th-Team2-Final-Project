#include "World/GuardianCoreSpawnerComponent.h"

#include "Character/RetrieveBossCharacter.h"
#include "Core/RetrieveGameState.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Quest/QuestBranchComponent.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "World/GuardianCoreActor.h"
#include "EngineUtils.h"

UGuardianCoreSpawnerComponent::UGuardianCoreSpawnerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void UGuardianCoreSpawnerComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	MonsterDiedHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FMonsterDiedPayload>(
		RetrieveGameplayTags::Channel_Monster_Died,
		[WeakThis = TWeakObjectPtr<UGuardianCoreSpawnerComponent>(this)]
	(FGameplayTag Channel, const FMonsterDiedPayload& Message)
		{
			if (UGuardianCoreSpawnerComponent* Self = WeakThis.Get())
			{
				Self->HandleMonsterDied(Channel, Message);
			}
		});

	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSub = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSub->OnWorldObjectStatesChanged.AddUniqueDynamic(
				this, &UGuardianCoreSpawnerComponent::HandleSaveLoaded);
			HandleSaveLoaded();
		}
	}
}

void UGuardianCoreSpawnerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MonsterDiedHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(MonsterDiedHandle);
		}
		MonsterDiedHandle = FGameplayMessageListenerHandle();
	}
	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (URetrieveSaveSubsystem* SaveSub = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSub->OnWorldObjectStatesChanged.RemoveDynamic(
				this, &UGuardianCoreSpawnerComponent::HandleSaveLoaded);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UGuardianCoreSpawnerComponent::HandleMonsterDied(FGameplayTag Channel, const FMonsterDiedPayload& Message)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return; // 스폰 권한은 호스트 전용
	}

	// TODO: 페이로드에 원소 태그 넣기. BossCharacter 의존성 제거.
	const ARetrieveBossCharacter* Boss = Cast<ARetrieveBossCharacter>(Message.DeadActor.Get());
	if (!Boss)
	{
		return; // 가디언 보스만 처리하도록 필터링
	}

	const FGameplayTag Element = Boss->GetUnlockElementTag();
	if (!Element.IsValid())
	{
		return; // 여왕 / 비가디언 보스 → 코어 없음
	}

	// 죽은 가디언의 원소와 CDO ElementTag가 일치하는 코어 클래스 찾기
	TSubclassOf<AGuardianCoreActor> ChosenClass = nullptr;
	for (const TSubclassOf<AGuardianCoreActor>& CoreClass : GuardianCoreClasses)
	{
		if (!CoreClass)
		{
			continue;
		}
		const AGuardianCoreActor* CDO = CoreClass->GetDefaultObject<AGuardianCoreActor>();
		if (CDO && CDO->GetElementTag().MatchesTagExact(Element))
		{
			ChosenClass = CoreClass;
			break;
		}
	}

	if (!ChosenClass)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[GuardianCoreSpawner] GuardianCoreClasses에 원소 '%s'와 매칭되는 코어 클래스 없음"),
		       *Element.ToString());
		return;
	}

	// 중복 방지: 이 가디언의 단계가 이미 완료된 경우 코어는 이미 소비됨, 재스폰 X
	if (const ARetrieveGameState* GS = Cast<ARetrieveGameState>(Owner))
	{
		if (const UQuestBranchComponent* Quest = GS->GetQuestBranchComponent())
		{
			const FGameplayTag StepTag = ChosenClass->GetDefaultObject<AGuardianCoreActor>()->GetGuardianDefeatedStep();
			if (StepTag.IsValid() && Quest->IsStepCompleted(StepTag))
			{
				return;
			}
		}
	}

	FVector SpawnLocation = Message.DeathLocation;
	SpawnLocation.Z += SpawnZOffset;
	const FTransform CoreTransform(FRotator::ZeroRotator, SpawnLocation);

	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (URetrieveSaveSubsystem* SaveSub = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSub->SetPendingGuardianCore(Element, CoreTransform);
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	GetWorld()->SpawnActor<AGuardianCoreActor>(
		ChosenClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
}

void UGuardianCoreSpawnerComponent::HandleSaveLoaded()
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	URetrieveSaveSubsystem* SaveSub =
		GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	if (!Owner || !Owner->HasAuthority() || !World || !SaveSub) { return; }

	for (const TSubclassOf<AGuardianCoreActor>& CoreClass : GuardianCoreClasses)
	{
		if (!CoreClass) { continue; }

		const AGuardianCoreActor* CDO = CoreClass->GetDefaultObject<AGuardianCoreActor>();
		if (!CDO) { continue; }

		const FGameplayTag Element = CDO->GetElementTag();
		if (!Element.IsValid()) { continue; }

		for (TActorIterator<AGuardianCoreActor> It(World); It; ++It)
		{
			AGuardianCoreActor* ExistingCore = *It;
			if (IsValid(ExistingCore)
				&& ExistingCore->GetElementTag().MatchesTagExact(Element))
			{
				ExistingCore->Destroy();
			}
		}

		FTransform CoreTransform;
		if (!SaveSub->TryGetPendingGuardianCore(Element, CoreTransform))
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		World->SpawnActor<AGuardianCoreActor>(
			CoreClass, CoreTransform, SpawnParams);
	}
}
