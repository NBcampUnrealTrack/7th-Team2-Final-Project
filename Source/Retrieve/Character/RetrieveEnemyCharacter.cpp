#include "Character/RetrieveEnemyCharacter.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h" 
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/PatternCounterComponent.h"
#include "Components/DropComponent.h"
#include "Components/RetrieveHealthComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DataTable.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Components/SphereComponent.h"
#include "Enemy/EnemyAIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ARetrieveEnemyCharacter::ARetrieveEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OwnedASC = CreateDefaultSubobject<URetrieveAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	OwnedASC->SetIsReplicated(true);
	OwnedASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	EnemyCombatComponent = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("EnemyCombatComponent"));
	PatternCounterComponent = CreateDefaultSubobject<UPatternCounterComponent>(TEXT("PatternCounterComponent"));
	DropComponent = CreateDefaultSubobject<UDropComponent>(TEXT("DropComponent"));
	
	FistHitbox = CreateDefaultSubobject<USphereComponent>(TEXT("FistHitbox"));
	FistHitbox->SetupAttachment(GetMesh());  
	
	FistHitbox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	FistHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FistHitbox->SetSphereRadius(30.f);
	
	// 회전 보간 적용
	bUseControllerRotationYaw = false;
	
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bUseControllerDesiredRotation = true;
	MoveComp->bOrientRotationToMovement = false;
	MoveComp->RotationRate = FRotator(0.f, 360.f, 0.f);
	MoveComp->bUseRVOAvoidance = true;
	
	if (IsValid(OwnedASC))
	{
		OwnedASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Enemy_Dead, 
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ARetrieveEnemyCharacter::OnDeadTagChanged);

		OwnedASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Enemy_Chase,
			EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ARetrieveEnemyCharacter::OnChaseTagChanged);

		OwnedASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Enemy_Hit,
			EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ARetrieveEnemyCharacter::OnHitTagChanged);

		OwnedASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Enemy_Staggered,
			EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ARetrieveEnemyCharacter::OnStaggeredTagChanged);
		
		OwnedASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Enemy_Groggy,
			EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ARetrieveEnemyCharacter::OnGroggyTagChanged);
	}
}

void ARetrieveEnemyCharacter::SetRespawnable(bool NewRespawnable)
{
	bRespawnable = NewRespawnable;
}

void ARetrieveEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (GetMesh())
	{
		InitialMeshRelativeTransform = GetMesh()->GetRelativeTransform();
	}
	
	InitializeAbilitySystem();
	InitializeComponents();
	
	UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(this);
	GroupAlertHandle = MsgSubsys.RegisterListener<FEnemyPlayerSpottedPayload>(
		RetrieveGameplayTags::Channel_Enemy_PlayerSpotted,
		this, &ARetrieveEnemyCharacter::OnAlerted);
	
	if (UCharacterMovementComponent* MoveComp = Cast<UCharacterMovementComponent>(GetMovementComponent()))
	{
		DefaultGravityScale = MoveComp->GravityScale;
		DefaultMovementMode = MoveComp->MovementMode;
	}
}

void ARetrieveEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GroupAlertHandle.IsValid())
	{
		UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(this);
		MsgSubsys.UnregisterListener(GroupAlertHandle);
	}
	
	Super::EndPlay(EndPlayReason);
}

void ARetrieveEnemyCharacter::InitializeAbilitySystem()
{
	if (PawnExtensionComponent && OwnedASC)
	{
		PawnExtensionComponent->InitializeAbilitySystem(OwnedASC, this);
	}
}

void ARetrieveEnemyCharacter::InitializeComponents()
{
	if (!MonsterDataTable || MonsterDataRowName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: MonsterDataTable 또는 MonsterDataRowName이 설정되지 않았습니다."),
			   *GetName());
		return;
	}
	
	const FMonsterDataRow* Row = MonsterDataTable->FindRow<FMonsterDataRow>(
	   MonsterDataRowName, TEXT("ARetrieveEnemyCharacter::InitializeComponents"));
	if (!Row) return;

	if (EnemyCombatComponent)
	{
		EnemyCombatComponent->Initialize(PatternTable, Row->PatternSlots);
		EnemyCombatComponent->SetActiveHitbox(FistHitbox);
	}

	if (DropComponent && !Row->DropRow.IsNone())
	{
		// DropComponent::Initialize는 DropTable도 필요
		DropComponent->Initialize(DropTable, Row->DropRow);
	}
}

