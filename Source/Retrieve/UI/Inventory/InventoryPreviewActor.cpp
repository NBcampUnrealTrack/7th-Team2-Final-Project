#include "UI/Inventory/InventoryPreviewActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Cosmetics/RetrieveModularMeshTypes.h"
#include "Components/Pawn/RetrievePawnCosmeticComponent.h"
#include "Components/Player/ArmorComponent.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"

AInventoryPreviewActor::AInventoryPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AInventoryPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	BindInventoryEvents();
	UpdateArmorPreview();
}

void AInventoryPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindInventoryEvents();
	ClearArmorMeshes();

	Super::EndPlay(EndPlayReason);
}

void AInventoryPreviewActor::ClearArmorMeshes()
{
	for (USkeletalMeshComponent* ArmorMeshComponent : ArmorMeshComponents)
	{
		if (IsValid(ArmorMeshComponent))
		{
			ArmorMeshComponent->DestroyComponent();
		}
	}

	ArmorMeshComponents.Reset();
}

void AInventoryPreviewActor::UpdateArmorPreview()
{
	ClearArmorMeshes();

	USkeletalMeshComponent* LeaderMeshComponent = ResolvePreviewMeshComponent();
	USkeletalMeshComponent* PlayerBodyMesh = ResolvePlayerBodyMeshComponent();
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!LeaderMeshComponent || !PlayerBodyMesh || !PlayerPawn)
	{
		return;
	}

	// 방어구/가면/머리 등은 플레이어 바디와 같은 스켈레톤(SKEL_Default_Sidekick)에 스키닝되어 있고,
	// 프리뷰 기본 메시는 다른 스켈레톤(ModularSyntyCharacter)이라 직접 LeaderPose하면 매핑이 깨진다.
	// 따라서 DT로 방어구만 따로 조립하지 않고, 플레이어가 현재 보여주는 모든 스켈레탈 메시 컴포넌트
	// (베이스 바디 + 기본 파츠(가면/머리) + 장착 방어구)를 통째로 복제하고, 모두 플레이어 바디 포즈를
	// LeaderPose로 따라가게 한다. 플레이어 바디가 애니메이션 중이므로 프리뷰도 동일하게 (바르게) 움직인다.
	// (ModularSynty용 자체 idle을 Sidekick 바디에 호환 리타겟하면 레퍼런스 포즈 차이로 포즈가 무너지므로,
	//  플레이어의 실제 포즈를 그대로 복사하는 방식이 가장 안정적이다.)

	// 루트(프리뷰 리더)는 PlayerBodyMesh 메시+스켈레톤을 미러링해 방어구 파츠의 LeaderPose 본 매핑을 맞춘다.
	// PreviewAnimClassOverride가 설정된 경우 해당 클래스를 사용하고, 없으면 플레이어 ABP를 폴백으로 사용한다.
	// LeaderMeshComponent에 ABP를 독립 인스턴스로 실행해 idle 포즈 + 몽타주 재생을 모두 지원한다.
	TSubclassOf<UAnimInstance> PreviewAnimClass = PreviewAnimClassOverride
		? PreviewAnimClassOverride
		: TSubclassOf<UAnimInstance>(PlayerBodyMesh->GetAnimClass());
	MirrorPlayerMeshComponent(LeaderMeshComponent, PlayerBodyMesh, PlayerBodyMesh);

	// Leader는 자체 AnimInstance로 몽타주를 재생해야 하므로 LeaderPose를 해제하고 ABP를 복원한다.
	// 방어구 파츠는 LeaderMeshComponent가 아닌 PlayerBodyMesh를 직접 LeaderPose로 참조하므로 영향 없다.
	LeaderMeshComponent->SetLeaderPoseComponent(nullptr);
	if (PreviewAnimClass)
	{
		LeaderMeshComponent->SetAnimInstanceClass(PreviewAnimClass);
	}

	// 체형 모프 타깃(masculineFeminine 등)을 Leader 메시에 적용해 여성형 체형으로 만든다.
	// CosmeticComponent는 spawned 파츠에만 SetMorphTarget을 적용하고 GetMesh()에는 적용하지 않으므로
	// CurrentMorphTargets를 직접 읽어서 주입한다.
	if (URetrievePawnCosmeticComponent* CosmeticComp = PlayerPawn->FindComponentByClass<URetrievePawnCosmeticComponent>())
	{
		for (const TPair<FName, float>& Morph : CosmeticComp->GetCurrentMorphTargets())
		{
			LeaderMeshComponent->SetMorphTarget(Morph.Key, Morph.Value);
		}
	}

	// 플레이어의 나머지 스켈레탈 메시 컴포넌트(기본 파츠 + 방어구)를 복제한다.
	TArray<USkeletalMeshComponent*> PlayerMeshComponents;
	PlayerPawn->GetComponents(PlayerMeshComponents);
	for (USkeletalMeshComponent* PlayerComponent : PlayerMeshComponents)
	{
		if (!IsValid(PlayerComponent) || PlayerComponent == PlayerBodyMesh)
		{
			continue;
		}
		if (!PlayerComponent->GetSkeletalMeshAsset())
		{
			continue;
		}

		USkeletalMeshComponent* PreviewComponent = NewObject<USkeletalMeshComponent>(this);
		if (!PreviewComponent)
		{
			continue;
		}

		PreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PreviewComponent->SetGenerateOverlapEvents(false);
		PreviewComponent->SetCastShadow(false);
		PreviewComponent->bCastDynamicShadow = false;
		PreviewComponent->bCastStaticShadow = false;
		// 위치는 프리뷰 리더에 붙이고, register 후 LeaderPose를 걸어야 master bone map이 정상 구축된다.
		PreviewComponent->AttachToComponent(LeaderMeshComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		PreviewComponent->bUseAttachParentBound = true;

		AddInstanceComponent(PreviewComponent);
		PreviewComponent->RegisterComponent();

		MirrorPlayerMeshComponent(PreviewComponent, PlayerComponent, PlayerBodyMesh);
		ArmorMeshComponents.Add(PreviewComponent);
	}
}

