#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "RetrieveGameplayAbility.generated.h"

class UAbilitySystemComponent;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;
struct FRetrieveBufferedCombatInput;

UENUM(BlueprintType)
enum class ERetrieveAbilityActivationPolicy : uint8
{
	OnInputTriggered,
	OnSpawn,
	WhileInputActive // reserved; not used in MVP
};

UCLASS(Abstract)
class RETRIEVE_API URetrieveGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URetrieveGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	ERetrieveAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }
	bool ShouldUseCombatInputBuffer() const { return bUseCombatInputBuffer; }
	int32 GetCombatInputPriority() const { return CombatInputPriority; }
	float GetCombatInputBufferSeconds() const { return CombatInputBufferSeconds; }

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& AbilitySpec) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	static bool IsAvatarAirborne(const FGameplayAbilityActorInfo* ActorInfo);

	/**
	 * 리졸버가 '이미 활성인 이 어빌리티'의 버퍼 입력을 발견했을 때 호출한다(재발동 불가한 자기 입력).
	 * 내부 전환(예: 콤보 다음 타)을 처리했으면 true를 반환해 그 입력이 버퍼에서 소비되게 한다.
	 * 기본은 처리 안 함. GA_Attack이 콤보 진행을 위해 override한다.
	 */
	virtual bool TryConsumeBufferedCombatInput(const FRetrieveBufferedCombatInput& BufferedInput) { return false; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Ability")
	ERetrieveAbilityActivationPolicy ActivationPolicy = ERetrieveAbilityActivationPolicy::OnInputTriggered;

	// === 전투 입력 버퍼 ===
	// 켜면 이 입력은 즉시 처리되지 않고 ASC의 CombatInputBuffer에 적재된다. 이후 매 프레임
	// ResolveBufferedCombatInput()이 "지금 발동 가능한지"를 보고 소비한다.
	//   - 전투 중이 아니면 → 그 프레임에 바로 소비(즉시 발동처럼 보임)
	//   - 공격 중이면      → 캔슬 윈도우(AttackCancelWindow)가 그 입력을 허용해야 발동
	// 즉 '선입력 버퍼'와 '캔슬 전환'을 플래그 없이 한 경로로 처리한다.
	// 전투 입력(평타/Heavy/Sprint/JumpAttack/점프/원소전환 등)에 켠다. 비전투(상호작용 등)는 끈다.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Combat Input")
	bool bUseCombatInputBuffer = false;

	// 버퍼 소비 우선순위(높을수록 먼저 소비). 서열 예: 20=원소전환 > 10=공격류(Sprint/Heavy/Jump) > 0=평타.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Combat Input", meta = (ClampMin = "0"))
	int32 CombatInputPriority = 0;

	// 버퍼된 입력의 유효 시간(초). 이 시간이 지나면 만료·폐기된다(선입력을 얼마나 일찍 받아줄지).
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Combat Input", meta = (ClampMin = "0.0"))
	float CombatInputBufferSeconds = 0.25f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Ability")
	bool bBlockActivationWhileAirborne = false;

	// ALS LocomotionAction(구르기/맨틀 등)이 진행 중이면 발동 차단. ALS 네이티브 잠금에 GAS를 연동.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Ability")
	bool bBlockedByLocomotionAction = false;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Ability|Parry")
	bool bAutoListenForParried = false;
	
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Ability|Parry")
	void StartListeningForParried();
	
	// TODO(하민): 패리 스태거는 방어자 push 경로(UGA_ParryBase::ApplyParryStagger)로 일원화됨. 향후 제거 예정
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Ability|Parry")
	TSubclassOf<UGameplayEffect> ParriedStaggerEffect;

	// 플레이어 현재 원소 모드 태그(PlayerState 조회). 없으면 EmptyTag.
	FGameplayTag ResolveCurrentElementTag() const;
	
	bool HasStamina(const FGameplayAbilityActorInfo* ActorInfo, float Cost) const;

	// 컨텍스트(Instigator=아바타, SourceObject=this)를 갖춘 아웃고잉 GE 스펙 생성. ASC/이펙트 없으면 무효 핸들
	FGameplayEffectSpecHandle MakeSourcedSpec(TSubclassOf<UGameplayEffect> EffectClass, float Level) const;

	// 대상이 보스(Monster.Type.Boss)면 BossEffect, 아니면 NormalEffect 선택
	static TSubclassOf<UGameplayEffect> SelectEffectByTargetType(const UAbilitySystemComponent* TargetASC, TSubclassOf<UGameplayEffect> NormalEffect, TSubclassOf<UGameplayEffect> BossEffect);

	// 플레이어 액션 공통 발동 게이트: 회피/경직/다운/사망 차단(+선택적으로 공중 차단)
	void ApplyCommonActionBlocks(bool bBlockAirborne = true);

private:
	void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& AbilitySpec) const;
	// LocomotionAction 차단 중에도, 현재 캔슬 윈도우가 이 어빌리티의 입력 intent를 허용하면 true(차단 무시 = 캔슬 우선).
	bool IsAllowedByActiveCancelWindow(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpecHandle Handle) const;

	UFUNCTION() void HandleParried(FGameplayEventData Payload);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ParriedTask;
};
