#include "Character/EnemyCharacter.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/PatternCounterComponent.h"
#include "Components/DropComponent.h"
// TODO: 장유진 UHealthComponent 완성 후 활성화
// #include "Components/HealthComponent.h"

AEnemyCharacter::AEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OwnedASC = CreateDefaultSubobject<URetrieveAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	OwnedASC->SetIsReplicated(true);
	OwnedASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	EnemyCombatComponent    = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("EnemyCombatComponent"));
	PatternCounterComponent = CreateDefaultSubobject<UPatternCounterComponent>(TEXT("PatternCounterComponent"));
	DropComponent           = CreateDefaultSubobject<UDropComponent>(TEXT("DropComponent"));
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();
	// TODO: 장유진 UHealthComponent 완성 후 바인딩 추가
	// if (UHealthComponent* HealthComp = FindComponentByClass<UHealthComponent>())
	// {
	//     HealthComp->OnDeathStarted.AddDynamic(this, &AEnemyCharacter::HandleDeathStarted);
	// }
}

void AEnemyCharacter::InitializeAbilitySystem()
{
	if (PawnExtensionComponent && OwnedASC)
	{
		PawnExtensionComponent->InitializeAbilitySystem(OwnedASC, this);
	}
}

void AEnemyCharacter::HandleDeathStarted(AActor* OwningActor)
{
	// TODO: 장유진 UHealthComponent 완성 후 구현
	// 1. GameplayEvent.Enemy.Die 발행 → StateTree Dead 상태 진입
	// 2. DropComponent->ProcessDrop()
	// 3. Channel.Monster.Died 브로드캐스트
}
