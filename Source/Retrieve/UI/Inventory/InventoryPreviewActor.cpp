#include "UI/Inventory/InventoryPreviewActor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Cosmetics/RetrieveModularMeshTypes.h"
#include "Components/Pawn/RetrievePawnCosmeticComponent.h"
#include "Components/Player/ArmorComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "UI/InventoryPreviewAnimInstance.h"

AInventoryPreviewActor::AInventoryPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AInventoryPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	BindInventoryEvents();
	UpdateArmorPreview();
	InitializePreviewAnimation();
}

void AInventoryPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindInventoryEvents();
	ClearArmorMeshes();

	Super::EndPlay(EndPlayReason);
}

void AInventoryPreviewActor::ClearArmorMeshes()
{
	// 앵커를 먼저 파괴하면 자식 무기가 월드 위치를 유지한 채 분리되어 공중에 남는다.
	ClearWeaponPreviewMeshes();

	for (USkeletalMeshComponent* ArmorMeshComponent : ArmorMeshComponents)
	{
		if (IsValid(ArmorMeshComponent))
		{
			ArmorMeshComponent->DestroyComponent();
		}
	}
	ArmorMeshComponents.Reset();

	if (IsValid(WeaponSocketAnchorComponent))
	{
		WeaponSocketAnchorComponent->DestroyComponent();
		WeaponSocketAnchorComponent = nullptr;
	}
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

	// 루트(프리뷰 리더)는 PlayerBodyMesh 메시+스켈레톤만 미러링한다.
	// AnimClass / SourceMesh 주입은 InitializePreviewAnimation에서만 처리한다.
	MirrorPreviewLeaderMesh(LeaderMeshComponent, PlayerBodyMesh);

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
	// PlayerBodyMesh의 명명된 소켓에 붙은 컴포넌트(무기)는 건너뛴다.
	// 무기는 플레이어 라이브 상태(납검/전환 중 hidden 등)에 독립적으로 데이터에서 직접 재구성한다.
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
		// 바디 메시의 명명된 소켓에 붙은 컴포넌트 = 무기 → 데이터 기반으로 별도 처리
		if (PlayerComponent->GetAttachParent() == PlayerBodyMesh
			&& !PlayerComponent->GetAttachSocketName().IsNone())
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
		// 방어구 파츠는 루트에 붙이고 LeaderPose로 본 포즈를 따른다.
		PreviewComponent->AttachToComponent(LeaderMeshComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		PreviewComponent->bUseAttachParentBound = true;

		AddInstanceComponent(PreviewComponent);
		PreviewComponent->RegisterComponent();

		MirrorPreviewPartMesh(PreviewComponent, PlayerComponent, LeaderMeshComponent);
		ArmorMeshComponents.Add(PreviewComponent);
	}

	// 무기 소켓 앵커: PlayerBodyMesh를 LeaderPose로 따르므로 Weapon_R 소켓이
	// 방어구 파츠와 동일한 좌표계(플레이어 실제 포즈)에서 평가된다.
	WeaponSocketAnchorComponent = NewObject<USkeletalMeshComponent>(this);
	if (WeaponSocketAnchorComponent)
	{
		WeaponSocketAnchorComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WeaponSocketAnchorComponent->SetGenerateOverlapEvents(false);
		WeaponSocketAnchorComponent->SetCastShadow(false);
		WeaponSocketAnchorComponent->bCastDynamicShadow = false;
		WeaponSocketAnchorComponent->bCastStaticShadow = false;
		WeaponSocketAnchorComponent->SetVisibility(false, false);
		WeaponSocketAnchorComponent->AttachToComponent(LeaderMeshComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		WeaponSocketAnchorComponent->bUseAttachParentBound = true;
		AddInstanceComponent(WeaponSocketAnchorComponent);
		WeaponSocketAnchorComponent->RegisterComponent();
		MirrorPreviewLeaderMesh(WeaponSocketAnchorComponent, PlayerBodyMesh);
		// LeaderPose를 설정해야 소켓이 애니메이션 포즈 기준으로 평가된다.
		// (없으면 T-pose 소켓 위치에 무기가 붙어 공중에 떠 보인다)
		WeaponSocketAnchorComponent->SetLeaderPoseComponent(LeaderMeshComponent);
		// MirrorPreviewLeaderMesh 가 Source 가시성을 복사하므로 앵커는 다시 숨긴다.
		WeaponSocketAnchorComponent->SetVisibility(false, false);
	}

	RefreshWeaponPreview(BoundInventoryComponent
		? BoundInventoryComponent->GetEquippedWeaponId()
		: NAME_None);

	RefreshSceneCaptureShowOnlyList();
}

void AInventoryPreviewActor::ClearWeaponPreviewMeshes()
{
	for (UMeshComponent* WeaponMeshComponent : WeaponPreviewMeshComponents)
	{
		if (IsValid(WeaponMeshComponent))
		{
			WeaponMeshComponent->DestroyComponent();
		}
	}
	WeaponPreviewMeshComponents.Reset();
}

void AInventoryPreviewActor::RefreshWeaponPreview(FName WeaponItemId)
{
	ClearWeaponPreviewMeshes();

	if (WeaponItemId.IsNone() || !IsValid(WeaponSocketAnchorComponent))
	{
		RefreshSceneCaptureShowOnlyList();
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UWeaponComponent* WeaponComponent = PlayerPawn
		? PlayerPawn->FindComponentByClass<UWeaponComponent>()
		: nullptr;
	if (!WeaponComponent)
	{
		return;
	}

	// 현재 장착 여부와 무관하게 WeaponItemId 로 데이터를 직접 조회한다.
	// (미장착 무기도 인벤토리 프리뷰에서 표시되어야 하므로)
	const FRetrieveWeaponDataRow* WeaponDataPtr = WeaponComponent->FindWeaponData(WeaponItemId);
	if (!WeaponDataPtr)
	{
		return;
	}

	for (const FRetrieveWeaponAttachmentData& Attachment : WeaponDataPtr->Attachments)
	{
		UMeshComponent* PreviewWeaponMesh = nullptr;
		if (Attachment.MeshType == ERetrieveWeaponMeshType::StaticMesh)
		{
			if (UStaticMesh* StaticMesh = Attachment.StaticMesh.LoadSynchronous())
			{
				UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(this);
				StaticMeshComponent->SetStaticMesh(StaticMesh);
				PreviewWeaponMesh = StaticMeshComponent;
			}
		}
		else if (USkeletalMesh* SkeletalMesh = Attachment.SkeletalMesh.LoadSynchronous())
		{
			USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(this);
			SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);
			PreviewWeaponMesh = SkeletalMeshComponent;
		}

		if (!PreviewWeaponMesh)
		{
			continue;
		}

		if (!Attachment.AttachSocketName.IsNone()
			&& !WeaponSocketAnchorComponent->DoesSocketExist(Attachment.AttachSocketName))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[InventoryPreview] Weapon socket not found. Weapon=%s Socket=%s"),
				*WeaponItemId.ToString(), *Attachment.AttachSocketName.ToString());
			PreviewWeaponMesh->DestroyComponent();
			continue;
		}

		PreviewWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PreviewWeaponMesh->SetGenerateOverlapEvents(false);
		PreviewWeaponMesh->SetCanEverAffectNavigation(false);
		PreviewWeaponMesh->SetCastShadow(false);
		AddInstanceComponent(PreviewWeaponMesh);
		PreviewWeaponMesh->RegisterComponent();
		PreviewWeaponMesh->AttachToComponent(
			WeaponSocketAnchorComponent,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			Attachment.AttachSocketName);
		PreviewWeaponMesh->SetRelativeTransform(Attachment.RelativeTransform);
		WeaponPreviewMeshComponents.Add(PreviewWeaponMesh);
	}

	// 새로 추가된 무기 메시를 SceneCapture show-only 목록에 반영한다.
	RefreshSceneCaptureShowOnlyList();
}

