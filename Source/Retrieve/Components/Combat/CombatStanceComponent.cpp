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
	SetStance(ERetrieveCombatStance::DrawnCombat, /*bInstant=*/bFromAttack); // 전투 태세 승격(적 포착 전용 진입점)

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SheatheTimerHandle);
		World->GetTimerManager().SetTimer(RelaxTimerHandle, this,
			&UCombatStanceComponent::HandleRelaxTimer, FMath::Max(0.01f,RelaxDelay), false);
	}
}

void UCombatStanceComponent::NotifyDrawnActivity(bool bInstant)
{
	// 납검 → 발검+평상. 이미 발검(Relaxed/Combat)이면 스탠스는 건드리지 않는다(Combat 강등 방지).
	if (CurrentStance == ERetrieveCombatStance::Sheathed)
	{
		SetStance(ERetrieveCombatStance::DrawnRelaxed, bInstant);
	}

	// Relaxed일 때만 납검 디케이 재무장. Combat 중엔 기존 Relax→Sheathe 디케이에 맡긴다.
	if (CurrentStance == ERetrieveCombatStance::DrawnRelaxed)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(SheatheTimerHandle, this,
				&UCombatStanceComponent::HandleSheatheTimer, FMath::Max(0.01f, SheatheDelay), false);
		}
	}
}

void UCombatStanceComponent::ForceSheatheWeapon()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RelaxTimerHandle);
		World->GetTimerManager().ClearTimer(SheatheTimerHandle);
	}

	SetStance(ERetrieveCombatStance::Sheathed, /*bInstant=*/true);
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

	// 이미 장착된 무기는 등 소켓으로 맞춰둔다(init 시점에 이미 장착돼 있던 경우).
	// 그리고 이후 런타임 장착(인벤토리)은 OnWeaponVisualsSpawned을 구독해 스폰 직후 현재 스탠스로 동기화한다.
	if (UWeaponComponent* Weapon = GetOwner() ? GetOwner()->FindComponentByClass<UWeaponComponent>() : nullptr)
	{
		Weapon->SetWeaponDrawn(false);
		WeaponVisualsSpawnedHandle = Weapon->OnWeaponVisualsSpawned.AddUObject(
			this, &UCombatStanceComponent::HandleWeaponVisualsSpawned);

		// 장착 = 발검 활동. 스폰 '전'에 발화하는 OnWeaponEquipped에서 스탠스를 미리 발검으로 올린다
		// → 직후 HandleWeaponVisualsSpawned가 손 소켓에 안착(깜빡임 없음).
		Weapon->OnWeaponEquipped.AddDynamic(this, &UCombatStanceComponent::HandleWeaponEquipped);
	}
}

void UCombatStanceComponent::UninitializeFromAbilitySystem()
{
	if (IsValid(OwnerASC) && AbilityActivateHandle.IsValid())
	{
		OwnerASC->AbilityActivatedCallbacks.Remove(AbilityActivateHandle);
	}
	AbilityActivateHandle.Reset();

	if (WeaponVisualsSpawnedHandle.IsValid())
	{
		if (UWeaponComponent* Weapon = GetOwner() ? GetOwner()->FindComponentByClass<UWeaponComponent>() : nullptr)
		{
			Weapon->OnWeaponVisualsSpawned.Remove(WeaponVisualsSpawnedHandle);
			Weapon->OnWeaponEquipped.RemoveDynamic(this, &UCombatStanceComponent::HandleWeaponEquipped);
		}
		WeaponVisualsSpawnedHandle.Reset();
	}

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

bool UCombatStanceComponent::IsPlayerAttacking() const
{
	return IsValid(OwnerASC)
		&& OwnerASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Attacking);
}

bool UCombatStanceComponent::IsAiming() const
{
	return IsValid(OwnerASC)
		&& OwnerASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Aiming);
}

