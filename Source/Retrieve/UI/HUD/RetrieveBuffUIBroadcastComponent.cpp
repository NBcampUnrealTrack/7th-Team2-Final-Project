#include "UI/HUD/RetrieveBuffUIBroadcastComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"

URetrieveBuffUIBroadcastComponent::URetrieveBuffUIBroadcastComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ─────────────────────────── 생명주기 ─────────────────────────────────────────

void URetrieveBuffUIBroadcastComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!WatchTagPrefix.IsValid())
	{
		WatchTagPrefix = RetrieveGameplayTags::UI_Buff;
	}

	// BP에 테이블 미지정 시 기본 정의 테이블 폴백 — 없으면 BuiltInRows(하드코딩 9종)만 동작해
	// DataTable로 추가한 버프칩(공명/세트/모멘텀 등)이 전부 무시된다.
	if (!BuffUITable)
	{
		BuffUITable = Cast<UDataTable>(FSoftObjectPath(
			TEXT("/Game/Retrieve/Data/Skill/DT_BuffDefinitions.DT_BuffDefinitions")).TryLoad());
	}

	InitBuiltInRows();

	// 아이콘을 BeginPlay 시점에 일괄 동기 로드해 두어 실제 발동 시 히치를 방지한다.
	if (BuffUITable)
	{
		static const FString PreloadContext(TEXT("URetrieveBuffUIBroadcastComponent::Preload"));
		TArray<FRetrieveBuffUIRow*> Rows;
		BuffUITable->GetAllRows<FRetrieveBuffUIRow>(PreloadContext, Rows);
		for (FRetrieveBuffUIRow* Row : Rows)
		{
			if (Row && !Row->Icon.IsNull())
			{
				Row->Icon.LoadSynchronous();
			}
		}
	}

	if (IAbilitySystemInterface* ASCOwner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		BindToASC(ASCOwner->GetAbilitySystemComponent());
	}
}

void URetrieveBuffUIBroadcastComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromASC();
	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────── ASC 바인딩 ───────────────────────────────────────

void URetrieveBuffUIBroadcastComponent::RefreshAbilitySystemBinding()
{
	UnbindFromASC();

	if (IAbilitySystemInterface* ASCOwner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		BindToASC(ASCOwner->GetAbilitySystemComponent());
	}
}

void URetrieveBuffUIBroadcastComponent::BindToASC(UAbilitySystemComponent* ASC)
{
	if (!ASC) return;
	if (CachedASC.Get() == ASC) return;

	UnbindFromASC();
	CachedASC = ASC;

	OnGEAddedHandle = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		this, &ThisClass::OnGEAdded);

	OnGERemovedHandle = ASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(
		this, &ThisClass::OnGERemoved);
}

void URetrieveBuffUIBroadcastComponent::UnbindFromASC()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(OnGEAddedHandle);
		ASC->OnAnyGameplayEffectRemovedDelegate().Remove(OnGERemovedHandle);
	}
	CachedASC = nullptr;
	HandleToBuffId.Empty();
}

// ─────────────────────────── GE 이벤트 핸들러 ────────────────────────────────

