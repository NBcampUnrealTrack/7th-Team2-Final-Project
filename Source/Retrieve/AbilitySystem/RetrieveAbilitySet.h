#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "RetrieveAbilitySet.generated.h"

class URetrieveAbilitySystemComponent;
class URetrieveGameplayAbility;
class UAttributeSet;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FRetrieveAbilitySet_GameplayAbility
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<URetrieveGameplayAbility> Ability;

	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Input,Ability"))
	FGameplayTag InputTag;
};

USTRUCT(BlueprintType)
struct FRetrieveAbilitySet_GameplayEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> GameplayEffect;

	UPROPERTY(EditDefaultsOnly)
	float EffectLevel = 1.0f;
};

USTRUCT(BlueprintType)
struct FRetrieveAbilitySet_AttributeSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAttributeSet> AttributeSet;
};

USTRUCT(BlueprintType)
struct FRetrieveAbilitySet_GrantedHandles
{
	GENERATED_BODY()

	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* Set);

	void TakeFromAbilitySystem(URetrieveAbilitySystemComponent* ASC);

protected:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};

UCLASS(BlueprintType, Const)
class RETRIEVE_API URetrieveAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	void GiveToAbilitySystem(URetrieveAbilitySystemComponent* ASC, FRetrieveAbilitySet_GrantedHandles* OutGrantedHandles,
	                         UObject* SourceObject = nullptr) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities", meta = (TitleProperty = Ability))
	TArray<FRetrieveAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects", meta = (TitleProperty = GameplayEffect))
	TArray<FRetrieveAbilitySet_GameplayEffect> GrantedGameplayEffects;

	UPROPERTY(EditDefaultsOnly, Category = "Attribute Sets", meta = (TitleProperty = AttributeSet))
	TArray<FRetrieveAbilitySet_AttributeSet> GrantedAttributes;
};
