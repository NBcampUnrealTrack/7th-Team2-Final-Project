#include "Character/RetrieveEnemyCharacter.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Enemy/EnemyAIController.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/Inventory/DropComponent.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/Enemy/NormalMonsterHealthBarComponent.h"
#include "Components/Enemy/PatternCounterComponent.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Components/Enemy/BossHPBarComponent.h"
#include "Components/Combat/HitReactionComponent.h"
#include "Combat/RetrieveHitReactionProfile.h"
#include "Player/RetrievePlayerController.h"

ARetrieveEnemyCharacter::ARetrieveEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OwnedASC = CreateDefaultSubobject<URetrieveAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	OwnedASC->SetIsReplicated(true);
	OwnedASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	EnemyCombatComponent = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("EnemyCombatComponent"));
	PatternCounterComponent = CreateDefaultSubobject<UPatternCounterComponent>(TEXT("PatternCounterComponent"));
	DropComponent = CreateDefaultSubobject<UDropComponent>(TEXT("DropComponent"));
	NormalHealthBarComponent = CreateDefaultSubobject<UNormalMonsterHealthBarComponent>(TEXT("NormalHealthBarComponent"));
	NormalHealthBarComponent->SetupAttachment(GetRootComponent());
	NormalHealthBarComponent->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	HitReactionComponent = CreateDefaultSubobject<UHitReactionComponent>(TEXT("HitReactionComponent"));
	
	FistHitbox = CreateDefaultSubobject<USphereComponent>(TEXT("FistHitbox"));
	FistHitbox->SetupAttachment(GetMesh());  
	
	FistHitbox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	FistHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FistHitbox->SetSphereRadius(30.f);
	
	// 카메라 충돌 방지
	FistHitbox->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	// GetCapsuleComponent()->SetCollisionObjectType(ECC_GameTraceChannel1);
	// GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	
	// 회전 보간 적용
	bUseControllerRotationYaw = false;
	
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bUseControllerDesiredRotation = false;
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.f, 360.f, 0.f);
	
	// Avoidance
	MoveComp->bUseRVOAvoidance = true;
	MoveComp->AvoidanceConsiderationRadius = 200.0f;
	MoveComp->AvoidanceWeight = 0.5f;
	
	if (IsValid(OwnedASC))
	{
		OwnedASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Enemy_Dead, 
			EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ARetrieveEnemyCharacter::OnDeadTagChanged);

		OwnedASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Enemy_Chase,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ARetrieveEnemyCharacter::OnChaseTagChanged);
		
		OwnedASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Enemy_SpecialAttack,
			EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ARetrieveEnemyCharacter::OnSpecialAttackTagChanged);
		
		OwnedASC->RegisterGameplayTagEvent(RetrieveGameplayTags::State_Enemy_Attack,
			EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ARetrieveEnemyCharacter::OnAttackTagChanged);

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

const FMonsterDataRow* ARetrieveEnemyCharacter::GetMonsterDataRow() const
{
	const FMonsterDataRow* Row = MonsterDataTable->FindRow<FMonsterDataRow>(
	   MonsterDataRowName, TEXT("ARetrieveEnemyCharacter::InitializeComponents"));
	
	return Row;
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
		BaseMaxWalkSpeed = MoveComp->MaxWalkSpeed;
	}
	
	if (OwnedASC)
	{
		OwnedASC->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetMoveSpeedAttribute())
			.AddUObject(this, &ARetrieveEnemyCharacter::OnMoveSpeedChanged);
	}
}

void ARetrieveEnemyCharacter::OnMoveSpeedChanged(const FOnAttributeChangeData& /*Data*/)
{
	RefreshMoveSpeedFromAttribute();
}

