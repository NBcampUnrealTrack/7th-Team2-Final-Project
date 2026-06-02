#include "AbilitySystem/Player/GA_RecallLumen.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"

UGA_RecallLumen::UGA_RecallLumen()
{
	AbilityTags.AddTag(RetrieveGameplayTags::Ability_Player_RecallLumen);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Cinematic);

	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationPolicy = ERetrieveAbilityActivationPolicy::OnInputTriggered;
}

void UGA_RecallLumen::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                      const FGameplayAbilityActorInfo* ActorInfo,
                                      const FGameplayAbilityActivationInfo ActivationInfo,
                                      const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (UWorld* World = GetWorld())
	{
		FRetrieveLumenCommandPayload Message;
		Message.CommandTag = RetrieveGameplayTags::Channel_Lumen_Command_Recall;
		Message.Instigator = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RetrieveGameplayTags::Channel_Lumen_Command_Recall, Message);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
