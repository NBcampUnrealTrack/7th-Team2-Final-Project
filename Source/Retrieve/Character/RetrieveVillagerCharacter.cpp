#include "Character/RetrieveVillagerCharacter.h"

#include "NPC/NPCPatrolAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/Navigation/NavigationAvoidanceTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#endif

ARetrieveVillagerCharacter::ARetrieveVillagerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = ANPCPatrolAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		MovementComp->bOrientRotationToMovement = true;
		// 스냅 회전 대신 자연스럽게 방향을 트는 속도
		MovementComp->RotationRate = FRotator(0.f, 180.f, 0.f);
		// 여유롭게 걷는 속도와 완만한 가감속 (플레이어의 급가속/급정지 느낌 배제)
		MovementComp->MaxWalkSpeed = 250.f;
		MovementComp->MaxAcceleration = 400.f;
		MovementComp->BrakingDecelerationWalking = 400.f;

		// RVO 회피 활성화: 같은 그룹(0)끼리 서로를 피해가며 걷게 해 서로 밀치며 뭉치는 현상을 줄인다.
		// (이분 탐색 결과 복원: 꺼두면 인사 중 두 캡슐이 너무 가까이 붙어 오래 정지하며 파묻힘 버그가 재현됨)
		MovementComp->bUseRVOAvoidance = true;
		MovementComp->AvoidanceConsiderationRadius = 300.f;
		MovementComp->AvoidanceWeight = 0.5f;
		FNavAvoidanceMask VillagerAvoidanceGroup;
		VillagerAvoidanceGroup.SetFlagsDirectly(1);
		MovementComp->SetAvoidanceGroupMask(VillagerAvoidanceGroup);
		MovementComp->SetGroupsToAvoidMask(VillagerAvoidanceGroup);
	}
	bUseControllerRotationYaw = false;
}

void ARetrieveVillagerCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitialSpawnLocation = GetActorLocation();
}

TArray<FString> ARetrieveVillagerCharacter::GetVillagerMeshOptions() const
{
	TArray<FString> Options;

#if WITH_EDITOR
	// Synty 인사/일상행동 몽타주가 IK 리타겟 파이프라인으로 실제 구축된 팩(스켈레톤)의
	// 메시 폴더만 나열한다. 이 목록 밖의 메시(예: 다른 팩)를 고르면 리타겟된 애니메이션이
	// 스켈레톤 불일치로 깨지므로, 애초에 고를 수 없게 막는다.
	static const TArray<FString> PipelineFolders = {
		TEXT("/Game/External/PolygonFantasyKingdom/Meshes/CharactersUE4"),
		TEXT("/Game/External/PolygonElven/Meshes/CharactersUE4Mannequin"),
		TEXT("/Game/External/PolygonDarkFantasy/Meshes/CharactersUE4Mannequin"),
		TEXT("/Game/External/PolygonDungeonRealms/Meshes/CharactersUE4Mannequin"),
		TEXT("/Game/External/PolygonDarkFortress/DarkFortress/Meshes/Characters"),
	};

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	for (const FString& Folder : PipelineFolders)
	{
		TArray<FAssetData> Assets;
		AssetRegistryModule.Get().GetAssetsByPath(FName(*Folder), Assets, /*bRecursive=*/false);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClassPath.GetAssetName() == TEXT("SkeletalMesh"))
			{
				Options.Add(Asset.GetSoftObjectPath().ToString());
			}
		}
	}
#endif

	return Options;
}

#if WITH_EDITOR
void ARetrieveVillagerCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() != GET_MEMBER_NAME_CHECKED(ARetrieveVillagerCharacter, VillagerMeshOption))
	{
		return;
	}

	if (VillagerMeshOption.IsEmpty())
	{
		return;
	}

	if (USkeletalMesh* NewMesh = LoadObject<USkeletalMesh>(nullptr, *VillagerMeshOption))
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetSkeletalMeshAsset(NewMesh);
		}
	}
}
#endif
