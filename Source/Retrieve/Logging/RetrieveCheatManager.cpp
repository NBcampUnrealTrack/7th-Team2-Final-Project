#include "Logging/RetrieveCheatManager.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Core/RetrieveGameState.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Components/PatternCounterComponent.h"
#include "Components/CombatReactionComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Quest/QuestBranchComponent.h"
#include "Components/ElementUnlockComponent.h"

#include "Components/PatternCounterComponent.h"
#include "Components/CombatReactionComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Components/EnemyCombatComponent.h"
#include "AbilitySystemInterface.h"

UAbilitySystemComponent* URetrieveCheatManager::GetLocalPlayerASC() const
{
	const APlayerController* PC = GetOuterAPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheatManager] PlayerController 없음"));
		return nullptr;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheatManager] 빙의된 Pawn 없음"));
		return nullptr;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheatManager] ASC 없음 — PawnData/AbilitySet 확인"));
	}
	return ASC;
}

class UPatternCounterComponent* URetrieveCheatManager::GetLockedOnPatternCounter() const
{
    const APlayerController* PC = GetOuterAPlayerController();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CheatManager] 빙의된 Pawn 없음"));
        return nullptr;
    }

    UCombatReactionComponent* Reaction = Pawn->FindComponentByClass<UCombatReactionComponent>();
    if (!Reaction)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CheatManager] CombatReactionComponent 없음"));
        return nullptr;
    }

    AActor* Target = Reaction->GetLockOnTarget();
    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CheatManager] 락온 대상 없음 — Tab으로 락온 먼저"));
        return nullptr;
    }

    UPatternCounterComponent* Counter = Target->FindComponentByClass<UPatternCounterComponent>();
    if (!Counter)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CheatManager] 대상에 PatternCounterComponent 없음: %s"),
            *Target->GetName());
    }
    return Counter;
}

void URetrieveCheatManager::RetrieveKillPlayer()
{
	UAbilitySystemComponent* ASC = GetLocalPlayerASC();
	if (!ASC) return;

	// Health를 직접 0으로 세팅 → HandleHealthChanged → 사망 파이프라인 트리거
	ASC->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), 0.f);

	UE_LOG(LogTemp, Display, TEXT("[CheatManager] RetrieveKillPlayer 실행"));
}

void URetrieveCheatManager::RetrieveDamagePlayer(float Amount)
{
	if (Amount <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheatManager] Amount는 양수여야 합니다. (입력값: %.1f)"), Amount);
		return;
	}

	UAbilitySystemComponent* ASC = GetLocalPlayerASC();
	if (!ASC) return;

	const float CurrentHP = ASC->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute());
	const float NewHP = FMath::Max(CurrentHP - Amount, 0.f);
	ASC->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), NewHP);

	UE_LOG(LogTemp, Display,
	       TEXT("[CheatManager] RetrieveDamagePlayer %.1f → HP: %.1f → %.1f"),
	       Amount, CurrentHP, NewHP);
}

void URetrieveCheatManager::RetrieveSetHealth(float Value)
{
	UAbilitySystemComponent* ASC = GetLocalPlayerASC();
	if (!ASC) return;

	const float MaxHP = ASC->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute());
	const float ClampedValue = FMath::Clamp(Value, 0.f, MaxHP);
	ASC->SetNumericAttributeBase(UCombatAttributeSet::GetHealthAttribute(), ClampedValue);

	UE_LOG(LogTemp, Display,
	       TEXT("[CheatManager] RetrieveSetHealth → %.1f (MaxHP: %.1f)"), ClampedValue, MaxHP);
}

ARetrieveGameState* URetrieveCheatManager::GetRetrieveGameState() const
{
	const APlayerController* PC = GetOuterAPlayerController();
	return (PC && PC->GetWorld()) ? PC->GetWorld()->GetGameState<ARetrieveGameState>() : nullptr;
}

void URetrieveCheatManager::RetrieveQuestComplete(const FString& StepTagName)
{
	ARetrieveGameState* GS = GetRetrieveGameState();
	if (!GS)
	{
		return;
	}
	
	UQuestBranchComponent* Quest = GS->GetQuestBranchComponent();
	if (!Quest)
	{
		return;
	}

	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*StepTagName), false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheatManager] 알 수 없는 태그: %s"), *StepTagName);
		return;
	}

	const bool bStepCompleted = Quest->CompleteStep(Tag);
	UE_LOG(LogTemp, Display, TEXT("[CheatManager] QuestComplete %s -> %s"),
	       *StepTagName, bStepCompleted ? TEXT("OK") : TEXT("거부됨(이미 완료됨/전제조건 미충족/행 없음/호스트 아님)"));
}

