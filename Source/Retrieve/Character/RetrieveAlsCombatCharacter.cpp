
#include "RetrieveAlsCombatCharacter.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "TimerManager.h"
#include "Utility/AlsGameplayTags.h"

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

	// 죽음 몽타주의 쓰러진 포즈가 부활 후에도 남으면, 캡슐은 정상인데 몸이 지면 아래로
	// 그려져 "리스폰 땅꺼짐"처럼 보인다. 몽타주를 멈추고 애님 일시정지도 해제한다.
	// 주의: 이 몽타주 정리는 반드시 StopRagdoll보다 먼저 한다 — StopRagdoll(ALS StopRagdolling)이
	// 지면 래그돌에서 겟업 몽타주를 재생하며 SetInputBlocked(true)+GettingUp 액션을 거는데,
	// 그 몽타주를 같은 프레임에 죽이면 몽타주의 SetLocomotionAction 노티가 시작조차 못 해
	// 입력 차단이 영구 잔존한다("부활은 되는데 시체 상태로 조작 불가"의 원인).
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = false;
		MeshComp->bNoSkeletonUpdate = false;
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.2f);
		}
	}

	StopRagdoll();

	SetActorLocationAndRotation(RespawnTransform.GetLocation(), RespawnTransform.GetRotation().Rotator(), false,
	                            nullptr, ETeleportType::TeleportPhysics);

	// 사망 연출(죽음 몽타주 종료 시 래그돌 트리거)이 부활 "이후" 뒤늦게 발동해 캡슐 콜리전을
	// NoCollision으로 다시 꺼버리는 경합이 있다 — 그 상태로 걸으면 지면을 통과한다(리스폰 땅꺼짐).
	// 부활 뒤 잠시 동안 재점검해 살아있는데 시체화된 상태를 강제로 복구한다.
	// (스트리밍 리스폰이 길어지면 이 고정 시점들이 텔레포트 완료 전에 소진되므로,
	//  BeginPlay에서 SaveSubsystem::OnFastTravelCompleted에도 ReassertAliveState를 연결해 둔다.)
	if (UWorld* World = GetWorld())
	{
		// 죽음 몽타주 잔여 길이를 모르므로 세 시점으로 나눠 재점검한다.
		FTimerHandle Reassert1, Reassert2, Reassert3;
		World->GetTimerManager().SetTimer(Reassert1, this, &ARetrieveAlsCombatCharacter::ReassertAliveState, 0.3f, false);
		World->GetTimerManager().SetTimer(Reassert2, this, &ARetrieveAlsCombatCharacter::ReassertAliveState, 1.2f, false);
		World->GetTimerManager().SetTimer(Reassert3, this, &ARetrieveAlsCombatCharacter::ReassertAliveState, 2.5f, false);
	}
}

void ARetrieveAlsCombatCharacter::ReassertAliveState()
{
#if !UE_BUILD_SHIPPING
	// 임시 진단(입구 로그 — 사망 조기 반환 "전"에 찍어 재사망 상태도 관측): 해결 확인 후 제거
	UE_LOG(LogTemp, Warning, TEXT("[부활진단/입구] %s: 사망중=%d HP=%.0f/%.0f"),
		*GetName(),
		(HealthComponent && HealthComponent->IsDeadOrDying()) ? 1 : 0,
		HealthComponent ? HealthComponent->GetHealth() : -1.f,
		HealthComponent ? HealthComponent->GetMaxHealth() : -1.f);
#endif

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
			AnimInstance && AnimInstance->IsAnyMontagePlaying()
			&& GetLocomotionAction() != AlsLocomotionActionTags::GettingUp)
		{
			// 부활 후에도 죽음 몽타주가 계속 재생 중이면 정지한다(쓰러진 포즈 잔존 방지).
			// 단 ALS 겟업 몽타주는 살려 둔다 — 여기서 죽이면 GettingUp 해제 노티가 못 돌아 입력 차단이
			// 잔존할 수 있다(아래 고아 액션 정리가 최후 방어지만, 애니가 자연 종료되는 쪽이 정상 경로).
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

	// 고아 LocomotionAction 정리: 액션(GettingUp 등)이 걸려 있는데 소유 몽타주가 재생 중이 아니면
	// 해제할 주체가 없다(몽타주가 SetLocomotionAction 노티 시작 전에 조기 종료된 경우 등).
	// 액션이 남으면 ALS SetInputBlocked(true)가 유지돼 이동 입력이 영구 차단된다("부활 후 시체 상태").
	// Ragdolling은 위 StopRagdoll 소관이므로 제외.
	if (const FGameplayTag& CurrentAction = GetLocomotionAction();
		CurrentAction.IsValid() && CurrentAction != AlsLocomotionActionTags::Ragdolling)
	{
		const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (!AnimInstance || !AnimInstance->IsAnyMontagePlaying())
		{
			SetLocomotionAction(FGameplayTag::EmptyTag); // NotifyLocomotionActionChanged가 입력 차단을 해제
			bFixed = true;
		}
	}

	// State.Player.Dead 루즈 태그 잔존 정리: 살아있는데(bDeathStarted=false) 태그가 남아 있으면
	// ABP 죽음 포즈/HUD 사망 표시가 시체 상태로 박제된다. (사망 시 Add ↔ 부활 시 Remove 짝이
	// 어긋나는 경로 방어 — 루즈 태그는 카운트 방식이라 1회 Remove로 안 풀릴 수 있음)
	if (URetrieveAbilitySystemComponent* RetrieveASC = Cast<URetrieveAbilitySystemComponent>(GetAbilitySystemComponent());
		RetrieveASC && RetrieveASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Dead))
	{
		RetrieveASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Dead, 0);
		bFixed = true;
	}

	// 메인 메시 외 스켈메시(VisualMesh 등)에 물리 시뮬이 잔존하면 화면에 보이는 몸만 래그돌로 남는다.
	// (진단·복구가 GetMesh만 다루던 사각지대 — 이중 메시 구조 대응)
	{
		TInlineComponentArray<USkeletalMeshComponent*> SkelMeshes(this);
		for (USkeletalMeshComponent* Skel : SkelMeshes)
		{
			if (Skel && Skel != GetMesh() && Skel->IsSimulatingPhysics())
			{
				Skel->SetSimulatePhysics(false);
				bFixed = true;
			}
		}
	}