void ARetrieveEnemyCharacter::HandleDeathStarted(AActor* OwningActor)
{
    Super::HandleDeathStarted(OwningActor);
	
	if (!HasAuthority())
	{
		return;
	}
	
	if (UWorld* World = GetWorld())
	{
		FMonsterDiedPayload Payload;
		Payload.DeadActor = this;
		Payload.DeathLocation = GetActorLocation();
		// 현재 최종 살해자 정보를 가져올 수단 없음
		Payload.Killer = nullptr;
		
		UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(World);
		MsgSubsys.BroadcastMessage(RetrieveGameplayTags::Channel_Monster_Died, Payload);
	}
	
	FGameplayEventData EventData;
	EventData.EventTag = RetrieveGameplayTags::GameplayEvent_Enemy_Die;
	EventData.Instigator = this;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, RetrieveGameplayTags::GameplayEvent_Enemy_Die, EventData);
}

void ARetrieveEnemyCharacter::OnDeadTagChanged(const FGameplayTag Tag, int32 Count)
{
	bCachedIsDead = Count > 0;
}

void ARetrieveEnemyCharacter::OnChaseTagChanged(const FGameplayTag Tag, int32 Count)
{
	bCachedIsChasing = Count > 0;
}

void ARetrieveEnemyCharacter::OnHitTagChanged(const FGameplayTag Tag, int32 Count)
{
	bCachedIsHit = Count > 0;
}

void ARetrieveEnemyCharacter::OnStaggeredTagChanged(const FGameplayTag Tag, int32 Count)
{
	bCachedIsStaggered = Count > 0;
}

void ARetrieveEnemyCharacter::OnGroggyTagChanged(const FGameplayTag Tag, int32 Count)
{
	bCachedIsGroggy = Count > 0;
}

void ARetrieveEnemyCharacter::HandleDeathEnded(AActor* OwningActor)
{
	if (bRespawnable)
	{
		DeactivateEnemy();
		
		OnDeathEnded.Broadcast(this); // Spawner, Controller에 통보
	}
	else
	{
		Destroy();
	}
}

void ARetrieveEnemyCharacter::ActivateEnemy(const FTransform& SpawnTransform, bool bIsRespawn)
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionProfileName(TEXT("CharacterMesh"));
		MeshComp->AttachToComponent(
			GetCapsuleComponent(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		MeshComp->SetRelativeTransform(InitialMeshRelativeTransform);
	}
	
	SetActorTransform(SpawnTransform);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	
	if (UCharacterMovementComponent* MoveComp = Cast<UCharacterMovementComponent>(GetMovementComponent()))
	{
		GetCharacterMovement()->GravityScale = DefaultGravityScale;
		GetCharacterMovement()->SetMovementMode(DefaultMovementMode);
	}
	
	if (bIsRespawn)
	{
		if (IsValid(HealthComponent))
		{
			HealthComponent->ResetHealth();
		}
	}
	
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		AI->Reactivate();
	}
}

void ARetrieveEnemyCharacter::DeactivateEnemy()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
		
	if (UCharacterMovementComponent* MoveComp = Cast<UCharacterMovementComponent>(GetMovementComponent()))
	{
		MoveComp->GravityScale = 0.0f;
		MoveComp->SetMovementMode(EMovementMode::MOVE_None);
	}
		
	if (AEnemyAIController* AI = Cast<AEnemyAIController>(GetController()))
	{
		AI->Deactivate();
	}
}

void ARetrieveEnemyCharacter::OnAlerted(FGameplayTag Channel, const FEnemyPlayerSpottedPayload& Payload)
{
	if (Payload.InstigatorEnemy == this)
	{
		return;
	}
	
	if (AlertedTarget)
	{
		return;
	}
	
	const float Dist = FVector::Dist(GetActorLocation(), Payload.InstigatorLocation);
	if (Dist <= GroupAlertRadius)
	{
		AlertedTarget = Payload.SpottedActor.Get();
	}
}