void URetrieveCheatManager::RetrieveUnlockWind()
{
	const APlayerController* PC = GetOuterAPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheatManager] 빙의된 Pawn 없음"));
		return;
	}

	UElementUnlockComponent* Unlock = Pawn->FindComponentByClass<UElementUnlockComponent>();
	if (!Unlock)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CheatManager] ElementUnlockComponent 없음 — SovereignCharacter 확인"));
		return;
	}

	Unlock->UnlockElement(RetrieveGameplayTags::Element_Wind);

	UE_LOG(LogTemp, Display, TEXT("[CheatManager] RetrieveUnlockWind -> %s"),
	       Unlock->IsElementUnlocked(RetrieveGameplayTags::Element_Wind) ? TEXT("해금됨") : TEXT("실패(호스트 아님?)"));
}

void URetrieveCheatManager::RetrieveLumenToggleWait()
{
	const APlayerController* PC = GetOuterAPlayerController();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	
	FRetrieveLumenCommandPayload Message;
	Message.CommandTag = RetrieveGameplayTags::Channel_Lumen_Command_ToggleWait;
	Message.Instigator = PC->GetPawn();
	UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Lumen_Command_ToggleWait,
	                                                       Message);
}

void URetrieveCheatManager::RetrieveLumenRecall()
{
	const APlayerController* PC = GetOuterAPlayerController();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	
	FRetrieveLumenCommandPayload Message;
	Message.CommandTag = RetrieveGameplayTags::Channel_Lumen_Command_Recall;
	Message.Instigator = PC->GetPawn();
	UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Lumen_Command_Recall, Message);
}

void URetrieveCheatManager::RetrieveToggleCombatTag()
{
	UAbilitySystemComponent* ASC = GetLocalPlayerASC();
	if (!ASC)
	{
		return;
	}
	
	const FGameplayTag Tag = RetrieveGameplayTags::State_Player_Combat;
	if (ASC->HasMatchingGameplayTag(Tag))
	{
		ASC->RemoveLooseGameplayTag(Tag);
	}
	else
	{
		ASC->AddLooseGameplayTag(Tag);
	}
	
	UE_LOG(LogTemp, Display, TEXT("[CheatManager] State.Player.Combat -> %s"),
	       ASC->HasMatchingGameplayTag(Tag) ? TEXT("ON") : TEXT("OFF"));
}

void URetrieveCheatManager::RetrieveTestGuardHit(bool bHeavy)
{
    UAbilitySystemComponent* ASC = GetLocalPlayerASC();
    if (!ASC) return;

    static const TCHAR* GEPath = TEXT("/Game/Retrieve/AbilitySystem/Player/GE_DebugGuardHit.GE_DebugGuardHit_C");
    UClass* GEClass = StaticLoadClass(UGameplayEffect::StaticClass(), nullptr, GEPath);
    if (!GEClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CheatManager] GE_DebugGuardHit 로드 실패: %s"), GEPath);
        return;
    }

    APawn* SelfPawn = GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr;

    FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
    Ctx.AddInstigator(SelfPawn, SelfPawn);

    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GEClass, 1.f, Ctx);
    if (!Spec.IsValid()) return;

    if (bHeavy)
    {
        Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Heavy);
        Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Property_GuardBreak);
    }
    else
    {
        Spec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Normal);
    }

    ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

    UE_LOG(LogTemp, Display, TEXT("[CheatManager] RetrieveTestGuardHit(bHeavy=%d) 적용"), bHeavy ? 1 : 0);
}

void URetrieveCheatManager::RetrieveTestCounter()
{
    UPatternCounterComponent* Counter = GetLockedOnPatternCounter();
    if (!Counter) return;
    
    if (UEnemyCombatComponent* Combat = Counter->GetOwner()->FindComponentByClass<UEnemyCombatComponent>())
    {
        Counter->SetActivePatternRow(TEXT("Wyvern_AerialDive"), Combat->GetPatternTable());
    }
    
    Counter->OpenCounterWindow(0.f);
    const bool bWasOpen = Counter->IsWindowOpen();

    Counter->TryCounter(
        FGameplayTag::EmptyTag, RetrieveGameplayTags::Element_Fire,
        GetOuterAPlayerController()->GetPawn());

    bool bGroggy = false;
    if (IAbilitySystemInterface* ASCIf = Cast<IAbilitySystemInterface>(Counter->GetOwner()))
    {
        if (UAbilitySystemComponent* EnemyASC = ASCIf->GetAbilitySystemComponent())
        {
            bGroggy = EnemyASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Groggy);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("[CheatManager] 그로기 진입=%s"), bGroggy ? TEXT("true") : TEXT("false"));
    const bool bCountered = bWasOpen && !Counter->IsWindowOpen();

    UE_LOG(LogTemp, Display,
        TEXT("[CheatManager] TestCounter → Opened=%s, 결과=%s"),
        bWasOpen ? TEXT("true") : TEXT("false"),
        bCountered ? TEXT("카운터 성공") : TEXT("실패"));
}