void AInventoryPreviewActor::MirrorPlayerMeshComponent(
	USkeletalMeshComponent* Target,
	USkeletalMeshComponent* Source,
	USkeletalMeshComponent* PoseSource) const
{
	if (!IsValid(Target) || !IsValid(Source))
	{
		return;
	}

	Target->SetSkeletalMesh(Source->GetSkeletalMeshAsset());
	// 포즈는 PoseSource(플레이어 바디)에서 LeaderPose로 가져오므로 자체 AnimBP는 비운다.
	Target->SetAnimInstanceClass(nullptr);
	Target->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// 머티리얼 복제 (바디 머티리얼/원소 색상 MID 등 런타임 오버라이드 반영).
	const int32 NumMaterials = Source->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		Target->SetMaterial(MaterialIndex, Source->GetMaterial(MaterialIndex));
	}

	// 모프 타깃 복제: 체형(masculineFeminine 등) 포함, Source에 활성화된 모프 값을 그대로 전달한다.
	if (USkeletalMesh* SkelMesh = Source->GetSkeletalMeshAsset())
	{
		for (UMorphTarget* MorphTarget : SkelMesh->GetMorphTargets())
		{
			if (MorphTarget)
			{
				Target->SetMorphTarget(MorphTarget->GetFName(), Source->GetMorphTarget(MorphTarget->GetFName()));
			}
		}
	}

	// 가시성 복제: 방어구로 가려져 숨긴 기본 파츠(suppression)나 숨긴 베이스 메시를 그대로 반영.
	Target->SetVisibility(Source->IsVisible(), /*bPropagateToChildren=*/false);

	if (IsValid(PoseSource) && PoseSource != Target)
	{
		Target->SetLeaderPoseComponent(PoseSource);
	}
}

void AInventoryPreviewActor::HandleEquippedArmorChanged(FGameplayTag EquipmentSlotTag, FName ArmorItemId)
{
	UpdateArmorPreview();

	if (ArmorItemId.IsNone())
	{
		return;
	}

	FGameplayTag MontagePartSlotTag;
	if (UDataTable* ArmorDataTable = ResolveArmorDataTable())
	{
		if (const FRetrieveArmorDataRow* ArmorData = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			ArmorItemId,
			TEXT("AInventoryPreviewActor::HandleEquippedArmorChanged")))
		{
			for (const FRetrieveArmorVisualPart& VisualPart : ArmorData->VisualParts)
			{
				if (FindArmorEquipMontage(EquipmentSlotTag, VisualPart.PartSlotTag))
				{
					MontagePartSlotTag = VisualPart.PartSlotTag;
					break;
				}
			}
		}
	}

	PlayArmorEquipMontage(EquipmentSlotTag, MontagePartSlotTag);
}

bool AInventoryPreviewActor::PlayArmorEquipMontage(FGameplayTag EquipmentSlotTag, FGameplayTag PartSlotTag)
{
	const FRetrieveInventoryPreviewArmorMontage* MontageConfig = FindArmorEquipMontage(EquipmentSlotTag, PartSlotTag);
	UAnimMontage* MontageToPlay = MontageConfig && MontageConfig->Montage
		? MontageConfig->Montage.Get()
		: ResolveArmorSlotMontage(EquipmentSlotTag);
	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InventoryPreview] PlayArmorEquipMontage FAILED: MontageToPlay is null. SlotTag=%s PartTag=%s"),
			*EquipmentSlotTag.ToString(), *PartSlotTag.ToString());
		return false;
	}

	USkeletalMeshComponent* LeaderMeshComponent = ResolvePreviewMeshComponent();
	UAnimInstance* AnimInstance = LeaderMeshComponent ? LeaderMeshComponent->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InventoryPreview] PlayArmorEquipMontage FAILED: AnimInstance is null. AnimClass=%s"),
			LeaderMeshComponent && LeaderMeshComponent->GetAnimClass()
				? *LeaderMeshComponent->GetAnimClass()->GetName()
				: TEXT("None"));
		return false;
	}

	const float PlayRate = MontageConfig && MontageConfig->Montage
		? MontageConfig->PlayRate
		: ArmorEquipMontagePlayRate;
	const float Duration = AnimInstance->Montage_Play(MontageToPlay, FMath::Max(PlayRate, 0.01f));
	if (Duration <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InventoryPreview] PlayArmorEquipMontage FAILED: Montage_Play returned 0. Montage=%s AnimClass=%s"),
			*MontageToPlay->GetName(), *AnimInstance->GetClass()->GetName());
		return false;
	}

	const FName StartSection = MontageConfig && MontageConfig->Montage
		? MontageConfig->StartSection
		: ArmorEquipMontageStartSection;
	if (!StartSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(StartSection, MontageToPlay);
	}


	return true;
}