void AInventoryPreviewActor::InitializePreviewAnimation()
{
	USkeletalMeshComponent* LeaderMeshComponent = ResolvePreviewMeshComponent();
	USkeletalMeshComponent* PlayerBodyMesh = ResolvePlayerBodyMeshComponent();
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!LeaderMeshComponent || !PlayerBodyMesh || !PlayerPawn)
	{
		return;
	}

	TSubclassOf<UAnimInstance> PreviewAnimClass = PreviewAnimClassOverride
		? PreviewAnimClassOverride
		: TSubclassOf<UAnimInstance>(PlayerBodyMesh->GetAnimClass());

	LeaderMeshComponent->SetLeaderPoseComponent(nullptr);
	if (PreviewAnimClass)
	{
		LeaderMeshComponent->SetAnimInstanceClass(PreviewAnimClass);
	}

	if (UInventoryPreviewAnimInstance* PreviewAnimInstance = Cast<UInventoryPreviewAnimInstance>(LeaderMeshComponent->GetAnimInstance()))
	{
		PreviewAnimInstance->SetSourceMeshComponent(PlayerBodyMesh);

		if (UAbilitySystemComponent* PlayerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerPawn))
		{
			PreviewAnimInstance->InitializeWithAbilitySystem(PlayerASC);
		}
	}
}

