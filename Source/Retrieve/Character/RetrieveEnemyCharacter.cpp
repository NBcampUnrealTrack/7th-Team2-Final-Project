#include "Character/RetrieveEnemyCharacter.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h" 
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/PatternCounterComponent.h"
#include "Components/DropComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DataTable.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Components/SphereComponent.h"

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
}

void ARetrieveEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();
	InitializeComponents();
	
	UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(this);
	GroupAlertHandle = MsgSubsys.RegisterListener<FEnemyPlayerSpottedPayload>(
		RetrieveGameplayTags::Channel_Enemy_PlayerSpotted,
		this, &ARetrieveEnemyCharacter::OnAlerted);
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
		UE_LOG(LogDataTable, Display, TEXT("[%s] EnemyCombatComponent initialized."), *GetName());
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
