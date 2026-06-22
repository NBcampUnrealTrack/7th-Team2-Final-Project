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
	UE_DEFINE_GAMEPLAY_TAG(Input_Sprint, "Input.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Input_Crouch, "Input.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Input_Dash, "Input.Dash");
	UE_DEFINE_GAMEPLAY_TAG(Input_Zoom, "Input.Zoom");

	// ---- Player abilities
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Attack, "Ability.Player.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_HeavyAttack, "Ability.Player.HeavyAttack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_HeavyAttack_Fire, "Ability.Player.HeavyAttack.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_HeavyAttack_Water, "Ability.Player.HeavyAttack.Water");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_HeavyAttack_Wind, "Ability.Player.HeavyAttack.Wind");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_HeavyAttack_Staff, "Ability.Player.HeavyAttack.Staff");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Guard, "Ability.Player.Guard");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Parry, "Ability.Player.Parry");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Dash, "Ability.Player.Dash");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Blink, "Ability.Player.Blink");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Jump, "Ability.Player.Jump");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_JumpAttack, "Ability.Player.JumpAttack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Burst, "Ability.Player.Burst");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Absorb, "Ability.Player.Absorb");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_Interact, "Ability.Player.Interact");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_LockOn, "Ability.Player.LockOn");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_UseItem, "Ability.Player.UseItem");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_SetElement_Fire, "Ability.Player.SetElement.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_SetElement_Water, "Ability.Player.SetElement.Water");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_SetElement_Wind, "Ability.Player.SetElement.Wind");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_RecallLumen, "Ability.Player.RecallLumen");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_SprintAttack, "Ability.Player.SprintAttack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Player_ParryCounter, "Ability.Player.ParryCounter");

	// ---- Ability families
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Attack, "Ability.Type.Attack");

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
	UE_DEFINE_GAMEPLAY_TAG(State_Player_CanCounter, "State.Player.CanCounter");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_UsingHeavyAttack, "State.Player.UsingHeavyAttack");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Bursting, "State.Player.Bursting");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Cinematic, "State.Player.Cinematic");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Staggered, "State.Player.Staggered");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Knockdown, "State.Player.Knockdown");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Dead, "State.Player.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_ForcedMove, "State.Player.ForcedMove");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Sprinting, "State.Player.Sprinting");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Crouching, "State.Player.Crouching");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Swimming, "State.Player.Swimming");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Swimming_UnderWater, "State.Player.Swimming.UnderWater");
	
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Modal_Dialogue, "State.Player.Modal.Dialogue");
	UE_DEFINE_GAMEPLAY_TAG(State_Player_Modal_Cinematic, "State.Player.Modal.Cinematic");

	// ---- Cooldown
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Player_Parry, "Cooldown.Player.Parry");

	// ---- Action state
	UE_DEFINE_GAMEPLAY_TAG(State_Action_HighMobility, "State.Action.HighMobility");

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

	// ---- Immune
	UE_DEFINE_GAMEPLAY_TAG(State_Immune_Knockback, "State.Immune.Knockback");

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
	UE_DEFINE_GAMEPLAY_TAG(State_Status_ColdReaction, "State.Status.ColdReaction");
	UE_DEFINE_GAMEPLAY_TAG(State_Status_Vulnerable, "State.Status.Vulnerable");
	
	// ---- Combat micro-state
	UE_DEFINE_GAMEPLAY_TAG(State_Combo_Open, "State.Combo.Open");

	// ---- Gauge state
	UE_DEFINE_GAMEPLAY_TAG(State_Gauge_Full, "State.Gauge.Full");
	
	// ---- Attack type
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type, "Attack.Type");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type_Normal, "Attack.Type.Normal");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type_Heavy, "Attack.Type.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type_BossHeavy, "Attack.Type.BossHeavy");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type_Unblockable, "Attack.Type.Unblockable");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Type_Parryable, "Attack.Type.Parryable");

	// ---- Attack property
	UE_DEFINE_GAMEPLAY_TAG(Attack_Property, "Attack.Property");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Property_GuardBreak, "Attack.Property.GuardBreak");

	// ---- Hit react type
	UE_DEFINE_GAMEPLAY_TAG(HitReact_Type, "HitReact.Type");
	UE_DEFINE_GAMEPLAY_TAG(HitReact_Type_Flinch, "HitReact.Type.Flinch");
	UE_DEFINE_GAMEPLAY_TAG(HitReact_Type_Stagger, "HitReact.Type.Stagger");
	UE_DEFINE_GAMEPLAY_TAG(HitReact_Type_Knockdown, "HitReact.Type.Knockdown");

	// ---- Elements
	UE_DEFINE_GAMEPLAY_TAG(Element, "Element");
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

	// ---- Equipment
	UE_DEFINE_GAMEPLAY_TAG(Equipment_Slot_Head, "Equipment.Slot.Head");
	UE_DEFINE_GAMEPLAY_TAG(Equipment_Slot_Chest, "Equipment.Slot.Chest");
	UE_DEFINE_GAMEPLAY_TAG(Equipment_Slot_Hands, "Equipment.Slot.Hands");
	UE_DEFINE_GAMEPLAY_TAG(Equipment_Slot_Legs, "Equipment.Slot.Legs");
	UE_DEFINE_GAMEPLAY_TAG(Equipment_Slot_Feet, "Equipment.Slot.Feet");

	// ---- Cosmetic
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Gender_Male, "Cosmetic.Gender.Male");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Gender_Female, "Cosmetic.Gender.Female");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Head, "Cosmetic.Part.Head");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Torso, "Cosmetic.Part.Torso");

	// ---- Cosmetic Part (granular, Sidekick PartType 단위 — modular spawn slot)
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Hair, "Cosmetic.Part.Hair");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Eyebrow_Left, "Cosmetic.Part.Eyebrow.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Eyebrow_Right, "Cosmetic.Part.Eyebrow.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Eye_Left, "Cosmetic.Part.Eye.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Eye_Right, "Cosmetic.Part.Eye.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Ear_Left, "Cosmetic.Part.Ear.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Ear_Right, "Cosmetic.Part.Ear.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_FacialHair, "Cosmetic.Part.FacialHair");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Arm_Upper_Left, "Cosmetic.Part.Arm.Upper.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Arm_Upper_Right, "Cosmetic.Part.Arm.Upper.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Arm_Lower_Left, "Cosmetic.Part.Arm.Lower.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Arm_Lower_Right, "Cosmetic.Part.Arm.Lower.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Hand_Left, "Cosmetic.Part.Hand.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Hand_Right, "Cosmetic.Part.Hand.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Hips, "Cosmetic.Part.Hips");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Leg_Left, "Cosmetic.Part.Leg.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Leg_Right, "Cosmetic.Part.Leg.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Foot_Left, "Cosmetic.Part.Foot.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Foot_Right, "Cosmetic.Part.Foot.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Head, "Cosmetic.Part.Attachment.Head");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Face, "Cosmetic.Part.Attachment.Face");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Back, "Cosmetic.Part.Attachment.Back");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Hips_Front, "Cosmetic.Part.Attachment.Hips.Front");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Hips_Back, "Cosmetic.Part.Attachment.Hips.Back");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Hips_Left, "Cosmetic.Part.Attachment.Hips.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Hips_Right, "Cosmetic.Part.Attachment.Hips.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Shoulder_Left, "Cosmetic.Part.Attachment.Shoulder.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Shoulder_Right, "Cosmetic.Part.Attachment.Shoulder.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Elbow_Left, "Cosmetic.Part.Attachment.Elbow.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Elbow_Right, "Cosmetic.Part.Attachment.Elbow.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Knee_Left, "Cosmetic.Part.Attachment.Knee.Left");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Attachment_Knee_Right, "Cosmetic.Part.Attachment.Knee.Right");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Nose, "Cosmetic.Part.Nose");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Teeth, "Cosmetic.Part.Teeth");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Tongue, "Cosmetic.Part.Tongue");
	UE_DEFINE_GAMEPLAY_TAG(Cosmetic_Part_Wrap, "Cosmetic.Part.Wrap");

	// ---- Lock-on
	UE_DEFINE_GAMEPLAY_TAG(LockOn_Active, "LockOn.Active");

	// ---- Animation locks
	UE_DEFINE_GAMEPLAY_TAG(Animation_Lock_Rotation, "Animation.Lock.Rotation");
	UE_DEFINE_GAMEPLAY_TAG(Animation_Lock_Movement, "Animation.Lock.Movement");
	
	// ---- ALS Locomotion Actions
	UE_DEFINE_GAMEPLAY_TAG(LocomotionAction_Attack, "Als.LocomotionAction.Attack");
	
	// ---- Gameplay events: combat
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_ComboFinisher, "GameplayEvent.Combo.Finisher");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Hit, "GameplayEvent.Hit");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Hit_Normal, "GameplayEvent.Hit.Normal");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Hit_Heavy, "GameplayEvent.Hit.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_Hit, "GameplayEvent.Attack.Hit");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Parry_Success, "GameplayEvent.Parry.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Parried, "GameplayEvent.Parried");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Guard_Success, "GameplayEvent.Guard.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Guard_Broken, "GameplayEvent.Guard.Broken");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Dodge_Success, "GameplayEvent.Dodge.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_DodgeCounter_Success, "GameplayEvent.DodgeCounter.Success");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_WeakPointHit, "GameplayEvent.WeakPointHit");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_Impact, "GameplayEvent.Attack.Impact");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_Impact_Begin, "GameplayEvent.Attack.Impact.Begin");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_HitSuccess, "GameplayEvent.Attack.HitSuccess");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_HitSuccess_Light, "GameplayEvent.Attack.HitSuccess.Light");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_HitSuccess_Heavy, "GameplayEvent.Attack.HitSuccess.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_HitSuccess_Burst, "GameplayEvent.Attack.HitSuccess.Burst");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Attack_HitSuccess_WeakPoint, "GameplayEvent.Attack.HitSuccess.WeakPoint");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Reaction_Finished, "GameplayEvent.Reaction.Finished");

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

	// ---- Gameplay events: enemy combat : special attack
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_Projectile, "GameplayEvent.Enemy.SpecialAttack.Projectile");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_ProjectileRapid, "GameplayEvent.Enemy.SpecialAttack.ProjectileRapid");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_ProjectileRain, "GameplayEvent.Enemy.SpecialAttack.ProjectileRain");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_ProjectileHoming, "GameplayEvent.Enemy.SpecialAttack.ProjectileHoming");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_ProjectileSpread, "GameplayEvent.Enemy.SpecialAttack.ProjectileSpread");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_Acceleration, "GameplayEvent.Enemy.SpecialAttack.Acceleration");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_DashAttack, "GameplayEvent.Enemy.SpecialAttack.DashAttack");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_AerialDive, "GameplayEvent.Enemy.SpecialAttack.AerialDive");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_GroundHazard, "GameplayEvent.Enemy.SpecialAttack.GroundHazard");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_GroundHazards, "GameplayEvent.Enemy.SpecialAttack.GroundHazards");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_WaterPillar, "GameplayEvent.Enemy.SpecialAttack.WaterPillar");
	UE_DEFINE_GAMEPLAY_TAG(GameplayEvent_Enemy_SpecialAttack_IcePillar, "GameplayEvent.Enemy.SpecialAttack.IcePillar");

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
	UE_DEFINE_GAMEPLAY_TAG(Item_Armor, "Item.Armor");
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

	// ---- UI Buff
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff, "UI.Buff");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Item, "UI.Buff.Item");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Item_FireBoost, "UI.Buff.Item.FireBoost");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Item_WaterBoost, "UI.Buff.Item.WaterBoost");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Item_WindBoost, "UI.Buff.Item.WindBoost");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Burst, "UI.Buff.Burst");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Burst_FireSlash, "UI.Buff.Burst.FireSlash");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Burst_WaterVortex, "UI.Buff.Burst.WaterVortex");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Burst_WindSlash, "UI.Buff.Burst.WindSlash");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Absorb, "UI.Buff.Absorb");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Absorb_Fire, "UI.Buff.Absorb.Fire");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Absorb_Water, "UI.Buff.Absorb.Water");
	UE_DEFINE_GAMEPLAY_TAG(UI_Buff_Absorb_Wind, "UI.Buff.Absorb.Wind");

	// ---- Message channels
	UE_DEFINE_GAMEPLAY_TAG(Channel_Session_StateChanged, "Channel.Session.StateChanged");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Combat_Hit, "Channel.Combat.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Combat_DamageDealt, "Channel.Combat.DamageDealt");
	UE_DEFINE_GAMEPLAY_TAG(Channel_ElementGauge_SlotChanged, "Channel.ElementGauge.SlotChanged");
	UE_DEFINE_GAMEPLAY_TAG(Channel_ElementGauge_Full, "Channel.ElementGauge.Full");
	UE_DEFINE_GAMEPLAY_TAG(Channel_ElementGauge_Burst, "Channel.ElementGauge.Burst");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Item_ElementBuff, "Channel.Item.ElementBuff");
	UE_DEFINE_GAMEPLAY_TAG(Channel_World_OutpostActivated, "Channel.World.OutpostActivated");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Player_Died, "Channel.Player.Died");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Monster_Died, "Channel.Monster.Died");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Game_QueenDefeated, "Channel.Game.QueenDefeated");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Enemy_PlayerSpotted, "Channel.Enemy.PlayerSpotted");
	
	// ---- Message channels: UI Buff
	UE_DEFINE_GAMEPLAY_TAG(Channel_UI_Buff_Apply,   "Channel.UI.Buff.Apply");
	UE_DEFINE_GAMEPLAY_TAG(Channel_UI_Buff_Remove,  "Channel.UI.Buff.Remove");

	// ---- Message channels: UI Skill popup
	UE_DEFINE_GAMEPLAY_TAG(Channel_UI_SkillActivated, "Channel.UI.SkillActivated");

	// ---- Message channels: Quest
	UE_DEFINE_GAMEPLAY_TAG(Channel_Quest_GuardianDefeated, "Channel.Quest.GuardianDefeated");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Quest_StepChanged, "Channel.Quest.StepChanged");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Quest_SealUnlocked, "Channel.Quest.SealUnlocked");
	
	// ---- Message channels: Lumen command
	UE_DEFINE_GAMEPLAY_TAG(Channel_Lumen_Command_ToggleWait, "Channel.Lumen.Command.ToggleWait");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Lumen_Command_Recall, "Channel.Lumen.Command.Recall");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Lumen_Mode_Changed, "Channel.Lumen.Mode.Changed");
	
	// ---- Message channels: Dialogue / Cinematic
	UE_DEFINE_GAMEPLAY_TAG(Channel_Dialogue_LineRequested, "Channel.Dialogue.LineRequested");
	UE_DEFINE_GAMEPLAY_TAG(Channel_Cinematic_Changed, "Channel.Cinematic.Changed");
	
	// ---- Data
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage_Mul, "Data.Damage.Mul");
	UE_DEFINE_GAMEPLAY_TAG(Data_Knockback_Strength, "Data.Knockback.Strength");
	UE_DEFINE_GAMEPLAY_TAG(Data_Knockback_UpwardStrength, "Data.Knockback.UpwardStrength");
	UE_DEFINE_GAMEPLAY_TAG(Data_Init_MaxHealth, "Data.Init.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(Data_Init_Health, "Data.Init.Health");
	UE_DEFINE_GAMEPLAY_TAG(Data_Init_AttackPower, "Data.Init.AttackPower");
	UE_DEFINE_GAMEPLAY_TAG(Data_Init_MoveSpeed, "Data.Init.MoveSpeed");
	UE_DEFINE_GAMEPLAY_TAG(Data_Init_IncomingDamageMultiplier, "Data.Init.IncomingDamageMultiplier");
	UE_DEFINE_GAMEPLAY_TAG(Data_Weapon_AttackPower, "Data.Weapon.AttackPower");
	UE_DEFINE_GAMEPLAY_TAG(Data_Armor_Defense, "Data.Armor.Defense");

	// ---- Gameplay cues: Heavy attack
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_HeavyAttack_Fire, "GameplayCue.HeavyAttack.Fire");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_HeavyAttack_Water, "GameplayCue.HeavyAttack.Water");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_HeavyAttack_Water_Active, "GameplayCue.HeavyAttack.Water.Active");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_HeavyAttack_Wind, "GameplayCue.HeavyAttack.Wind");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_HeavyAttack_NoSlot, "GameplayCue.HeavyAttack.NoSlot");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Sigil_Burst, "GameplayCue.Sigil.Burst");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Sigil_Fire, "GameplayCue.Sigil.Fire");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Sigil_Wind, "GameplayCue.Sigil.Wind");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Sigil_Water, "GameplayCue.Sigil.Water");

	// ---- Gameplay cues: Staff
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Staff_Cast, "GameplayCue.Staff.Cast");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Staff_Impact, "GameplayCue.Staff.Impact");

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
	
	// ---- Quest steps
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_Awakening, "Quest.Step.Awakening");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_SigilCompleted, "Quest.Step.SigilCompleted");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_FireGuardianDefeated, "Quest.Step.FireGuardianDefeated");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_WindGuardianDefeated, "Quest.Step.WindGuardianDefeated");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_WaterGuardianDefeated, "Quest.Step.WaterGuardianDefeated");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_FireSigilActivated, "Quest.Step.FireSigilActivated");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_WindSigilActivated, "Quest.Step.WindSigilActivated");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_WaterSigilActivated, "Quest.Step.WaterSigilActivated");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_SealUnlocked, "Quest.Step.SealUnlocked");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_SealOpened, "Quest.Step.SealOpened");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_QueenDefeated, "Quest.Step.QueenDefeated");
	UE_DEFINE_GAMEPLAY_TAG(Quest_Step_GameComplete, "Quest.Step.GameComplete");
	
	// ---- Dialogue
	UE_DEFINE_GAMEPLAY_TAG(Speaker_Lumen, "Speaker.Lumen");
	UE_DEFINE_GAMEPLAY_TAG(Topic_Lumen_Imprint, "Topic.Lumen.Imprint");
	UE_DEFINE_GAMEPLAY_TAG(Topic_Lumen_About, "Topic.Lumen.About");
	UE_DEFINE_GAMEPLAY_TAG(Topic_Lumen_FollowWait, "Topic.Lumen.FollowWait");
	UE_DEFINE_GAMEPLAY_TAG(Topic_Lumen_FireSigil, "Topic.Lumen.FireSigil");
	UE_DEFINE_GAMEPLAY_TAG(Topic_Lumen_WindSigil, "Topic.Lumen.WindSigil");
	UE_DEFINE_GAMEPLAY_TAG(Topic_Lumen_WaterSigil, "Topic.Lumen.WaterSigil");
	
	// --- Monster Types
	UE_DEFINE_GAMEPLAY_TAG(Monster_Type_Normal, "Monster.Type.Normal");
	UE_DEFINE_GAMEPLAY_TAG(Monster_Type_Epic, "Monster.Type.Epic");
	UE_DEFINE_GAMEPLAY_TAG(Monster_Type_Boss, "Monster.Type.Boss");
}