void AInventoryPreviewActor::MirrorPreviewLeaderMesh(USkeletalMeshComponent* Target, USkeletalMeshComponent* Source) const
{
	if (!IsValid(Target) || !IsValid(Source))
	{
		return;
	}

	if (Target->GetSkeletalMeshAsset() != Source->GetSkeletalMeshAsset())
	{
		Target->SetSkeletalMesh(Source->GetSkeletalMeshAsset());
	}
	Target->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	const int32 NumMaterials = Source->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		Target->SetMaterial(MaterialIndex, Source->GetMaterial(MaterialIndex));
	}

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

	Target->SetVisibility(Source->IsVisible(), /*bPropagateToChildren=*/false);
}

void AInventoryPreviewActor::MirrorPreviewPartMesh(
	USkeletalMeshComponent* Target,
	USkeletalMeshComponent* Source,
	USkeletalMeshComponent* PoseSource) const
{
	if (!IsValid(Target) || !IsValid(Source))
	{
		return;
	}

	Target->SetSkeletalMesh(Source->GetSkeletalMeshAsset());
	// 포즈는 PoseSource에서 LeaderPose로 가져오므로 파츠 자체 AnimBP는 비운다.
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

void AInventoryPreviewActor::HandleEquippedWeaponChanged(FName WeaponItemId)
{
	RefreshWeaponPreview(WeaponItemId);
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

USceneCaptureComponent2D* AInventoryPreviewActor::ResolveSceneCaptureComponent() const
{
	TArray<USceneCaptureComponent2D*> SceneCaptureComponents;
	GetComponents(SceneCaptureComponents);

	if (!PreviewSceneCaptureComponentName.IsNone())
	{
		for (USceneCaptureComponent2D* SceneCaptureComponent : SceneCaptureComponents)
		{
			if (SceneCaptureComponent && SceneCaptureComponent->GetFName() == PreviewSceneCaptureComponentName)
			{
				return SceneCaptureComponent;
			}
		}
	}

	return SceneCaptureComponents.Num() > 0 ? SceneCaptureComponents[0] : nullptr;
}

void AInventoryPreviewActor::RefreshSceneCaptureShowOnlyList()
{
	USceneCaptureComponent2D* SceneCaptureComponent = ResolveSceneCaptureComponent();
	if (!SceneCaptureComponent)
	{
		return;
	}

	// 방어구/모듈러 파츠는 UpdateArmorPreview에서 재생성되므로 ShowOnlyList도 매번 갱신해야 한다.
	SceneCaptureComponent->ShowOnlyActors.Reset();
	SceneCaptureComponent->ShowOnlyComponents.Reset();

	if (USkeletalMeshComponent* LeaderMeshComponent = ResolvePreviewMeshComponent())
	{
		SceneCaptureComponent->ShowOnlyComponent(LeaderMeshComponent);
	}

	for (USkeletalMeshComponent* ArmorMeshComponent : ArmorMeshComponents)
	{
		if (IsValid(ArmorMeshComponent))
		{
			SceneCaptureComponent->ShowOnlyComponent(ArmorMeshComponent);
		}
	}

	for (UMeshComponent* WeaponMeshComponent : WeaponPreviewMeshComponents)
	{
		if (IsValid(WeaponMeshComponent))
		{
			SceneCaptureComponent->ShowOnlyComponent(WeaponMeshComponent);
		}
	}
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
		BoundInventoryComponent->OnEquippedWeaponChanged.RemoveDynamic(this, &ThisClass::HandleEquippedWeaponChanged);
		BoundInventoryComponent->OnEquippedWeaponChanged.AddDynamic(this, &ThisClass::HandleEquippedWeaponChanged);
		BoundInventoryComponent->OnEquippedArmorChanged.RemoveDynamic(this, &ThisClass::HandleEquippedArmorChanged);
		BoundInventoryComponent->OnEquippedArmorChanged.AddDynamic(this, &ThisClass::HandleEquippedArmorChanged);
	}
}

void AInventoryPreviewActor::UnbindInventoryEvents()
{
	if (BoundInventoryComponent)
	{
		BoundInventoryComponent->OnEquippedWeaponChanged.RemoveDynamic(this, &ThisClass::HandleEquippedWeaponChanged);
		BoundInventoryComponent->OnEquippedArmorChanged.RemoveDynamic(this, &ThisClass::HandleEquippedArmorChanged);
	}
}
