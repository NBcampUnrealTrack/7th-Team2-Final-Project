#include "RetrieveGameplayTags.h"

namespace RetrieveGameplayTags
{
	// ---- Init State
	UE_DEFINE_GAMEPLAY_TAG(InitState_Spawned, "InitState.Spawned");
	UE_DEFINE_GAMEPLAY_TAG(InitState_DataAvailable, "InitState.DataAvailable");
	UE_DEFINE_GAMEPLAY_TAG(InitState_DataInitialized, "InitState.DataInitialized");
	UE_DEFINE_GAMEPLAY_TAG(InitState_GameplayReady, "InitState.GameplayReady");

	// ---- Input
	UE_DEFINE_GAMEPLAY_TAG(Input_Move, "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(Input_Look, "Input.Look");

	// ---- Player abilities
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Attack, "Ability.Player.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_HeavyAttack, "Ability.Player.HeavyAttack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Guard, "Ability.Player.Guard");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Parry, "Ability.Player.Parry");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Dash, "Ability.Player.Dash");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Jump, "Ability.Player.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Burst, "Ability.Player.Burst");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Absorb, "Ability.Player.Absorb");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Interact, "Ability.Player.Interact");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_LockOn, "Ability.Player.LockOn");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_UseItem, "Ability.Player.UseItem");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_SetElement_Fire, "Ability.Player.SetElement.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_SetElement_Water, "Ability.Player.SetElement.Water");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_SetElement_Wind, "Ability.Player.SetElement.Wind");

	// ---- Enemy / Boss abilities
	UE_DEFINE_GAMEPLAY_TAG(Ability_Enemy_Attack, "Ability.Enemy.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Enemy_SpecialAttack, "Ability.Enemy.SpecialAttack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Boss_PatternAttack, "Ability.Boss.PatternAttack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Boss_PhaseTransition, "Ability.Boss.PhaseTransition");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Boss_Groggy, "Ability.Boss.Groggy");

	// ---- Common abilities
	UE_DEFINE_GAMEPLAY_TAG(Ability_Common_Die, "Ability.Common.Die");

	// ---- Player state
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Normal, "State.Player.Normal");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Combat, "State.Player.Combat");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Dodging, "State.Player.Dodging");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Guarding, "State.Player.Guarding");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Parrying, "State.Player.Parrying");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Bursting, "State.Player.Bursting");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Staggered, "State.Player.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Knockdown, "State.Player.Knockdown");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Dead, "State.Player.Dead");

	// ---- Enemy state
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Idle, "State.Enemy.Idle");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Chase, "State.Enemy.Chase");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Attack, "State.Enemy.Attack");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_SpecialAttack, "State.Enemy.SpecialAttack");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Hit, "State.Enemy.Hit");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Staggered, "State.Enemy.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Groggy, "State.Enemy.Groggy");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Return, "State.Enemy.Return");
	UE_DEFINE_GAMEPLAY_TAG(State_Enemy_Dead, "State.Enemy.Dead");

	// ---- Boss state
	UE_DEFINE_GAMEPLAY_TAG(State_Boss_Intro, "State.Boss.Intro");
	UE_DEFINE_GAMEPLAY_TAG(State_Boss_Combat, "State.Boss.Combat");
	UE_DEFINE_GAMEPLAY_TAG(State_Boss_PatternAttack, "State.Boss.PatternAttack");
	UE_DEFINE_GAMEPLAY_TAG(State_Boss_PhaseTransition, "State.Boss.PhaseTransition");
	UE_DEFINE_GAMEPLAY_TAG(State_Boss_Groggy, "State.Boss.Groggy");
	UE_DEFINE_GAMEPLAY_TAG(State_Boss_Dead, "State.Boss.Dead");

	// ---- Combat micro-state
	UE_DEFINE_GAMEPLAY_TAG(State_Combo_Open, "State.Combo.Open");

	// ---- Gauge state
	UE_DEFINE_GAMEPLAY_TAG(State_Gauge_Full, "State.Gauge.Full");

	// ---- Elements
	UE_DEFINE_GAMEPLAY_TAG(Element_Fire, "Element.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Element_Water, "Element.Water");
	UE_DEFINE_GAMEPLAY_TAG(Element_Wind, "Element.Wind");
	UE_DEFINE_GAMEPLAY_TAG(Element_Corruption, "Element.Corruption");
	UE_DEFINE_GAMEPLAY_TAG(Element_None, "Element.None");

	// ---- Lock-on
	UE_DEFINE_GAMEPLAY_TAG(LockOn_Active, "LockOn.Active");

	// ---- Gameplay events: combat
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_ComboFinisher, "GameplayEvent.ComboFinisher");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Hit, "GameplayEvent.Hit");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Hit_Normal, "GameplayEvent.Hit.Normal");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Hit_Heavy, "GameplayEvent.Hit.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Parry_Success, "GameplayEvent.Parry.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Dodge_Success, "GameplayEvent.Dodge.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_DodgeCounter_Success, "GameplayEvent.DodgeCounter.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_WeakPointHit, "GameplayEvent.WeakPointHit");

	// ---- Gameplay events: boss / pattern counter
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Boss_PatternCast, "GameplayEvent.Boss.PatternCast");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Boss_PhaseTransition, "GameplayEvent.Boss.PhaseTransition");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_PatternCounterWindow, "GameplayEvent.PatternCounterWindow");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_PatternCountered, "GameplayEvent.PatternCountered");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_GroggyTrigger, "GameplayEvent.GroggyTrigger");

	// ---- Gameplay events: enemy combat / death
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_Attack, "GameplayEvent.Enemy.Attack");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack, "GameplayEvent.Enemy.SpecialAttack");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_Die, "GameplayEvent.Enemy.Die");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Boss_Die, "GameplayEvent.Boss.Die");

	// ---- Gameplay events: element / core / item
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Element_Unlock, "GameplayEvent.Element.Unlock");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Element_ModeChange, "GameplayEvent.Element.ModeChange");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Core_Drop, "GameplayEvent.Core.Drop");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Core_Absorb, "GameplayEvent.Core.Absorb");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Item_Used, "GameplayEvent.Item.Used");

	// ---- Input routing tags
	UE_DEFINE_GAMEPLAY_TAG(Input_UseItem_Slot1, "Input.UseItem.Slot1");
	UE_DEFINE_GAMEPLAY_TAG(Input_UseItem_Slot2, "Input.UseItem.Slot2");

	// ---- Message channels
	UE_DEFINE_GAMEPLAY_TAG(Channel_Combat_Hit, "Channel.Combat.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Combat_DamageDealt, "Channel.Combat.DamageDealt");
	UE_DEFINE_GAMEPLAY_TAG(Channel_ElementGauge_SlotChanged, "Channel.ElementGauge.SlotChanged");
	UE_DEFINE_GAMEPLAY_TAG(Channel_ElementGauge_Full, "Channel.ElementGauge.Full");
	UE_DEFINE_GAMEPLAY_TAG(Channel_ElementGauge_Burst, "Channel.ElementGauge.Burst");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Quest_GuardianDefeated, "Channel.Quest.GuardianDefeated");
	UE_DEFINE_GAMEPLAY_TAG(Channel_World_OutpostActivated, "Channel.World.OutpostActivated");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Player_Died, "Channel.Player.Died");
}
