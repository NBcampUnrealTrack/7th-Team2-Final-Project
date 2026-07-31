#include "Components/Element/ElementUnlockComponent.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

UElementUnlockComponent::UElementUnlockComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// AreAllElementsUnlocked 판정 기준 — 가디언 3원소
	AllElements.AddTag(RetrieveGameplayTags::Element_Fire);
	AllElements.AddTag(RetrieveGameplayTags::Element_Water);
	AllElements.AddTag(RetrieveGameplayTags::Element_Wind);
}

void UElementUnlockComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UElementUnlockComponent, UnlockedElements);
	DOREPLIFETIME(UElementUnlockComponent, bLumenEngraved);
}

void UElementUnlockComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!IsValid(InASC)) { return; }

	ASC = InASC;

	InASC->GenericGameplayEventCallbacks
		.FindOrAdd(RetrieveGameplayTags::GameplayEvent_Core_Absorb)
		.AddUObject(this, &ThisClass::HandleCoreAbsorb);

	InASC->GenericGameplayEventCallbacks
		.FindOrAdd(RetrieveGameplayTags::GameplayEvent_Element_ModeChange)
		.AddUObject(this, &ThisClass::HandleElementModeChanged);

	LoadFromPersistentState();

	// 이미 해금된 원소 모드로 빙의했을 수 있으므로 즉시 재평가.
	RefreshAwakeningEffect();

	// 로드는 in-place라 폰 재빙의 없이 진행된다. 로드 완료 시 슬롯 기준으로 재동기화하도록 구독.
	if (URetrieveSaveSubsystem* Save = GetSaveSubsystem())
	{
		Save->OnWorldObjectStatesChanged.AddUniqueDynamic(this, &UElementUnlockComponent::HandleSaveLoaded);
	}
}

void UElementUnlockComponent::UninitializeFromAbilitySystem()
{
	if (URetrieveSaveSubsystem* Save = GetSaveSubsystem())
	{
		Save->OnWorldObjectStatesChanged.RemoveDynamic(this, &UElementUnlockComponent::HandleSaveLoaded);
	}

	if (UAbilitySystemComponent* CachedASC = ASC.Get())
	{
		if (auto* AbsorbDelegate = CachedASC->GenericGameplayEventCallbacks.Find(RetrieveGameplayTags::GameplayEvent_Core_Absorb))
		{
			AbsorbDelegate->RemoveAll(this);
		}
		if (auto* ModeDelegate = CachedASC->GenericGameplayEventCallbacks.Find(RetrieveGameplayTags::GameplayEvent_Element_ModeChange))
		{
			ModeDelegate->RemoveAll(this);
		}

		// 폰 교체(재빙의) 시 ASC(PlayerState)에 남은 버프 중복 적용을 방지.
		if (ActiveAwakeningHandle.IsValid())
		{
			CachedASC->RemoveActiveGameplayEffect(ActiveAwakeningHandle);
			ActiveAwakeningHandle.Invalidate();
		}
	}
	ASC = nullptr;
}

void UElementUnlockComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UElementUnlockComponent::HandleCoreAbsorb(const FGameplayEventData* Payload)
{
	if (!Payload || Payload->InstigatorTags.IsEmpty()) { return; }

	TArray<FGameplayTag> Tags;
	Payload->InstigatorTags.GetGameplayTagArray(Tags);
	UnlockElement(Tags[0]);
}

void UElementUnlockComponent::UnlockElement(FGameplayTag ElementTag)
{
	// 진행 상태 변경 / 영속 저장은 호스트 권한에서만. 클라는 복제로 수신.
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) { return; }

	if (!ElementTag.IsValid() || UnlockedElements.HasTagExact(ElementTag)) { return; }

	UnlockedElements.AddTag(ElementTag);

	if (URetrieveSaveSubsystem* Save = GetSaveSubsystem())
	{
		Save->MarkElementUnlocked(ElementTag);
	}

	UE_LOG(LogTemp, Log, TEXT("[ElementUnlock] 원소 해방 — %s (총 %d)"),
		*ElementTag.ToString(), UnlockedElements.Num());

	// 해금 시점에 이미 그 원소 모드라면 각성 버프 즉시 반영.
	RefreshAwakeningEffect();

	OnElementUnlocked.Broadcast(ElementTag);
}

void UElementUnlockComponent::HandleElementModeChanged(const FGameplayEventData* /*Payload*/)
{
	RefreshAwakeningEffect();
}

