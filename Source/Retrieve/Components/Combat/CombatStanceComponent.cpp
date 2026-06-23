#include "CombatStanceComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Components/Player/WeaponComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "TimerManager.h"

UCombatStanceComponent::UCombatStanceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatStanceComponent::NotifyCombatActivity(bool bFromAttack)
{
	SetStance(ERetrieveCombatStance::DrawnCombat, /*bInstant=*/bFromAttack); // 즉시 승격(공격發이면 발검 연출 스킵)

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SheatheTimerHandle);
		World->GetTimerManager().SetTimer(RelaxTimerHandle, this,
			&UCombatStanceComponent::HandleRelaxTimer, FMath::Max(0.01f,RelaxDelay), false);
	}
}

void UCombatStanceComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	if (!IsValid(ASC) || OwnerASC == ASC)
	{
		return;
	}
	UninitializeFromAbilitySystem();
	OwnerASC = ASC;
			
	// 공격 발동을 '중앙에서' 감지 - 어빌리티 마다 코드 x
	AbilityActivateHandle = OwnerASC->AbilityActivatedCallbacks.AddUObject(this, &UCombatStanceComponent::HandleAbilityActivated);

	// 적 포착 신호 구독. 적 AI가 이미 Channel.Enemy.PlayerSpotted를 쏘므로 AI 무수정.
	if (UWorld* World = GetWorld())
	{
		SpottedListenerHandle = UGameplayMessageSubsystem::Get(World).RegisterListener(
			RetrieveGameplayTags::Channel_Enemy_PlayerSpotted,
			this, &UCombatStanceComponent::HandlePlayerSpotted);
	}

	// 시작 = 납검. 스폰 시엔 몽타주 없이 '태그만' 초기화한다.
	// (SetStance는 동일 상태면 early-return이라 초기 태그가 안 박히고, 스폰부터 납검 몽타주가 나가면 안 됨)
	CurrentStance = ERetrieveCombatStance::Sheathed;
	OwnerASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_WeaponSheathed, 1);
	OwnerASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Combat, 0);

	// 이미 장착된 무기는 등 소켓으로 맞춰둔다. (무기 장착이 init보다 뒤면 OnWeaponEquipped 시점 동기화 필요 — TODO)
	if (UWeaponComponent* Weapon = GetOwner() ? GetOwner()->FindComponentByClass<UWeaponComponent>() : nullptr)
	{
		Weapon->SetWeaponDrawn(false);
	}
}

void UCombatStanceComponent::UninitializeFromAbilitySystem()
{
	if (IsValid(OwnerASC) && AbilityActivateHandle.IsValid())
	{
		OwnerASC->AbilityActivatedCallbacks.Remove(AbilityActivateHandle);
	}
	AbilityActivateHandle.Reset();

	if (SpottedListenerHandle.IsValid())
	{
		SpottedListenerHandle.Unregister();
	}

	OwnerASC = nullptr;
}

void UCombatStanceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RelaxTimerHandle);
		World->GetTimerManager().ClearTimer(SheatheTimerHandle);
	}
	UninitializeFromAbilitySystem();	
	Super::EndPlay(EndPlayReason);
}

void UCombatStanceComponent::SetStance(ERetrieveCombatStance NewStance, bool bInstant)
{
	if (CurrentStance == NewStance || !IsValid(OwnerASC))
	{
		return;
	}
	const ERetrieveCombatStance OldStance = CurrentStance;
	CurrentStance = NewStance;

	const bool bSheathedNow = (NewStance == ERetrieveCombatStance::Sheathed);

	// 상태 → 태그. AnimBP는 GameplayTagPropertyMap으로 이 태그를 bool로 읽어 기본 포즈를 블렌드한다.
	// 0/1 명시 Set(언더플로 방지, 다른 클리어 지점과 통일).
	OwnerASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Combat,
		NewStance == ERetrieveCombatStance::DrawnCombat ? 1 : 0);
	OwnerASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_WeaponSheathed,
		bSheathedNow ? 1 : 0);

	// 검이 실제 이동하는 '납검 경계'를 넘을 때만 소켓/연출 처리. (Relaxed↔Combat은 포즈 블렌드뿐)
	const bool bWasSheathed = (OldStance == ERetrieveCombatStance::Sheathed);
	if (bWasSheathed == bSheathedNow)
	{
		return;
	}

	// 새 전환은 진행 중 전환을 추월한다 → 이전 발검/납검 GA를 먼저 캔슬(스냅이든 몽타주든 깔끔히 교체).
	FGameplayTagContainer StanceTag;
	StanceTag.AddTag(RetrieveGameplayTags::Ability_Player_StanceTransition);
	OwnerASC->CancelAbilities(&StanceTag);

	const bool bDrawnNow = !bSheathedNow;

	if (bInstant)
	{
		// 연출 스킵 → 소켓만 즉시 스왑(공격發 발검 / 수영 입수 등). 몽타주 GA 미발동.
		if (UWeaponComponent* Weapon = GetOwner() ? GetOwner()->FindComponentByClass<UWeaponComponent>() : nullptr)
		{
			Weapon->SetWeaponDrawn(bDrawnNow);
		}
		return;
	}

	// 연출 있음 → 발검/납검 GA 트리거. 소켓 스왑은 그 몽타주의 노티가 SetWeaponDrawn으로 처리.
	const FGameplayTag EventTag = bDrawnNow
		? RetrieveGameplayTags::GameplayEvent_Player_DrawWeapon
		: RetrieveGameplayTags::GameplayEvent_Player_SheatheWeapon;

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetOwner();
	OwnerASC->HandleGameplayEvent(EventTag, &Payload);
}

void UCombatStanceComponent::HandleRelaxTimer()
{
	SetStance(ERetrieveCombatStance::DrawnRelaxed);;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SheatheTimerHandle, 
			this, &UCombatStanceComponent::HandleSheatheTimer, FMath::Max(0.01f, SheatheDelay), false);
	}
}

void UCombatStanceComponent::HandleSheatheTimer()
{
	SetStance(ERetrieveCombatStance::Sheathed);
}

void UCombatStanceComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
    // 공격류 발동만 '전투 활동'으로 친다 (점프/구르기 등은 무시). 공격 전이라면 발검 연출 스킵(즉시 손).
	if (Ability && Ability->GetAssetTags().HasTag(RetrieveGameplayTags::Ability_Type_Attack))
	{
		NotifyCombatActivity(/*bFromAttack=*/true);
	}
}

void UCombatStanceComponent::HandlePlayerSpotted(FGameplayTag Channel, const FEnemyPlayerSpottedPayload& Payload)
{
	// 적이 나를 포착 → 전투 태세 진입. 해제는 현재 자기 디케이(타이머)에 맡긴다.
	// (Lost 신호가 생기면 위협집합으로 정밀 해제 + 의심/교전 2티어로 확장 — 그때 분기)
	if (Payload.SpottedActor.Get() == GetOwner())
	{
		NotifyCombatActivity(/*bFromAttack=*/false);
	}
}
