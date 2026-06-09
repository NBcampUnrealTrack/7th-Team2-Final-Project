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
	TSubclassOf<UGameplayEffect> SourceEffect)
{
	if (!BuffUITag.IsValid()) return;

	const FRetrieveBuffUIRow* Row = FindRow(BuffUITag);
	if (!Row) return;

	const float Duration = DurationOverride > 0.f
		? DurationOverride
		: Row->DurationOverride;

	BroadcastApply(*Row, Duration, SourceEffect);
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
		float DurationOverride)
	{
		FRetrieveBuffUIRow Row;
		Row.BuffUITag = Tag;
		Row.DisplayName = FText::FromString(FString(DisplayName));
		Row.Description = FText::FromString(FString(Description));
		Row.EffectSummary = FText::FromString(FString(EffectSummary));
		Row.TintColor = Tint;
		Row.DurationOverride = DurationOverride;
		BuiltInRows.Add(Tag, Row);
	};

	AddRow(RetrieveGameplayTags::UI_Buff_Item_FireBoost, TEXT("Fire Boost"), TEXT("Increases elemental gauge charge while active."), TEXT("Fire item charge multiplier"), FLinearColor::White, 0.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Item_WaterBoost, TEXT("Water Boost"), TEXT("Increases elemental gauge charge while active."), TEXT("Water item charge multiplier"), FLinearColor::White, 0.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Item_WindBoost, TEXT("Wind Boost"), TEXT("Increases elemental gauge charge while active."), TEXT("Wind item charge multiplier"), FLinearColor::White, 0.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Burst_FireSlash, TEXT("Fire Slash"), TEXT("Burst effect granted by a fire-focused element pattern."), TEXT("Fire burst follow-up window"), FLinearColor::White, 5.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Burst_WaterVortex, TEXT("Water Vortex"), TEXT("Burst effect granted by a water-focused element pattern."), TEXT("Water burst follow-up window"), FLinearColor::White, 5.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Burst_WindSlash, TEXT("Wind Slash"), TEXT("Burst effect granted by a wind-focused element pattern."), TEXT("Wind burst follow-up window"), FLinearColor::White, 5.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Absorb_Fire, TEXT("Fire Absorb"), TEXT("Fire element absorbed. Fire attribute buff active."), TEXT("Fire absorb buff"), FLinearColor(1.f, 0.35f, 0.05f, 1.f), 0.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Absorb_Water, TEXT("Water Absorb"), TEXT("Water element absorbed. Water attribute buff active."), TEXT("Water absorb buff"), FLinearColor(0.1f, 0.45f, 1.f, 1.f), 0.f);
	AddRow(RetrieveGameplayTags::UI_Buff_Absorb_Wind, TEXT("Wind Absorb"), TEXT("Wind element absorbed. Wind attribute buff active."), TEXT("Wind absorb buff"), FLinearColor(0.3f, 1.f, 0.35f, 1.f), 0.f);
}

void URetrieveBuffUIBroadcastComponent::BroadcastApply(const FRetrieveBuffUIRow& Row, float Duration, TSubclassOf<UGameplayEffect> SourceEffect)
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
	Payload.Duration  = Duration;
	Payload.bIsDebuff = Row.bIsDebuff;

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
