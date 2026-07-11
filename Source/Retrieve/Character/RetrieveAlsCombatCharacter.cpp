
#include "RetrieveAlsCombatCharacter.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

ARetrieveAlsCombatCharacter::ARetrieveAlsCombatCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HealthComponent = CreateDefaultSubobject<URetrieveHealthComponent>(TEXT("HealthComponent"));
}

void ARetrieveAlsCombatCharacter::Revive(const FTransform& RespawnTransform)
{
	if (HealthComponent)
	{
		HealthComponent->Revive();
	}
	StopRagdoll();

	// 죽음 몽타주의 쓰러진 포즈가 부활 후에도 남으면, 캡슐은 정상인데 몸이 지면 아래로
	// 그려져 "리스폰 땅꺼짐"처럼 보인다. 몽타주를 멈추고 애님 일시정지도 해제한다.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = false;
		MeshComp->bNoSkeletonUpdate = false;
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.2f);
		}
	}

	SetActorLocationAndRotation(RespawnTransform.GetLocation(), RespawnTransform.GetRotation().Rotator(), false,
	                            nullptr, ETeleportType::TeleportPhysics);

	// 사망 연출(죽음 몽타주 종료 시 래그돌 트리거)이 부활 "이후" 뒤늦게 발동해 캡슐 콜리전을
	// NoCollision으로 다시 꺼버리는 경합이 있다 — 그 상태로 걸으면 지면을 통과한다(리스폰 땅꺼짐).
	// 부활 뒤 잠시 동안 재점검해 살아있는데 시체화된 상태를 강제로 복구한다.
	const FTimerDelegate ReassertAlive = FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (HealthComponent && HealthComponent->IsDeadOrDying())
		{
			return; // 재점검 사이에 진짜로 다시 죽었으면 유지
		}

		bool bFixed = false;
		if (StopRagdoll())
		{
			bFixed = true;
		}
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (MeshComp->bPauseAnims || MeshComp->bNoSkeletonUpdate)
			{
				MeshComp->bPauseAnims = false;
				MeshComp->bNoSkeletonUpdate = false;
				bFixed = true;
			}
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
				AnimInstance && AnimInstance->IsAnyMontagePlaying())
			{
				// 부활 후에도 죽음 몽타주가 계속 재생 중이면 정지한다(쓰러진 포즈 잔존 방지).
				AnimInstance->Montage_Stop(0.2f);
				bFixed = true;
			}
		}
		if (UCapsuleComponent* Capsule = GetCapsuleComponent();
			Capsule && Capsule->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			bFixed = true;
		}
		if (UCharacterMovementComponent* Move = GetCharacterMovement();
			Move && Move->MovementMode == MOVE_None)
		{
			Move->SetMovementMode(MOVE_Walking);
			bFixed = true;
		}
		if (bFixed)
		{
			UE_LOG(LogTemp, Log,
				TEXT("[ReviveGuard] %s: 부활 후 뒤늦은 시체화 상태(래그돌/캡슐/이동) 감지 → 강제 복구"),
				*GetName());
		}
	});

	if (UWorld* World = GetWorld())
	{
		// 죽음 몽타주 잔여 길이를 모르므로 세 시점으로 나눠 재점검한다.
		FTimerHandle Reassert1, Reassert2, Reassert3;
		World->GetTimerManager().SetTimer(Reassert1, ReassertAlive, 0.3f, false);
		World->GetTimerManager().SetTimer(Reassert2, ReassertAlive, 1.2f, false);
		World->GetTimerManager().SetTimer(Reassert3, ReassertAlive, 2.5f, false);
	}
}

void ARetrieveAlsCombatCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.AddDynamic(this, &ARetrieveAlsCombatCharacter::HandleDeathStarted);
	}
	
	if (URetrievePawnExtensionComponent* PawnExt = GetPawnExtensionComponent())
	{
		PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(this,
				&ARetrieveAlsCombatCharacter::HandleAbilitySystemInitialized));	
	}
}

void ARetrieveAlsCombatCharacter::HandleAbilitySystemInitialized()
{
	URetrievePawnExtensionComponent* PawnExt = GetPawnExtensionComponent();

	if (PawnExt && HealthComponent)
	{
		HealthComponent->InitializeWithAbilitySystem(PawnExt->GetRetrieveAbilitySystemComponent());
	}

	// 베이스의 GAS 태그 → ALS 상태 매핑 등록 (Sprint / Crouch / LockOn 등)
	OnAbilitySystemReady();
}

void ARetrieveAlsCombatCharacter::HandleDeathStarted(AActor* OwningActor)
{
	// 기본 구현: 이동 정지. 아키타입 서브클래스(Sovereign 등)가 사망 GA 활성화/GameplayEvent.*.Die 전송을 추가합니다.
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	// ALS 사망 레그돌은 GA_Die가 죽음 몽타주 종료 시점에 트리거 (방향 C).
	// 여기서 즉시 호출하면 몽타주와 동시 발동해 서로 덮어쓰므로 호출하지 않는다.
}