void UElementUnlockComponent::RefreshAwakeningEffect()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) { return; }

	UAbilitySystemComponent* CachedASC = ASC.Get();
	if (!CachedASC) { return; }

	// 기존 각성 버프 제거 (모드 전환/해제 대비)
	if (ActiveAwakeningHandle.IsValid())
	{
		CachedASC->RemoveActiveGameplayEffect(ActiveAwakeningHandle);
		ActiveAwakeningHandle.Invalidate();
	}

	// 현재 원소 모드(loose 태그) && 해당 원소 해금 && GE 매핑이 있으면 부여.
	for (const TPair<FGameplayTag, TSubclassOf<UGameplayEffect>>& Pair : ElementAwakeningEffects)
	{
		const FGameplayTag& ElementTag = Pair.Key;
		if (!Pair.Value) { continue; }
		if (!CachedASC->HasMatchingGameplayTag(ElementTag)) { continue; } // 현재 그 모드 아님
		if (!IsElementUnlocked(ElementTag)) { continue; }                  // 미해금

		FGameplayEffectContextHandle Context = CachedASC->MakeEffectContext();
		Context.AddSourceObject(this);

		const FGameplayEffectSpecHandle Spec = CachedASC->MakeOutgoingSpec(Pair.Value, 1.f, Context);
		if (Spec.IsValid())
		{
			ActiveAwakeningHandle = CachedASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			UE_LOG(LogTemp, Log, TEXT("[ElementUnlock] 각성 버프 부여 — %s"), *ElementTag.ToString());
		}
		break; // 동시에 하나의 원소 모드만 활성
	}
}

bool UElementUnlockComponent::IsElementUnlocked(FGameplayTag ElementTag) const
{
	return UnlockedElements.HasTagExact(ElementTag);
}

bool UElementUnlockComponent::AreAllElementsUnlocked() const
{
	return !AllElements.IsEmpty() && UnlockedElements.HasAll(AllElements);
}

void UElementUnlockComponent::InitializeByLumenEngrave()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) { return; }

	if (bLumenEngraved) { return; }

	bLumenEngraved = true;

	if (URetrieveSaveSubsystem* Save = GetSaveSubsystem())
	{
		Save->SetLumenEngraved(true);
	}
}

void UElementUnlockComponent::LoadFromPersistentState()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) { return; }

	if (URetrieveSaveSubsystem* Save = GetSaveSubsystem())
	{
		UnlockedElements = Save->GetUnlockedElements();
		bLumenEngraved = Save->IsLumenEngraved();
	}
}

void UElementUnlockComponent::HandleSaveLoaded()
{
	// 전부-되돌리기: 로드된 슬롯 기준으로 해방/각인 플래그를 다시 읽고 각성 버프를 재평가.
	LoadFromPersistentState();
	RefreshAwakeningEffect();

	// 현재 모드가 회수된 원소면 기본 Fire 모드로 강제 전환.
	EnsureValidElementModeAfterLoad();
}

void UElementUnlockComponent::EnsureValidElementModeAfterLoad()
{
	UAbilitySystemComponent* CachedASC = ASC.Get();
	if (!CachedASC) { return; }

	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) { return; }

	// 현재 Water/Wind 모드인데 그 원소가 미해금이면(로드로 회수) 기본 Fire 모드로 되돌린다.
	// Fire 모드는 스폰 기본이자 항상 허용되는 상태라 건드리지 않는다. (모드 전환은 해금 게이팅 없음)
	const bool bInLockedWater =
		CachedASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Water) &&
		!IsElementUnlocked(RetrieveGameplayTags::Element_Water);
	const bool bInLockedWind =
		CachedASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Wind) &&
		!IsElementUnlocked(RetrieveGameplayTags::Element_Wind);

	if (!bInLockedWater && !bInLockedWind)
	{
		return;
	}

	// GA_SetElement_Base와 동일 시퀀스: 전 모드 태그 클리어 → Fire만 세팅 → ModeChange 발행.
	CachedASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_Fire,  0);
	CachedASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_Water, 0);
	CachedASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_Wind,  0);
	CachedASC->SetLooseGameplayTagCount(RetrieveGameplayTags::Element_None,  0);
	CachedASC->AddLooseGameplayTag(RetrieveGameplayTags::Element_Fire);

	FGameplayEventData Payload;
	Payload.EventTag = RetrieveGameplayTags::GameplayEvent_Element_ModeChange;
	if (AActor* Avatar = CachedASC->GetAvatarActor())
	{
		Payload.Instigator = Avatar;
		Payload.Target = Avatar;
	}
	Payload.InstigatorTags.AddTag(RetrieveGameplayTags::Element_Fire);
	CachedASC->HandleGameplayEvent(Payload.EventTag, &Payload);
}

URetrieveSaveSubsystem* UElementUnlockComponent::GetSaveSubsystem() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<URetrieveSaveSubsystem>();
		}
	}
	return nullptr;
}
