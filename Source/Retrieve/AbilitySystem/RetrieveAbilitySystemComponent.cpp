#include "AbilitySystem/RetrieveAbilitySystemComponent.h"

#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Character/Cosmetics/RetrieveAlsAnimInstance.h"
#include "Character/Cosmetics/SovereignAnimInstance.h"
#include "Engine/World.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void URetrieveAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	check(ActorInfo);
	check(InOwnerActor);

	const bool bHasNewPawnAvatar = Cast<APawn>(InAvatarActor) && (InAvatarActor != ActorInfo->AvatarActor.Get());

	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (bHasNewPawnAvatar)
	{
		if (USovereignAnimInstance* SovereignAnimInst = Cast<USovereignAnimInstance>(ActorInfo->GetAnimInstance()))
		{
			SovereignAnimInst->InitializeWithAbilitySystem(this);
		}

		// ALS 가지 메인 AnimInstance. 동일 Mesh의 AnimInstance는 둘 중 하나만 성공 (안전).
		if (URetrieveAlsAnimInstance* AlsAnimInst = Cast<URetrieveAlsAnimInstance>(ActorInfo->GetAnimInstance()))
		{
			AlsAnimInst->InitializeWithAbilitySystem(this);
		}
	}
}

void URetrieveAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.Ability || !AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		// 전투 입력은 버퍼로만 흐른다 — 발동 여부는 리졸버가 매 프레임 결정한다.
		// 비전투 입력(상호작용 등)은 기존대로 즉시 처리 목록에 올린다.
		const URetrieveGameplayAbility* RetrieveAbility = Cast<URetrieveGameplayAbility>(AbilitySpec.Ability);
		if (RetrieveAbility && RetrieveAbility->ShouldUseCombatInputBuffer())
		{
			BufferCombatInput(AbilitySpec, InputTag, *RetrieveAbility);
		}
		else
		{
			InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
			InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
		}
	}
}

void URetrieveAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
			InputHeldSpecHandles.Remove(AbilitySpec.Handle);
		}
	}
}

void URetrieveAbilitySystemComponent::ClearInputHeldForSpec(const FGameplayAbilitySpecHandle& SpecHandle)
{
	InputHeldSpecHandles.Remove(SpecHandle);
}

bool URetrieveAbilitySystemComponent::HasActivatableAbilityWithInputTag(const FGameplayTag& InputTag) const
{
	if (!InputTag.IsValid())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return true;
		}
	}
	return false;
}

void URetrieveAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	if (bGamePaused)
	{
		ClearAbilityInput();
		return;
	}

	// --- 비전투(즉시) 입력: 눌림은 발동, 활성 중이면 InputPressed 전달 ---
	TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;
	AbilitiesToActivate.Reserve(InputPressedSpecHandles.Num());

	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle);
		if (!AbilitySpec || !AbilitySpec->Ability)
		{
			continue;
		}

		if (AbilitySpec->IsActive())
		{
			AbilitySpecInputPressed(*AbilitySpec);

			if (const UGameplayAbility* AbilityInstance = AbilitySpec->GetPrimaryInstance())
			{
				InvokeReplicatedEvent(
					EAbilityGenericReplicatedEvent::InputPressed,
					AbilitySpec->Handle,
					AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
			}
		}
		else
		{
			AbilitiesToActivate.AddUnique(SpecHandle);
		}
	}

	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(AbilitySpecHandle);
	}

	// --- 입력 뗌: 활성 어빌리티에 InputReleased 전달 ---
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle);
		if (!AbilitySpec || !AbilitySpec->Ability || !AbilitySpec->IsActive())
		{
			continue;
		}

		AbilitySpecInputReleased(*AbilitySpec);

		if (const UGameplayAbility* AbilityInstance = AbilitySpec->GetPrimaryInstance())
		{
			InvokeReplicatedEvent(
				EAbilityGenericReplicatedEvent::InputReleased,
				AbilitySpec->Handle,
				AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();

	// --- 전투 입력: 버퍼에서 발동 가능한 1건 소비(시스템의 단일 결정 지점) ---
	ResolveBufferedCombatInput();

	// --- WhileInputActive: 홀드 중인데 차단으로 미발동인 어빌리티 재시도 ---
	// 공격 중 차단된 Guard가 공격 종료 후 자동 진입하는 경로. 발동되면 IsActive로 스킵된다.
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle);
		if (!AbilitySpec || !AbilitySpec->Ability || AbilitySpec->IsActive())
		{
			continue;
		}

		const URetrieveGameplayAbility* RetrieveAbility = Cast<URetrieveGameplayAbility>(AbilitySpec->Ability);
		if (RetrieveAbility && RetrieveAbility->GetActivationPolicy() == ERetrieveAbilityActivationPolicy::WhileInputActive)
		{
			TryActivateAbility(SpecHandle);
		}
	}
}

void URetrieveAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
	CombatInputBuffer.Reset();
	CombatInputSequence = 0;
}

