#include "Character/RetrieveEnemyCharacter.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Enemy/EnemyAIController.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "AIController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
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

void ARetrieveEnemyCharacter::AlertFromDamageInstigator(AActor* DamageInstigator)
{
	if (!HasAuthority() || !IsValid(DamageInstigator) || DamageInstigator == this)
	{
		return;
	}

	AActor* AlertTarget = DamageInstigator;
	if (AController* ControllerInstigator = Cast<AController>(AlertTarget))
	{
		AlertTarget = ControllerInstigator->GetPawn();
	}
	else if (APawn* PawnInstigator = Cast<APawn>(AlertTarget))
	{
		if (AController* PawnController = PawnInstigator->GetController())
		{
			AlertTarget = PawnController->GetPawn();
		}
	}
	else if (APawn* OwnerInstigator = Cast<APawn>(AlertTarget->GetOwner()))
	{
		AlertTarget = OwnerInstigator;
	}
	else if (APawn* ActorInstigator = AlertTarget->GetInstigator())
	{
		AlertTarget = ActorInstigator;
	}

	if (!IsValid(AlertTarget) || AlertTarget == this)
	{
		return;
	}

	if (const URetrieveHealthComponent* Health = GetHealthComponent())
	{
		if (Health->IsDeadOrDying())
		{
			return;
		}
	}

	if (const AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (AIController->GetTeamAttitudeTowards(*AlertTarget) != ETeamAttitude::Hostile)
		{
			return;
		}
	}

	AlertedTarget = AlertTarget;

	FEnemyPlayerSpottedPayload Payload;
	Payload.SpottedActor = AlertTarget;
	Payload.SpottedLocation = AlertTarget->GetActorLocation();
	Payload.InstigatorLocation = GetActorLocation();
	Payload.InstigatorEnemy = this;

	UGameplayMessageSubsystem::Get(this).BroadcastMessage(
		RetrieveGameplayTags::Channel_Enemy_PlayerSpotted,
		Payload);
}

const FMonsterDataRow* ARetrieveEnemyCharacter::GetMonsterDataRow() const
{
	if (!MonsterDataTable || MonsterDataRowName.IsNone())
	{
		return nullptr;
	}
	return MonsterDataTable->FindRow<FMonsterDataRow>(
		MonsterDataRowName, TEXT("ARetrieveEnemyCharacter::GetMonsterDataRow"));
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
		DefaultMovementMode = (MoveComp->MovementMode == MOVE_Falling || MoveComp->MovementMode == MOVE_Flying || MoveComp->MovementMode == MOVE_None)
			? MOVE_Walking
			: MoveComp->MovementMode.GetValue();
		BaseMaxWalkSpeed = MoveComp->MaxWalkSpeed;

		if (HasAerialPhase() && MoveComp->MovementMode == MOVE_Falling)
		{
			MoveComp->StopMovementImmediately();
			MoveComp->SetMovementMode(DefaultMovementMode);
		}
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

	GetWorldTimerManager().ClearTimer(AlertStaggerTimer);
	
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

	// 넉백 면역: 보스(MonsterType) 자동 + 데이터 플래그(에픽/일반). DoKnockback이 이 태그를 체크해 스킵.
	if (OwnedASC && (Row->MonsterType.MatchesTagExact(RetrieveGameplayTags::Monster_Type_Boss) || Row->bKnockbackImmune))
	{
		OwnedASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Immune_Knockback);
	}


	ConfigureEnemyMovement();

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
	ResetAerialSpecialPhase();

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

	// 에픽 전용: 스폰 위치가 네비메시(지면)보다 높이 떠 있으면 지면으로 스냅한다.
	// 드래곤처럼 비행 진입용으로 공중에 배치/스폰된 경우, 지상 전투 시 네비메시 밖이라
	// 경로탐색이 전부 실패해 제자리에 멈추는 문제를 방지한다. (일반/보스는 기본 false → 무영향)
	if (ShouldGroundSnapOnSpawn())
	{
		if (UWorld* SnapWorld = GetWorld())
		{
			if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(SnapWorld))
			{
				FNavLocation NavLoc;
				const FVector QueryExtent(800.f, 800.f, 4000.f);
				if (NavSys->ProjectPointToNavigation(GetActorLocation(), NavLoc, QueryExtent))
				{
					const float HalfHeight = GetCapsuleComponent()
						? GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
						: 0.f;
					const FVector GroundedLocation(NavLoc.Location.X, NavLoc.Location.Y, NavLoc.Location.Z + HalfHeight);
					if (FMath::Abs(GetActorLocation().Z - GroundedLocation.Z) > 20.f)
					{
						SetActorLocation(GroundedLocation, false, nullptr, ETeleportType::TeleportPhysics);
						UE_LOG(LogTemp, Warning,
							TEXT("[ActivateEnemy] Ground-snapped %s to navmesh. NavZ=%.1f NewActorZ=%.1f"),
							*GetName(), NavLoc.Location.Z, GroundedLocation.Z);
					}
				}
			}

			if (UCharacterMovementComponent* SnapMoveComp = GetCharacterMovement())
			{
				SnapMoveComp->SetMovementMode(MOVE_Walking);
			}
		}
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
		StopLocomotionMontages();
		MoveComp->SetMovementMode(MOVE_Flying);
		MoveComp->GravityScale = 0.f;
	}
	else
	{
		MoveComp->StopMovementImmediately();
		MoveComp->GravityScale = DefaultGravityScale;
		if (MoveComp->MovementMode == MOVE_Flying)
		{
			MoveComp->SetMovementMode(MOVE_Falling);
			return;
		}

		const EMovementMode RestoreMode =
			(DefaultMovementMode == MOVE_Flying || DefaultMovementMode == MOVE_None)
			? MOVE_Walking
			: DefaultMovementMode;
		MoveComp->SetMovementMode(RestoreMode);
	}
}

