#include "Logging/RetrieveCheatManager.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Core/RetrieveGameState.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Quest/QuestBranchComponent.h"

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

    Spec.Data->AddDynamicAssetTag(bHeavy
        ? RetrieveGameplayTags::Attack_Type_Heavy
        : RetrieveGameplayTags::Attack_Type_Normal);

    ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

    UE_LOG(LogTemp, Display, TEXT("[CheatManager] RetrieveTestGuardHit(bHeavy=%d) 적용"), bHeavy ? 1 : 0);
}