void URetrieveAbilitySystemComponent::AddAttackCancelWindow(const FGameplayTag& CancelOpenTag, const FGameplayTagContainer& AllowedCancelIntents)
{
	// 윈도우가 겹칠 수 있어 intent별 ref-count로 관리한다. 한 윈도우의 해제가
	// 다른 윈도우가 아직 허용 중인 intent를 꺼버리지 않도록 보장.
	for (auto It = AllowedCancelIntents.CreateConstIterator(); It; ++It)
	{
		const FGameplayTag& IntentTag = *It;
		if (IntentTag.IsValid())
		{
			++AttackCancelIntentCounts.FindOrAdd(IntentTag);
		}
	}

	if (CancelOpenTag.IsValid())
	{
		AddLooseGameplayTag(CancelOpenTag);
	}
}

void URetrieveAbilitySystemComponent::RemoveAttackCancelWindow(const FGameplayTag& CancelOpenTag, const FGameplayTagContainer& AllowedCancelIntents)
{
	// 종료 정리(ClearAttackCancelWindows)가 카운트를 먼저 0으로 만든 뒤 ANS End가 늦게 떨어질 수 있다.
	// count>0 일 때만 제거해 언더플로 워닝을 막는다.
	if (CancelOpenTag.IsValid() && GetGameplayTagCount(CancelOpenTag) > 0)
	{
		RemoveLooseGameplayTag(CancelOpenTag);
	}

	for (auto It = AllowedCancelIntents.CreateConstIterator(); It; ++It)
	{
		const FGameplayTag& IntentTag = *It;
		int32* Count = IntentTag.IsValid() ? AttackCancelIntentCounts.Find(IntentTag) : nullptr;
		if (!Count)
		{
			continue;
		}

		if (--(*Count) <= 0)
		{
			AttackCancelIntentCounts.Remove(IntentTag);
		}
	}
}

void URetrieveAbilitySystemComponent::ClearAttackCancelWindows(const FGameplayTag& CancelOpenTag)
{
	AttackCancelIntentCounts.Reset();
	if (CancelOpenTag.IsValid())
	{
		SetLooseGameplayTagCount(CancelOpenTag, 0);
	}
}

bool URetrieveAbilitySystemComponent::IsAttackCancelIntentAllowed(const FGameplayTag& IntentTag) const
{
	if (!IntentTag.IsValid() || !HasMatchingGameplayTag(RetrieveGameplayTags::State_Attack_CancelOpen))
	{
		return false;
	}

	const int32* Count = AttackCancelIntentCounts.Find(IntentTag);
	return Count && *Count > 0;
}

void URetrieveAbilitySystemComponent::SetPendingCounterTarget(AActor* InTarget)
{
	if (IsValid(InTarget))
	{
		PendingCounterTarget = InTarget;
		return;
	}

	PendingCounterTarget.Reset();
}

AActor* URetrieveAbilitySystemComponent::GetPendingCounterTarget() const
{
	return IsValid(PendingCounterTarget.Get()) ? PendingCounterTarget.Get() : nullptr;
}

void URetrieveAbilitySystemComponent::ClearPendingCounterTarget()
{
	PendingCounterTarget.Reset();
}

void URetrieveAbilitySystemComponent::SetCounterWarpTargetLocked(bool bInLocked)
{
	bCounterWarpTargetLocked = bInLocked;
}

bool URetrieveAbilitySystemComponent::IsCounterWarpTargetLocked() const
{
	return bCounterWarpTargetLocked;
}

void URetrieveAbilitySystemComponent::BufferCombatInput(const FGameplayAbilitySpec& AbilitySpec, const FGameplayTag& InputTag, const URetrieveGameplayAbility& AbilityCDO)
{
	const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	PruneExpiredCombatInputs(NowSeconds);

	// 같은 어빌리티+intent의 이전 입력은 최신 것으로 갱신(연타 시 버퍼가 불어나지 않게).
	CombatInputBuffer.RemoveAll([&AbilitySpec, &InputTag](const FRetrieveBufferedCombatInput& Entry)
	{
		return Entry.AbilitySpecHandle == AbilitySpec.Handle && Entry.IntentTag == InputTag;
	});

	FRetrieveBufferedCombatInput& Buffered = CombatInputBuffer.AddDefaulted_GetRef();
	Buffered.AbilitySpecHandle = AbilitySpec.Handle;
	Buffered.IntentTag = InputTag;
	Buffered.TimeSeconds = NowSeconds;
	Buffered.BufferSeconds = AbilityCDO.GetCombatInputBufferSeconds();
	Buffered.Priority = AbilityCDO.GetCombatInputPriority();
	Buffered.Sequence = ++CombatInputSequence;
}

