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
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Attacking, "State.Player.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Dodging, "State.Player.Dodging");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Guarding, "State.Player.Guarding");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Parrying, "State.Player.Parrying");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Bursting, "State.Player.Bursting");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Cinematic, "State.Player.Cinematic");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Staggered, "State.Player.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Knockdown, "State.Player.Knockdown");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Dead, "State.Player.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_ForcedMove, "State.Player.ForcedMove");

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
	UE_DEFINE_GAMEPLAY_TAG(State_Boss_Staggered, "State.Boss.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(State_Boss_Dead, "State.Boss.Dead");

	// ---- Status state
	UE_DEFINE_GAMEPLAY_TAG(State_Status_Burn, "State.Status.Burn");
	UE_DEFINE_GAMEPLAY_TAG(State_Status_Cold, "State.Status.Cold");
	UE_DEFINE_GAMEPLAY_TAG(State_Status_Vulnerable, "State.Status.Vulnerable");
	
	// ---- Combat micro-state
	UE_DEFINE_GAMEPLAY_TAG(State_Combo_Open, "State.Combo.Open");

	// ---- Gauge state
	UE_DEFINE_GAMEPLAY_TAG(State_Gauge_Full, "State.Gauge.Full");
	
	// ---- Attack type
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type_Normal, "Attack.Type.Normal");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type_Heavy, "Attack.Type.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type_BossHeavy, "Attack.Type.BossHeavy");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type_Unblockable, "Attack.Type.Unblockable");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type_Parryable, "Attack.Type.Parryable");

	// ---- Elements
	UE_DEFINE_GAMEPLAY_TAG(Element_Fire, "Element.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Element_Water, "Element.Water");
	UE_DEFINE_GAMEPLAY_TAG(Element_Wind, "Element.Wind");
	UE_DEFINE_GAMEPLAY_TAG(Element_Corruption, "Element.Corruption");
	UE_DEFINE_GAMEPLAY_TAG(Element_None, "Element.None");

	// ---- Weapon
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Type_Unarmed, "Weapon.Type.Unarmed");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Type_SwordShield, "Weapon.Type.SwordShield");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Type_DualBlade, "Weapon.Type.DualBlade");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Type_Staff, "Weapon.Type.Staff");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Grade_Low, "Weapon.Grade.Low");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Grade_Mid, "Weapon.Grade.Mid");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Grade_High, "Weapon.Grade.High");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Affinity_Fire, "Weapon.Affinity.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Affinity_Water, "Weapon.Affinity.Water");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Affinity_Wind, "Weapon.Affinity.Wind");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Affinity_None, "Weapon.Affinity.None");

	// ---- Cosmetic
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Gender_Male, "Cosmetic.Gender.Male");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Gender_Female, "Cosmetic.Gender.Female");
	
	// ---- Lock-on
	UE_DEFINE_GAMEPLAY_TAG(LockOn_Active, "LockOn.Active");

	// ---- Animation locks
	UE_DEFINE_GAMEPLAY_TAG(Animation_Lock_Rotation, "Animation.Lock.Rotation");
	UE_DEFINE_GAMEPLAY_TAG(Animation_Lock_Movement, "Animation.Lock.Movement");

	// ---- Gameplay events: combat
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_ComboFinisher, "GameplayEvent.ComboFinisher");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Hit, "GameplayEvent.Hit");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Hit_Normal, "GameplayEvent.Hit.Normal");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Hit_Heavy, "GameplayEvent.Hit.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Parry_Success, "GameplayEvent.Parry.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Dodge_Success, "GameplayEvent.Dodge.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_DodgeCounter_Success, "GameplayEvent.DodgeCounter.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_WeakPointHit, "GameplayEvent.WeakPointHit");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_Impact, "GameplayEvent.Attack.Impact");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_Impact_Begin, "GameplayEvent_Attack_Impact_Begin");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_HitSuccess, "GameplayEvent.Attack.HitSuccess");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_HitSuccess_Light, "GameplayEvent.Attack.HitSuccess.Light");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_HitSuccess_Heavy, "GameplayEvent.Attack.HitSuccess.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_HitSuccess_WeakPoint, "GameplayEvent.Attack.HitSuccess.WeakPoint");

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

	// ---- Item
	UE_DEFINE_GAMEPLAY_TAG(Item_Weapon, "Item.Weapon");
	UE_DEFINE_GAMEPLAY_TAG(Item_Consumable, "Item.Consumable");
	UE_DEFINE_GAMEPLAY_TAG(Item_Material, "Item.Material");
	UE_DEFINE_GAMEPLAY_TAG(Item_Currency, "Item.Currency");
	UE_DEFINE_GAMEPLAY_TAG(Item_Consumable_Draught_Fire, "Item.Consumable.Draught.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Item_Consumable_Draught_Water, "Item.Consumable.Draught.Water");
	UE_DEFINE_GAMEPLAY_TAG(Item_Consumable_Draught_Wind, "Item.Consumable.Draught.Wind");
	UE_DEFINE_GAMEPLAY_TAG(Item_Material_EmptyFlask, "Item.Material.EmptyFlask");
	UE_DEFINE_GAMEPLAY_TAG(Item_Material_Fire, "Item.Material.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Item_Material_Water, "Item.Material.Water");
	UE_DEFINE_GAMEPLAY_TAG(Item_Material_Wind, "Item.Material.Wind");
	UE_DEFINE_GAMEPLAY_TAG(Item_Currency_Basic, "Item.Currency.Basic");

	// ---- Message channels
	UE_DEFINE_GAMEPLAY_TAG(Channel_Session_StateChanged, "Channel.Session.StateChanged");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Combat_Hit, "Channel.Combat.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Combat_DamageDealt, "Channel.Combat.DamageDealt");
	UE_DEFINE_GAMEPLAY_TAG(Channel_ElementGauge_SlotChanged, "Channel.ElementGauge.SlotChanged");
	UE_DEFINE_GAMEPLAY_TAG(Channel_ElementGauge_Full, "Channel.ElementGauge.Full");
	UE_DEFINE_GAMEPLAY_TAG(Channel_ElementGauge_Burst, "Channel.ElementGauge.Burst");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Quest_GuardianDefeated, "Channel.Quest.GuardianDefeated");
	UE_DEFINE_GAMEPLAY_TAG(Channel_World_OutpostActivated, "Channel.World.OutpostActivated");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Player_Died, "Channel.Player.Died");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Monster_Died, "Channel.Monster.Died");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Game_QueenDefeated, "Channel.Game.QueenDefeated");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Enemy_PlayerSpotted, "Channel.Enemy.PlayerSpotted");
	
	// ---- Data
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_Mul, "Data.Damage.Mul");
	UE_DEFINE_GAMEPLAY_TAG(Data_Init_MaxHealth, "Data.Init.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(Data_Init_Health, "Data.Init.Health");
	UE_DEFINE_GAMEPLAY_TAG(Data_Init_AttackPower, "Data.Init.AttackPower");
	UE_DEFINE_GAMEPLAY_TAG(Data_Init_MoveSpeed, "Data.Init.MoveSpeed");
	UE_DEFINE_GAMEPLAY_TAG(Data_Init_IncomingDamageMultiplier, "Data.Init.IncomingDamageMultiplier");
	/*
	// ---- Hit react
	UE_DEFINE_GAMEPLAY_TAG(Hit_React_Stagger, "Hit.React.Stagger");

	// ---- Damage context
	UE_DEFINE_GAMEPLAY_TAG(Damage_Source_Player, "Damage.Source.Player");

	// ---- Gauge state
	UE_DEFINE_GAMEPLAY_TAG(State_Gauge_Empty, "State.Gauge.Empty");
	UE_DEFINE_GAMEPLAY_TAG(State_Gauge_SlotReady, "State.Gauge.SlotReady");

	// ---- Gameplay events: combat
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_HeavyAttack_Launched, "GameplayEvent.HeavyAttack.Launched");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Guard_Success, "GameplayEvent.Guard.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Guard_Break, "GameplayEvent.Guard.Break");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Parry_Fail, "GameplayEvent.Parry.Fail");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Hit_Knockdown, "GameplayEvent.Hit.Knockdown");
	*/

	// ---- UI VFX
	UE_DEFINE_GAMEPLAY_TAG(UI_VFX_Panel_Open, "UI.VFX.Panel.Open");
	UE_DEFINE_GAMEPLAY_TAG(UI_VFX_Panel_Close, "UI.VFX.Panel.Close");
	UE_DEFINE_GAMEPLAY_TAG(UI_VFX_Gauge_FullPulse, "UI.VFX.Gauge.FullPulse");
	UE_DEFINE_GAMEPLAY_TAG(UI_VFX_Icon_ItemAdded, "UI.VFX.Icon.ItemAdded");
	UE_DEFINE_GAMEPLAY_TAG(UI_VFX_Button_Hover, "UI.VFX.Button.Hover");
	UE_DEFINE_GAMEPLAY_TAG(UI_VFX_Button_Unhover, "UI.VFX.Button.Unhover");
	UE_DEFINE_GAMEPLAY_TAG(UI_VFX_Button_Press, "UI.VFX.Button.Press");
	UE_DEFINE_GAMEPLAY_TAG(UI_VFX_Button_Release, "UI.VFX.Button.Release");
	UE_DEFINE_GAMEPLAY_TAG(UI_VFX_Tab_Switch, "UI.VFX.Tab.Switch");
}