#if !UE_BUILD_SHIPPING
	// 임시 진단: "부활 후 시체 상태" 원인 확정용 상태 덤프 — 해결 확인 후 제거
	{
		const UCharacterMovementComponent* Move = GetCharacterMovement();
		const USkeletalMeshComponent* MeshComp = GetMesh();
		const UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
		const int32 DeadTagCount = GetAbilitySystemComponent()
			? GetAbilitySystemComponent()->GetTagCount(RetrieveGameplayTags::State_Player_Dead) : -1;
		FString MeshStates;
		TInlineComponentArray<USkeletalMeshComponent*> SkelMeshes(this);
		for (const USkeletalMeshComponent* Skel : SkelMeshes)
		{
			if (Skel)
			{
				MeshStates += FString::Printf(TEXT("%s(물리=%d 정지=%d 애님모드=%d) "),
					*Skel->GetName(), Skel->IsSimulatingPhysics() ? 1 : 0, Skel->bPauseAnims ? 1 : 0,
					static_cast<int32>(Skel->GetAnimationMode()));
			}
		}
		UE_LOG(LogTemp, Warning,
			TEXT("[부활진단] %s: 사망중=%d HP=%.0f/%.0f 사망태그=%d 이동모드=%d 액션=%s 몽타주재생=%d 액터충돌=%d 수정발생=%d 메시[%s]"),
			*GetName(),
			(HealthComponent && HealthComponent->IsDeadOrDying()) ? 1 : 0,
			HealthComponent ? HealthComponent->GetHealth() : -1.f,
			HealthComponent ? HealthComponent->GetMaxHealth() : -1.f,
			DeadTagCount,
			Move ? static_cast<int32>(Move->MovementMode.GetValue()) : -1,
			*GetLocomotionAction().ToString(),
			(AnimInstance && AnimInstance->IsAnyMontagePlaying()) ? 1 : 0,
			GetActorEnableCollision() ? 1 : 0,
			bFixed ? 1 : 0,
			*MeshStates);
	}
#endif

	if (bFixed)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[ReviveGuard] %s: 부활 후 뒤늦은 시체화 상태(래그돌/캡슐/이동/고아 액션) 감지 → 강제 복구"),
			*GetName());
	}
}

void ARetrieveAlsCombatCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.AddDynamic(this, &ARetrieveAlsCombatCharacter::HandleDeathStarted);
	}

	// 스트리밍 리스폰/빠른이동 완료 직후에도 시체화 잔존 상태를 재점검한다.
	// Revive의 고정 타이머(≤2.5s)는 목적지 셀 스트리밍이 긴 리스폰(체크포인트가 먼 경우 수 초~십수 초)에서
	// 텔레포트 완료 전에 소진돼, 완료 시점의 이동모드 복원/상태 잔존을 아무도 점검하지 못했다.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSubsystem = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSubsystem->OnFastTravelCompleted.AddUniqueDynamic(this, &ARetrieveAlsCombatCharacter::ReassertAliveState);
		}
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
