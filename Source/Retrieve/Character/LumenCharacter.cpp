#include "Character/LumenCharacter.h"

#include "../Lumen/LumenFollowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/World/RetrieveDialogueComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

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

	// 골렘 메시가 SK_Mannequin_PhysicsAsset(체구가 훨씬 작은 스켈레톤 기준)을 사용해
	// 자동 계산된 바운드가 실제 메시보다 작다. World Partition 스트리밍/컬링 시
	// 이 축소된 바운드로 화면 밖이라 판정돼 Shipping에서 메시 전체가 렌더링되지 않는다.
	// 바운드를 넉넉히 키우고 화면 밖에서도 본을 계속 갱신하도록 강제한다.
	if (USkeletalMeshComponent* LumenMesh = GetMesh())
	{
		LumenMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		LumenMesh->bComponentUseFixedSkelBounds = true;
		LumenMesh->SetBoundsScale(2.0f);
		LumenMesh->MarkRenderStateDirty();
	}

	FollowComponent = CreateDefaultSubobject<ULumenFollowComponent>(TEXT("FollowComponent"));
	DialogueComponent = CreateDefaultSubobject<URetrieveDialogueComponent>(TEXT("DialogueComponent"));
}

void ALumenCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 임시 방어 코드: 레벨 데이터/CDO 모두 HiddenInGame=false로 저장돼 있음을 직접 확인했음에도
	// Shipping 런타임에서는 스폰 직후부터 true로 관측된다 (원인 불명 - C++/블루프린트/StateTree
	// 바인딩을 모두 뒤졌으나 이 메시를 숨기는 코드를 찾지 못함). 근본 원인 파악 전까지, 매 0.2초마다
	// 강제로 다시 보이게 만들어 증상을 우회한다.
	GetWorldTimerManager().SetTimer(ForceVisibleTimerHandle, this, &ALumenCharacter::ForceMeshVisible, 0.2f, true);
}

void ALumenCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 월드/레벨 트랜지션 중 액터가 파괴돼도 반복 타이머가 남아있다가 무효해진 월드에서
	// 발동해 크래시가 나는 것을 막기 위해 명시적으로 해제한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ForceVisibleTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ALumenCharacter::ForceMeshVisible()
{
	if (!IsValid(this))
	{
		return;
	}

	if (USkeletalMeshComponent* LumenMesh = GetMesh())
	{
		// bHiddenInGame과 bVisible은 서로 다른 별개의 플래그이며 IsVisible()은 bVisible만 반영한다.
		// bHiddenInGame만 강제로 되돌렸을 때 IsVisible()이 계속 0으로 나온 것으로 보아
		// bVisible 쪽도 별도로 false가 되고 있을 가능성이 있어 둘 다 강제한다.
		if (LumenMesh->bHiddenInGame)
		{
			LumenMesh->SetHiddenInGame(false);
		}
		if (!LumenMesh->IsVisible())
		{
			LumenMesh->SetVisibility(true, true);
		}
	}
}