UInventoryComponent* AInventoryPreviewActor::ResolveInventoryComponent() const
{
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return PlayerPawn->FindComponentByClass<UInventoryComponent>();
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			return Pawn->FindComponentByClass<UInventoryComponent>();
		}
	}

	return nullptr;
}

UArmorComponent* AInventoryPreviewActor::ResolveArmorComponent() const
{
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return PlayerPawn->FindComponentByClass<UArmorComponent>();
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			return Pawn->FindComponentByClass<UArmorComponent>();
		}
	}

	return nullptr;
}

USkeletalMeshComponent* AInventoryPreviewActor::ResolvePlayerBodyMeshComponent() const
{
	if (const ACharacter* PlayerCharacter = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		return PlayerCharacter->GetMesh();
	}

	if (const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (const ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerController->GetPawn()))
		{
			return PlayerCharacter->GetMesh();
		}
	}

	return nullptr;
}

UDataTable* AInventoryPreviewActor::ResolveArmorDataTable() const
{
	if (ArmorDataTableOverride)
	{
		return ArmorDataTableOverride;
	}

	UArmorComponent* ArmorComponent = BoundArmorComponent.Get();
	if (!ArmorComponent)
	{
		ArmorComponent = ResolveArmorComponent();
	}

	return ArmorComponent ? ArmorComponent->GetArmorDataTable() : nullptr;
}

USkeletalMeshComponent* AInventoryPreviewActor::ResolvePreviewMeshComponent() const
{
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	GetComponents(SkeletalMeshComponents);

	if (!PreviewSkeletalMeshComponentName.IsNone())
	{
		for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
		{
			if (SkeletalMeshComponent && SkeletalMeshComponent->GetFName() == PreviewSkeletalMeshComponentName)
			{
				return SkeletalMeshComponent;
			}
		}
	}

	return SkeletalMeshComponents.Num() > 0 ? SkeletalMeshComponents[0] : nullptr;
}

const FRetrieveInventoryPreviewArmorMontage* AInventoryPreviewActor::FindArmorEquipMontage(
	FGameplayTag EquipmentSlotTag,
	FGameplayTag PartSlotTag) const
{
	const FRetrieveInventoryPreviewArmorMontage* SlotFallback = nullptr;
	for (const FRetrieveInventoryPreviewArmorMontage& MontageConfig : ArmorEquipMontages)
	{
		if (!MontageConfig.Montage || MontageConfig.EquipmentSlotTag != EquipmentSlotTag)
		{
			continue;
		}

		if (PartSlotTag.IsValid() && MontageConfig.PartSlotTag == PartSlotTag)
		{
			return &MontageConfig;
		}

		if (!MontageConfig.PartSlotTag.IsValid())
		{
			SlotFallback = &MontageConfig;
		}
	}

	return SlotFallback;
}

UAnimMontage* AInventoryPreviewActor::ResolveArmorSlotMontage(FGameplayTag EquipmentSlotTag) const
{
	if (EquipmentSlotTag == RetrieveGameplayTags::Equipment_Slot_Head && Montage_ArmorHead)
	{
		return Montage_ArmorHead;
	}

	if (EquipmentSlotTag == RetrieveGameplayTags::Equipment_Slot_Chest && Montage_ArmorChest)
	{
		return Montage_ArmorChest;
	}

	if (EquipmentSlotTag == RetrieveGameplayTags::Equipment_Slot_Hands && Montage_ArmorHands)
	{
		return Montage_ArmorHands;
	}

	if (EquipmentSlotTag == RetrieveGameplayTags::Equipment_Slot_Legs && Montage_ArmorLegs)
	{
		return Montage_ArmorLegs;
	}

	if (EquipmentSlotTag == RetrieveGameplayTags::Equipment_Slot_Feet && Montage_ArmorFeet)
	{
		return Montage_ArmorFeet;
	}

	return Montage_ArmorDefault;
}

void AInventoryPreviewActor::BindInventoryEvents()
{
	BoundInventoryComponent = ResolveInventoryComponent();
	BoundArmorComponent = ResolveArmorComponent();

	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnEquippedArmorChanged.RemoveDynamic(this, &ThisClass::HandleEquippedArmorChanged);
		BoundInventoryComponent->OnEquippedArmorChanged.AddDynamic(this, &ThisClass::HandleEquippedArmorChanged);
	}
}

void AInventoryPreviewActor::UnbindInventoryEvents()
{
	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnEquippedArmorChanged.RemoveDynamic(this, &ThisClass::HandleEquippedArmorChanged);
	}
}