void ARetrieveEnemyCharacter::RefreshMoveSpeedFromAttribute()
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (EnemyCombatComponent && EnemyCombatComponent->IsMovementLockedByAttack())
		{
			MoveComp->MaxWalkSpeed = 0.f;
			return;
		}

		float CurrentMoveSpeed = UCombatAttributeSet::ReferenceMoveSpeed;
		if (OwnedASC)
		{
			CurrentMoveSpeed = OwnedASC->GetNumericAttribute(UCombatAttributeSet::GetMoveSpeedAttribute());
		}

		const float Ratio = CurrentMoveSpeed / UCombatAttributeSet::ReferenceMoveSpeed;
		MoveComp->MaxWalkSpeed = BaseMaxWalkSpeed * Ratio;
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
	if (!Row)
	{
		return;
	}
	
	if (EnemyCombatComponent)
	{
		EnemyCombatComponent->Initialize(PatternTable, Row->PatternSlots);
		EnemyCombatComponent->SetActiveHitbox(FistHitbox);
	}
	
	if (PatternCounterComponent)
	{
		PatternCounterComponent->SetGroggyCooldown(Row->GroggyCooldown);
	}

	if (HitReactionComponent && HitReactionProfile)
	{
		HitReactionComponent->Configure(HitReactionProfile);
	}

	if (DropComponent && Row->DropRows.Num() > 0)
	{
		// DropComponent::Initialize는 DropTable도 필요
		DropComponent->Initialize(DropTable, Row->DropRows);
	}

	// 몬스터 이름·등급을 체력바 위젯에 연동
	// DisplayName: 에디터 설정값 우선, 없으면 DataRow 키 사용
	// TypeTag:     에디터에서 컴포넌트에 직접 설정한 값 우선, 없으면 DataTable 값 사용
	if (NormalHealthBarComponent)
	{
		const FText DisplayName = NormalHealthBarComponent->GetMonsterDisplayName().IsEmpty()
			? FText::FromName(MonsterDataRowName)
			: NormalHealthBarComponent->GetMonsterDisplayName();

		const FGameplayTag EditorTag = NormalHealthBarComponent->GetMonsterTypeTag();
		const FGameplayTag TypeTag = EditorTag.IsValid() ? EditorTag : Row->MonsterType;

		NormalHealthBarComponent->SetMonsterIdentity(DisplayName, TypeTag);
	}
}

void ARetrieveEnemyCharacter::HandleDeathStarted(AActor* OwningActor)
{
    Super::HandleDeathStarted(OwningActor);
	
	if (UAbilitySystemComponent* ASC = OwnedASC)
	{
		ASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Enemy_Dead);
	}
	
	if (!HasAuthority())
	{
		return;
	}
	
	const URetrieveHealthComponent* HC = GetHealthComponent();
	
	FMonsterDiedPayload Payload;
	Payload.DeadActor       = this;
	Payload.DeathLocation   = GetActorLocation();
	Payload.Killer          = HC ? HC->LastDamageInstigator.Get() : nullptr;
	Payload.DamageCauser    = HC ? HC->LastDamageCauser.Get() : nullptr;
	Payload.MonsterDataRow  = MonsterDataRowName;

	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		RetrieveGameplayTags::Channel_Monster_Died, 
		Payload
	);
	
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

void ARetrieveEnemyCharacter::OnAttackTagChanged(const FGameplayTag Tag, int32 Count)
{
	bCachedIsAttacking =  Count > 0;
}

void ARetrieveEnemyCharacter::OnSpecialAttackTagChanged(const FGameplayTag Tag, int32 Count)
{
	bCachedIsSpecialAttacking =  Count > 0;
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
		
		MeshComp->bPauseAnims = false;
		MeshComp->SetComponentTickEnabled(true);
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

void ARetrieveEnemyCharacter::SetAerialMode(bool bAerial)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	if (bAerial)
	{
		MoveComp->SetMovementMode(MOVE_Flying);
		MoveComp->GravityScale = 0.f;
	}
	else
	{
		MoveComp->GravityScale = DefaultGravityScale;
		MoveComp->SetMovementMode(MOVE_Falling);
	}
}

void ARetrieveEnemyCharacter::DeactivateEnemy()
{
	if (UBossHPBarComponent* BossHPBar = FindComponentByClass<UBossHPBarComponent>())
	{
		BossHPBar->Hide();
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
		
	if (UCharacterMovementComponent* MoveComp = Cast<UCharacterMovementComponent>(GetMovementComponent()))
	{
		MoveComp->GravityScale = 0.0f;
		MoveComp->SetMovementMode(EMovementMode::MOVE_None);
	}
	
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = true;
		MeshComp->SetComponentTickEnabled(false);
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