void URetrieveBuffUIBroadcastComponent::OnGEAdded(
	UAbilitySystemComponent* /*ASC*/,
	const FGameplayEffectSpec& Spec,
	FActiveGameplayEffectHandle Handle)
{
	if (!WatchTagPrefix.IsValid()) return;

	FGameplayTagContainer AssetTags;
	Spec.GetAllAssetTags(AssetTags);

	FGameplayTag FoundTag;
	for (const FGameplayTag& Tag : AssetTags)
	{
		if (Tag.MatchesTag(WatchTagPrefix) && Tag != WatchTagPrefix)
		{
			FoundTag = Tag;
			break;
		}
	}
	if (!FoundTag.IsValid()) return;

	// 모든 방어구 세트 GE가 레거시 UI.Buff.ArmorSet 태그를 공유하므로,
	// 실제 GE 클래스명으로 세트/2·4단계를 분리해 서로 다른 슬롯 키를 만든다.
	if (FoundTag == RetrieveGameplayTags::UI_Buff_ArmorSet && Spec.Def)
	{
		const FString EffectName = Spec.Def->GetClass()->GetName();
		struct FArmorSetTagMapping { const TCHAR* Token; FGameplayTag Tag; };
		const FArmorSetTagMapping Mappings[] =
		{
			{ TEXT("Berserker_2pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Berserker_2pc },
			{ TEXT("Berserker_4pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Berserker_4pc },
			{ TEXT("Gale_2pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Gale_2pc },
			{ TEXT("Gale_4pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Gale_4pc },
			{ TEXT("Guardian_2pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Guardian_2pc },
			{ TEXT("Guardian_4pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Guardian_4pc },
			{ TEXT("Hunter_2pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Hunter_2pc },
			{ TEXT("Hunter_4pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Hunter_4pc },
			{ TEXT("Pyromancer_2pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Pyromancer_2pc },
			{ TEXT("Pyromancer_4pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Pyromancer_4pc },
			{ TEXT("Vampire_2pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Vampire_2pc },
			{ TEXT("Vampire_4pc"), RetrieveGameplayTags::UI_Buff_ArmorSet_Vampire_4pc },
		};
		for (const FArmorSetTagMapping& Mapping : Mappings)
		{
			if (EffectName.Contains(Mapping.Token))
			{
				FoundTag = Mapping.Tag;
				break;
			}
		}
	}

	const FRetrieveBuffUIRow* Row = FindRow(FoundTag);
	if (!Row) return;

	HandleToBuffId.Add(Handle, FoundTag);

	const float Duration = Row->DurationOverride > 0.f
		? Row->DurationOverride
		: FMath::Max(0.f, Spec.GetDuration());

	BroadcastApply(*Row, Duration, Spec.Def ? Spec.Def->GetClass() : nullptr);
}

void URetrieveBuffUIBroadcastComponent::OnGERemoved(const FActiveGameplayEffect& ActiveGE)
{
	FGameplayTag* BuffId = HandleToBuffId.Find(ActiveGE.Handle);
	if (!BuffId) return;

	BroadcastBuffRemove(*BuffId);
	HandleToBuffId.Remove(ActiveGE.Handle);
}

// ─────────────────────────── 수동 발행 API ────────────────────────────────────

void URetrieveBuffUIBroadcastComponent::BroadcastBuffManual(
	FGameplayTag BuffUITag,
	float DurationOverride,
	TSubclassOf<UGameplayEffect> SourceEffect,
	int32 StackCount)
{
	if (!BuffUITag.IsValid()) return;

	const FRetrieveBuffUIRow* Row = FindRow(BuffUITag);
	if (!Row) return;

	const float Duration = DurationOverride > 0.f
		? DurationOverride
		: Row->DurationOverride;

	BroadcastApply(*Row, Duration, SourceEffect, StackCount);
}

void URetrieveBuffUIBroadcastComponent::BroadcastBuffRemove(FGameplayTag BuffUITag)
{
	if (!BuffUITag.IsValid() || !GetWorld()) return;

	FRetrieveUIBuffRemovePayload Payload;
	Payload.BuffId = BuffUITag;

	UGameplayMessageSubsystem::Get(GetWorld())
		.BroadcastMessage(RetrieveGameplayTags::Channel_UI_Buff_Remove, Payload);
}

// ─────────────────────────── 내부 유틸 ────────────────────────────────────────

void URetrieveBuffUIBroadcastComponent::InitBuiltInRows()
{
	BuiltInRows.Reset();

	auto AddRow = [this](
		FGameplayTag Tag,
		const TCHAR* DisplayName,
		const TCHAR* Description,
		const TCHAR* EffectSummary,
		const FLinearColor& Tint,
		float DurationOverride,
		bool bIsStackable = false,
		int32 MaxStack = 0,
		const TCHAR* IconPath = nullptr)
	{
		FRetrieveBuffUIRow Row;
		Row.BuffUITag        = Tag;
		Row.DisplayName      = FText::FromString(FString(DisplayName));
		Row.Description      = FText::FromString(FString(Description));
		Row.EffectSummary    = FText::FromString(FString(EffectSummary));
		Row.TintColor        = Tint;
		Row.DurationOverride = DurationOverride;
		Row.bIsStackable     = bIsStackable;
		Row.MaxStack         = MaxStack;
		if (IconPath)
		{
			Row.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(IconPath));
		}
		BuiltInRows.Add(Tag, Row);
	};

	AddRow(RetrieveGameplayTags::UI_Buff_Item_FireBoost,    TEXT("Fire Boost"),    TEXT("Increases elemental gauge charge while active."), TEXT("Fire item charge multiplier"),  FLinearColor::White,                   0.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Item_WaterBoost,   TEXT("Water Boost"),   TEXT("Increases elemental gauge charge while active."), TEXT("Water item charge multiplier"), FLinearColor::White,                   0.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Item_WindBoost,    TEXT("Wind Boost"),    TEXT("Increases elemental gauge charge while active."), TEXT("Wind item charge multiplier"),  FLinearColor::White,                   0.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Burst_FireSlash,   TEXT("Fire Slash"),    TEXT("Burst effect granted by a fire-focused element pattern."),  TEXT("Fire burst follow-up window"),  FLinearColor::White, 5.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Burst_WaterVortex, TEXT("Water Vortex"),  TEXT("Burst effect granted by a water-focused element pattern."), TEXT("Water burst follow-up window"), FLinearColor::White, 5.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Burst_WindSlash,   TEXT("Wind Slash"),    TEXT("Burst effect granted by a wind-focused element pattern."),  TEXT("Wind burst follow-up window"),  FLinearColor::White, 5.f);
	// 흡수 버프 — 같은 원소를 반복 흡수하면 스택이 쌓인다 (최대 5스택)
	AddRow(RetrieveGameplayTags::UI_Buff_Absorb_Fire,  TEXT("Fire Absorb"),  TEXT("Fire element absorbed. Fire attribute buff active."),  TEXT("Fire absorb buff"),  FLinearColor(1.f,  0.35f, 0.05f, 1.f), 0.f, /*bIsStackable=*/true, /*MaxStack=*/5);
	AddRow(RetrieveGameplayTags::UI_Buff_Absorb_Water, TEXT("Water Absorb"), TEXT("Water element absorbed. Water attribute buff active."), TEXT("Water absorb buff"), FLinearColor(0.1f, 0.45f, 1.f,  1.f), 0.f, /*bIsStackable=*/true, /*MaxStack=*/5);
	AddRow(RetrieveGameplayTags::UI_Buff_Absorb_Wind,  TEXT("Wind Absorb"),  TEXT("Wind element absorbed. Wind attribute buff active."),  TEXT("Wind absorb buff"),  FLinearColor(0.3f, 1.f,  0.35f, 1.f), 0.f, /*bIsStackable=*/true, /*MaxStack=*/5);
	const FString ArmorSetIconRoot(TEXT("/Game/Retrieve/UI/Icons/Buffs/ArmorSets/"));
	auto AddArmorSetRow = [&AddRow, &ArmorSetIconRoot](FGameplayTag Tag, const TCHAR* Name, const TCHAR* Description, const TCHAR* Summary, const TCHAR* AssetName, const FLinearColor& Tint)
	{
		const FString IconPath = ArmorSetIconRoot + AssetName + TEXT(".") + AssetName;
		AddRow(Tag, Name, Description, Summary, Tint, 0.f, false, 0, *IconPath);
	};
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Berserker_2pc, TEXT("Berserker Set (2)"), TEXT("Heavy attack damage increased."), TEXT("Heavy attack damage"), TEXT("T_Buff_Set_Berserker_2pc"), FLinearColor(0.95f, 0.18f, 0.12f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Berserker_4pc, TEXT("Berserker Set (4)"), TEXT("Heavy attack damage and attack speed increased."), TEXT("Heavy damage + attack speed"), TEXT("T_Buff_Set_Berserker_4pc"), FLinearColor(1.f, 0.35f, 0.08f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Gale_2pc, TEXT("Gale Set (2)"), TEXT("Movement speed and attack speed increased."), TEXT("Move + attack speed"), TEXT("T_Buff_Set_Gale_2pc"), FLinearColor(0.2f, 0.9f, 0.45f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Gale_4pc, TEXT("Gale Set (4)"), TEXT("Critical chance increased."), TEXT("Critical chance"), TEXT("T_Buff_Set_Gale_4pc"), FLinearColor(0.25f, 1.f, 0.55f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Guardian_2pc, TEXT("Guardian Set (2)"), TEXT("Guard damage taken reduced."), TEXT("Guard damage reduction"), TEXT("T_Buff_Set_Guardian_2pc"), FLinearColor(0.2f, 0.45f, 1.f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Guardian_4pc, TEXT("Guardian Set (4)"), TEXT("Defense and maximum poise increased."), TEXT("Defense + max poise"), TEXT("T_Buff_Set_Guardian_4pc"), FLinearColor(0.35f, 0.6f, 1.f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Hunter_2pc, TEXT("Hunter Set (2)"), TEXT("Normal attack damage increased."), TEXT("Normal attack damage"), TEXT("T_Buff_Set_Hunter_2pc"), FLinearColor(1.f, 0.65f, 0.12f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Hunter_4pc, TEXT("Hunter Set (4)"), TEXT("Critical chance increased."), TEXT("Critical chance"), TEXT("T_Buff_Set_Hunter_4pc"), FLinearColor(1.f, 0.78f, 0.2f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Pyromancer_2pc, TEXT("Pyromancer Set (2)"), TEXT("Elemental damage increased."), TEXT("Elemental damage"), TEXT("T_Buff_Set_Pyromancer_2pc"), FLinearColor(0.95f, 0.2f, 0.65f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Pyromancer_4pc, TEXT("Pyromancer Set (4)"), TEXT("Element charge gain increased."), TEXT("Element charge gain"), TEXT("T_Buff_Set_Pyromancer_4pc"), FLinearColor(1.f, 0.25f, 0.8f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Vampire_2pc, TEXT("Vampire Set (2)"), TEXT("Life steal increased."), TEXT("Life steal"), TEXT("T_Buff_Set_Vampire_2pc"), FLinearColor(0.75f, 0.05f, 0.12f));
	AddArmorSetRow(RetrieveGameplayTags::UI_Buff_ArmorSet_Vampire_4pc, TEXT("Vampire Set (4)"), TEXT("Life steal further increased."), TEXT("Enhanced life steal"), TEXT("T_Buff_Set_Vampire_4pc"), FLinearColor(1.f, 0.08f, 0.18f));
}

void URetrieveBuffUIBroadcastComponent::BroadcastApply(const FRetrieveBuffUIRow& Row, float Duration, TSubclassOf<UGameplayEffect> SourceEffect, int32 StackCount)
{
	if (!GetWorld()) return;

	FRetrieveUIBuffPayload Payload;
	Payload.BuffId    = Row.BuffUITag;
	Payload.DisplayName = Row.DisplayName;
	Payload.Description = Row.Description;
	Payload.EffectSummary = Row.EffectSummary;
	Payload.LinkedGameplayEffect = Row.LinkedGameplayEffect ? Row.LinkedGameplayEffect : SourceEffect;
	Payload.Icon      = Row.Icon.LoadSynchronous();
	Payload.TintColor = Row.TintColor;
	Payload.Duration     = Duration;
	Payload.bIsDebuff    = Row.bIsDebuff;
	Payload.bIsStackable = Row.bIsStackable;
	Payload.MaxStack     = Row.MaxStack;
	Payload.StackCount   = StackCount;

	UGameplayMessageSubsystem::Get(GetWorld())
		.BroadcastMessage(RetrieveGameplayTags::Channel_UI_Buff_Apply, Payload);
}

const FRetrieveBuffUIRow* URetrieveBuffUIBroadcastComponent::FindRow(FGameplayTag BuffUITag) const
{
	if (!BuffUITag.IsValid()) return nullptr;

	static const FString Context(TEXT("URetrieveBuffUIBroadcastComponent"));
	const FName RowName(*BuffUITag.ToString());
	if (BuffUITable)
	{
		if (const FRetrieveBuffUIRow* Row = BuffUITable->FindRow<FRetrieveBuffUIRow>(
			RowName,
			Context,
			/*bWarnIfRowMissing=*/false))
		{
			return Row;
		}
	}

	return BuiltInRows.Find(BuffUITag);
}
