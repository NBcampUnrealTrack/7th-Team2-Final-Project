#include "AbilitySystem/Player/GA_Attack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Animation/RetrieveWeaponSockets.h"
#include "Components/WeaponComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_Attack::UGA_Attack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_Attack);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dead);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Staggered);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Knockdown);
	ActivationBlockedTags.AddTag(RetrieveGameplayTags::State_Player_Dodging);
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::State_Player_Attacking);
}

bool UGA_Attack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWeaponComponent* WeaponComp = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(WeaponComp) || !WeaponComp->IsEquipped())
	{
		return false;
	}
	
	if (WeaponComp->GetWeaponDataRef().ComboSteps.IsEmpty())
	{
		return false;
	}
	
	return IsValid(DamageEffectClass);
}

void UGA_Attack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	CachedWeaponComponent = AvatarActor ? AvatarActor->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (!IsValid(CachedWeaponComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CachedWeaponData = CachedWeaponComponent->GetWeaponDataRef();

	ImpactBeginEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RetrieveGameplayTags::GameplayEvent_Attack_Impact_Begin, nullptr, false, true);
	if (ImpactBeginEventTask)
	{
		ImpactBeginEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleImpactBeginEvent);
		ImpactBeginEventTask->ReadyForActivation();
	}
	
	ImpactEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, RetrieveGameplayTags::GameplayEvent_Attack_Impact, nullptr, false, true);
	if (ImpactEventTask)
	{
		ImpactEventTask->EventReceived.AddDynamic(this, &ThisClass::HandleImpactEvent);
		ImpactEventTask->ReadyForActivation();
	}

	StartComboStep(0);
}

void UGA_Attack::StartComboStep(int32 StepIndex)
{
	if (!CachedWeaponData.ComboSteps.IsValidIndex(StepIndex))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	CurrentComboIndex = StepIndex;
	bComboQueued = false;
	
	HitActorsThisStep.Reset();
	bHasValidPreviousTraceOrigin = false;

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	const FWeaponComboStep& StepData = CachedWeaponData.ComboSteps[CurrentComboIndex];
	UAnimMontage* Montage = StepData.Montage.LoadSynchronous();
	if (!IsValid(Montage))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.f, StepData.SectionName, true);
	if (!MontageTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	MontageTask->ReadyForActivation();
	
	StartListeningComboInput();
}

void UGA_Attack::StartListeningComboInput()
{
	if (InputPressTask)
	{
		InputPressTask->EndTask();
		InputPressTask = nullptr;
	}

	InputPressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	if (InputPressTask)
	{
		InputPressTask->OnPress.AddDynamic(this, &ThisClass::HandleInputPressed);
		InputPressTask->ReadyForActivation();
	}
}

void UGA_Attack::HandleInputPressed(float TimeWaited)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		return;
	}
	
	const bool bCanChain = CachedWeaponData.ComboSteps.IsValidIndex(CurrentComboIndex + 1);
	const bool bWindowOpen = ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Combo_Open);

	if (bCanChain && bWindowOpen)
	{
		bComboQueued = true;
		return;
	}

	StartListeningComboInput();
}

void UGA_Attack::HandleImpactBeginEvent(FGameplayEventData Payload)
{
	bHasValidPreviousTraceOrigin = false;
	HitActorsThisStep.Reset();
}

void UGA_Attack::HandleImpactEvent(FGameplayEventData Payload)
{
	if (!IsActive() || CurrentComboIndex == INDEX_NONE)
	{
		return;
	}
	ApplyStepDamage();
}