void ARetrieveEnemyCharacter::SetAerialSpecialAttackReady(bool bReady)
{
	bAerialSpecialAttackReady = bReady;
	if (!bReady)
	{
		AerialSpecialPhaseStartTime = -1.f;
	}
}

void ARetrieveEnemyCharacter::BeginAerialSpecialPhase()
{
	if (AerialSpecialPhaseStartTime >= 0.f)
	{
		return;
	}

	const UWorld* World = GetWorld();
	AerialSpecialPhaseStartTime = World ? World->GetTimeSeconds() : 0.f;
}

void ARetrieveEnemyCharacter::ResetAerialSpecialPhase()
{
	bAerialSpecialAttackReady = false;
	AerialSpecialPhaseStartTime = -1.f;
}

float ARetrieveEnemyCharacter::GetAerialSpecialPhaseElapsedTime() const
{
	if (AerialSpecialPhaseStartTime < 0.f)
	{
		return 0.f;
	}

	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.f, World->GetTimeSeconds() - AerialSpecialPhaseStartTime) : 0.f;
}

bool ARetrieveEnemyCharacter::HasAerialPhase() const
{
	// 데이터 기반으로만 판정한다. (특정 행 이름 하드코딩 제거)
	// 에픽을 보스처럼 지상 전투 + 쿨다운 특수공격으로 운용하기로 했으므로,
	// DT_MonsterData에서 bHasAerialPhase가 true가 아닌 한 비행 페이즈는 비활성이다.
	const FMonsterDataRow* Row = GetMonsterDataRow();
	return Row && Row->bHasAerialPhase;
}

void ARetrieveEnemyCharacter::DeactivateEnemy()
{
	ResetAerialSpecialPhase();
	StopLocomotionMontages();

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
	if (Payload.InstigatorEnemy == this || AlertedTarget)
	{
		return;
	}
	if (FVector::Dist(GetActorLocation(), Payload.InstigatorLocation) > GroupAlertRadius)
	{
		return;
	}
	AActor* SpottedActor = Payload.SpottedActor.Get();
	if (!IsValid(SpottedActor))
	{
		return;
	}

	if (EngageStaggerMaxDelay <= 0.f)
	{
		AlertedTarget = SpottedActor;
		return;
	}
	const float Delay = FMath::FRandRange(0.f, EngageStaggerMaxDelay);
	FTimerDelegate InDelegate = FTimerDelegate::CreateWeakLambda(this, [this, SpottedActor]()
	{
		if (!AlertedTarget && IsValid(SpottedActor))
		{
			AlertedTarget = SpottedActor;
		}
	});
	GetWorldTimerManager().SetTimer(AlertStaggerTimer, InDelegate, FMath::Max(0.05f, Delay), false);
}
