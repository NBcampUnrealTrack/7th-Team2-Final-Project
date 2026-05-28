#include "Logging/RetrieveCheatManager.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

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