void URetrieveAbilitySystemComponent::PruneExpiredCombatInputs(double NowSeconds)
{
	CombatInputBuffer.RemoveAll([NowSeconds](const FRetrieveBufferedCombatInput& Entry)
	{
		return NowSeconds - Entry.TimeSeconds > Entry.BufferSeconds;
	});
}

void URetrieveAbilitySystemComponent::ResolveBufferedCombatInput()
{
	if (CombatInputBuffer.Num() == 0)
	{
		return;
	}

	const double NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	PruneExpiredCombatInputs(NowSeconds);
	if (CombatInputBuffer.Num() == 0)
	{
		return;
	}

	// 우선순위 높은 입력부터(동순위면 최신 입력부터) 검사할 순서를 만든다.
	TArray<int32> Order;
	Order.Reserve(CombatInputBuffer.Num());
	for (int32 i = 0; i < CombatInputBuffer.Num(); ++i)
	{
		Order.Add(i);
	}
	Order.Sort([this](int32 A, int32 B)
	{
		const FRetrieveBufferedCombatInput& L = CombatInputBuffer[A];
		const FRetrieveBufferedCombatInput& R = CombatInputBuffer[B];
		if (L.Priority != R.Priority)
		{
			return L.Priority > R.Priority;
		}
		return L.Sequence > R.Sequence;
	});

	const bool bAttackActive = IsAttackAbilityActive();

	for (const int32 Index : Order)
	{
		if (!CombatInputBuffer.IsValidIndex(Index))
		{
			continue;
		}

		const FRetrieveBufferedCombatInput Entry = CombatInputBuffer[Index];
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Entry.AbilitySpecHandle);
		if (!Spec || !Spec->Ability)
		{
			continue;
		}

		// (1) 이미 활성인 자기 어빌리티 → 재발동 불가. 내부 전환(콤보 다음 타 등)만 위임한다.
		// 주의: 콤보 상태(CurrentComboIndex 등)는 CDO가 아니라 '활성 인스턴스'에 있다.
		// 반드시 GetPrimaryInstance()로 인스턴스에 위임해야 한다(InstancedPerActor).
		if (Spec->IsActive())
		{
			UGameplayAbility* AbilityInstance = Spec->GetPrimaryInstance();
			URetrieveGameplayAbility* RetrieveAbility = Cast<URetrieveGameplayAbility>(AbilityInstance ? AbilityInstance : Spec->Ability.Get());
			if (RetrieveAbility && RetrieveAbility->TryConsumeBufferedCombatInput(Entry))
			{
				CombatInputBuffer.RemoveAll([&Entry](const FRetrieveBufferedCombatInput& E)
				{
					return E.AbilitySpecHandle == Entry.AbilitySpecHandle && E.Sequence == Entry.Sequence;
				});
				return;
			}
			continue;
		}

		// (2) 새로 발동할 후보.
		// 공격 진행 중이면, 그 입력을 '캔슬 윈도우'가 허용해야만 발동할 수 있다. 평상시엔 제한 없음.
		if (bAttackActive && !IsAttackCancelIntentAllowed(Entry.IntentTag))
		{
			continue;
		}

		// CanActivate는 직접 부르지 않는다 — TryActivateAbility가 내부에서 '인스턴스 기준'으로 검사·발동한다.
		// (CDO에 대고 CanActivateAbility를 부르면 ResolveCurrentElementTag 등이 CDO에서 실행돼 ensure가 터진다)
		// 공격류(Ability.Type.Attack)는 자신의 CancelAbilitiesWithTag로 진행 중 공격을 자동 캔슬하고,
		// 원소 전환처럼 그 태그가 없는 것은 캔슬하지 않아 평타 콤보가 유지된다.
		// 발동이 실패하면 아무것도 캔슬되지 않으므로 현재 공격은 그대로다(no-slot Heavy / 정지 Sprint 안전).
		// '진행 중 공격을 끊고 들어가는 캔슬-인'인지 발동 직전에 기록한다. 어빌리티가 ActivateAbility에서 읽는다
		// (CancelOpen 태그는 CancelAbilitiesWithTag로 ActivateAbility 전에 지워져 못 쓴다 → 동기 플래그로 전달).
		bActivatingAsCancel = bAttackActive;
		const bool bActivated = TryActivateAbility(Entry.AbilitySpecHandle);
		bActivatingAsCancel = false;
		if (bActivated)
		{
			// 발동이 버퍼를 흔들어 Index가 무효화됐을 수 있어 신원(Handle+Sequence)으로 제거.
			CombatInputBuffer.RemoveAll([&Entry](const FRetrieveBufferedCombatInput& E)
			{
				return E.AbilitySpecHandle == Entry.AbilitySpecHandle && E.Sequence == Entry.Sequence;
			});
			return;
		}
	}
}

bool URetrieveAbilitySystemComponent::IsAttackAbilityActive() const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.Ability
			&& Spec.Ability->GetAssetTags().HasTag(RetrieveGameplayTags::Ability_Type_Attack))
		{
			return true;
		}
	}
	return false;
}