void UGA_Attack::ApplyStepDamage()
{
	if (!HasAuthority(&GetCurrentActivationInfoRef()))
	{
		return;
	}
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(SourceASC) || !IsValid(AvatarActor) || !IsValid(DamageEffectClass))
	{
		return;
	}
	
	const float DamageMul = CachedWeaponData.ComboSteps.IsValidIndex(CurrentComboIndex)
		? CachedWeaponData.ComboSteps[CurrentComboIndex].DamageMultiplier : 1.0f;

	UMeshComponent* TraceMesh = IsValid(CachedWeaponComponent) ? CachedWeaponComponent->GetPrimaryEquippedWeaponMesh() : nullptr;
	FName TraceSocket = CachedWeaponData.TraceSocketName.IsNone() ? RetrieveWeaponSockets::Weapon_R : CachedWeaponData.TraceSocketName;

	FVector CurrentTraceOrigin = AvatarActor->GetActorLocation();
	if (IsValid(TraceMesh) && TraceMesh->DoesSocketExist(TraceSocket))
	{
		CurrentTraceOrigin = TraceMesh->GetSocketLocation(TraceSocket);
	}

	UWorld* World = AvatarActor->GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	
	const FVector SweepStart = bHasValidPreviousTraceOrigin ? PreviousTraceOrigin : CurrentTraceOrigin;
	
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GA_Attack_Impact), false, AvatarActor);

	TArray<FHitResult> HitResults;
	const bool bHit = World->SweepMultiByObjectType(HitResults, SweepStart, CurrentTraceOrigin, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(TraceRadius), QueryParams);
	if (bDebugDrawTrace)
	{
		constexpr float DebugLife = -1.0f; 
		DrawDebugLine(World, SweepStart, CurrentTraceOrigin, bHit ? FColor::Green : FColor::Red, false, DebugLife, 0, 0.5f);
		DrawDebugSphere(World, CurrentTraceOrigin, TraceRadius, 12, bHit ? FColor::Green : FColor::Red, false, DebugLife);
	}
	
	PreviousTraceOrigin = CurrentTraceOrigin;
	bHasValidPreviousTraceOrigin = true;

	if (!bHit)
	{
		return;
	}
	for (const FHitResult& Hit : HitResults)
	{
		AActor* TargetActor = Hit.GetActor();
		if (!IsValid(TargetActor) || TargetActor == AvatarActor || HitActorsThisStep.Contains(TargetActor)) continue;

		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor))
		{
			FGameplayEffectContextHandle PerHitContext = SourceASC->MakeEffectContext();
			PerHitContext.AddInstigator(AvatarActor, AvatarActor);
			PerHitContext.AddSourceObject(this);
			
			PerHitContext.AddHitResult(Hit, /*bReset=*/true);

			FGameplayEffectSpecHandle PerHitSpec = SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), PerHitContext);
			if (!PerHitSpec.IsValid() || !PerHitSpec.Data.IsValid()) continue;

			PerHitSpec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, DamageMul);

			PerHitSpec.Data->AddDynamicAssetTag(RetrieveGameplayTags::Attack_Type_Normal);
			
			SourceASC->ApplyGameplayEffectSpecToTarget(*PerHitSpec.Data.Get(), TargetASC);
			HitActorsThisStep.Add(TargetActor);
		}
	}
}

void UGA_Attack::HandleMontageCompleted()
{
	if (bComboQueued && CachedWeaponData.ComboSteps.IsValidIndex(CurrentComboIndex + 1))
	{
		StartComboStep(CurrentComboIndex + 1);
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Attack::HandleMontageInterrupted()
{	
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
void UGA_Attack::HandleMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGA_Attack::StopRuntimeTasks()
{
	if (InputPressTask)
	{
		InputPressTask->EndTask(); InputPressTask = nullptr;
	}
	if (MontageTask)
	{
		MontageTask->EndTask(); MontageTask = nullptr;
	}
	if (ImpactBeginEventTask)
	{
		ImpactBeginEventTask->EndTask(); ImpactBeginEventTask = nullptr;
	}
	if (ImpactEventTask)
	{
		ImpactEventTask->EndTask(); ImpactEventTask = nullptr;
	}
}

void UGA_Attack::CleanupComboTag() const
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Combo_Open, 0);
	}
}

void UGA_Attack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopRuntimeTasks();
	CleanupComboTag();

	CurrentComboIndex = INDEX_NONE;
	bComboQueued = false;
	HitActorsThisStep.Reset();
	bHasValidPreviousTraceOrigin = false;
	CachedWeaponComponent = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Attack::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	StopRuntimeTasks();
	CleanupComboTag();
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
