#include "Character/LumenCharacter.h"

#include "../Lumen/LumenFollowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/World/RetrieveDialogueComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	FollowComponent = CreateDefaultSubobject<ULumenFollowComponent>(TEXT("FollowComponent"));
	DialogueComponent = CreateDefaultSubobject<URetrieveDialogueComponent>(TEXT("DialogueComponent"));
}