void UCombatStanceComponent::HandleRelaxTimer()
{
	if (IsPlayerAttacking())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RelaxTimerHandle,
				this, &UCombatStanceComponent::HandleRelaxTimer, FMath::Max(0.01f, RelaxDelay), false);
		}
		return;
	}

	SetStance(ERetrieveCombatStance::DrawnRelaxed);;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SheatheTimerHandle, 
			this, &UCombatStanceComponent::HandleSheatheTimer, FMath::Max(0.01f, SheatheDelay), false);
	}
}

void UCombatStanceComponent::HandleSheatheTimer()
{
	// 공격/조준 지속 중이면 납검만 보류(Relaxed 유지, Combat 승격 없음 — Combat은 적 포착 전용).
	// 스탠스는 그대로 두고(손 유지) 타이머만 재무장해 종료를 기다린다.
	if (IsPlayerAttacking() || IsAiming())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(SheatheTimerHandle,
				this, &UCombatStanceComponent::HandleSheatheTimer, FMath::Max(0.01f, SheatheDelay), false);
		}
		return;
	}

	SetStance(ERetrieveCombatStance::Sheathed);
}

void UCombatStanceComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
    // 공격류 발동만 '전투 활동'으로 친다 (점프/구르기 등은 무시).
	if (!Ability || !Ability->GetAssetTags().HasTag(RetrieveGameplayTags::Ability_Type_Attack))
	{
		return;
	}

	// 평타(일반 콤보) + 납검 → 발검은 GA_Attack이 ActivateAbility에서 직접 처리한다(스윙 스킵 + 발검 트리거).
	// 이 콜백은 PreActivate에서 ActivateAbility보다 '먼저' 실행되므로
	// (엔진: CallActivateAbility → PreActivate → ActivateAbility),
	// 여기서 스탠스를 바꾸면 납검 태그가 먼저 지워져 GA_Attack의 발검 게이트가 깨진다 → 건드리지 않는다.
	const bool bIsNormalCombo = Ability->GetAssetTags().HasTag(RetrieveGameplayTags::Ability_Player_Attack);
	const bool bSheathed = IsValid(OwnerASC) && OwnerASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_WeaponSheathed);
	if (bIsNormalCombo && bSheathed)
	{
		return;
	}

	// 강/대시/점프, 또는 이미 발검 상태의 평타 → 즉시 발검(Relaxed) + 스윙. Combat 승격은 적 포착 전용.
	NotifyDrawnActivity(/*bInstant=*/true);
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

void UCombatStanceComponent::HandleWeaponVisualsSpawned()
{
	// Equip 전환 중이면 소켓 배치/표시를 그 몽타주 노티에 위임한다(검→방패 순차 등장).
	// 스폰은 hidden 상태이므로, 여기서 건드리지 않으면 발검 노티가 부착+visible 할 때까지 숨어 있다.
	if (IsValid(OwnerASC) && OwnerASC->HasMatchingGameplayTag(RetrieveGameplayTags::Ability_Player_EquipTransition))
	{
		return;
	}

	// 새로 스폰된 무기를 현재 스탠스에 맞춘다. 납검이면 등, 발검이면 손.
	// (스폰은 항상 손 소켓 기본이므로, 납검 중 런타임 장착 시 여기서 등으로 보낸다)
	if (UWeaponComponent* Weapon = GetOwner() ? GetOwner()->FindComponentByClass<UWeaponComponent>() : nullptr)
	{
		Weapon->SetWeaponDrawn(CurrentStance != ERetrieveCombatStance::Sheathed);
	}
}

void UCombatStanceComponent::HandleWeaponEquipped(FName /*WeaponItemId*/)
{
	// 장착 = 발검 활동(Relaxed). 연출은 Equip 몽타주가 담당하므로 상태/타이머만 올린다.
	// bInstant=true → 발검(Draw) 몽타주는 스킵(Equip 몽타주와 이중 재생 방지).
	NotifyDrawnActivity(/*bInstant=*/true);
}