void URetrieveCheatManager::RetrieveOpenCounterWindow(float Duration)
{
    UPatternCounterComponent* Counter = GetLockedOnPatternCounter();
    if (!Counter) return;

    AActor* Enemy = Counter->GetOwner();

    FGameplayEventData Payload;
    Payload.EventTag       = RetrieveGameplayTags::GameplayEvent_PatternCounterWindow;
    Payload.Instigator     = GetOuterAPlayerController()->GetPawn();
    Payload.EventMagnitude = Duration;
    
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        Enemy, RetrieveGameplayTags::GameplayEvent_PatternCounterWindow, Payload);

    UE_LOG(LogTemp, Display,
        TEXT("[CheatManager] OpenCounterWindow → %s, Duration=%.2f, WindowOpen=%s"),
        *Enemy->GetName(), Duration,
        Counter->IsWindowOpen() ? TEXT("true") : TEXT("false"));
}

void URetrieveCheatManager::RetrieveTryCounter()
{
	UPatternCounterComponent* Counter = GetLockedOnPatternCounter();
	if (!Counter) return;

	const bool bWasOpen = Counter->IsWindowOpen();

	Counter->TryCounter(
		FGameplayTag::EmptyTag,
		RetrieveGameplayTags::Element_Fire,
		GetOuterAPlayerController()->GetPawn());

	const bool bCountered = bWasOpen && !Counter->IsWindowOpen();

	UE_LOG(LogTemp, Display,
		TEXT("[CheatManager] TryCounter → WindowWasOpen=%s, 결과=%s"),
		bWasOpen ? TEXT("true") : TEXT("false"),
		bCountered ? TEXT("카운터 성공") : TEXT("실패(윈도우 닫힘/미오픈)"));
}

void URetrieveCheatManager::RetrieveTestParryCounter()
{
    UAbilitySystemComponent* ASC = GetLocalPlayerASC();
    if (!ASC) return;

    // ActivationRequiredTags(State.Player.CanCounter) 충족용으로 태그를 임시 부여
    const FGameplayTag CanCounterTag = RetrieveGameplayTags::State_Player_CanCounter;
    const bool bAlreadyHadTag = ASC->HasMatchingGameplayTag(CanCounterTag);
    if (!bAlreadyHadTag)
    {
        ASC->AddLooseGameplayTag(CanCounterTag);
    }

    FGameplayTagContainer ActivateTags;
    ActivateTags.AddTag(RetrieveGameplayTags::Ability_Player_ParryCounter);
    const bool bActivated = ASC->TryActivateAbilitiesByTag(ActivateTags);

    // 활성화 게이트 용도로만 부여했으므로 즉시 제거
    if (!bAlreadyHadTag)
    {
        ASC->RemoveLooseGameplayTag(CanCounterTag);
    }

    UE_LOG(LogTemp, Display,
        TEXT("[CheatManager] TestParryCounter → 활성화=%s (실패 시: 어빌리티셋 미등록 / 무기 미장착 / DamageEffectClass 미지정 확인. 데미지가 없으면 전방에 적 없음)"),
        bActivated ? TEXT("성공") : TEXT("실패"));
}

void URetrieveCheatManager::RetrieveTestHitReact(int32 Strength)
{
    APawn* SelfPawn = GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr;
    if (!SelfPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CheatManager] 빙의된 Pawn 없음"));
        return;
    }

    FGameplayTag ReactTag;
    switch (Strength)
    {
    case 2:  ReactTag = RetrieveGameplayTags::HitReact_Type_Knockdown; break;
    case 1:  ReactTag = RetrieveGameplayTags::HitReact_Type_Stagger;   break;
    default: ReactTag = RetrieveGameplayTags::HitReact_Type_Flinch;    break;
    }
	
    FGameplayEventData EventData;
    EventData.Instigator = SelfPawn;
    EventData.Target     = SelfPawn;
    EventData.EventTag   = RetrieveGameplayTags::GameplayEvent_Hit_Normal;
    EventData.TargetTags.AddTag(ReactTag);

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
        SelfPawn, RetrieveGameplayTags::GameplayEvent_Hit_Normal, EventData);

    UE_LOG(LogTemp, Display, TEXT("[CheatManager] RetrieveTestHitReact(Strength=%d -> %s)"),
        Strength, *ReactTag.ToString());
}
