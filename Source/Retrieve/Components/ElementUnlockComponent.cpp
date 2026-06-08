#include "Components/ElementUnlockComponent.h"

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
}

void UElementUnlockComponent::UninitializeFromAbilitySystem()
{
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
