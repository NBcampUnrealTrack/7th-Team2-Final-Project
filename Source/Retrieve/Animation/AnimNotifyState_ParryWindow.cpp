#include "Animation/AnimNotifyState_ParryWindow.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_ParryWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 여러 Ability가 동시에 활성일 수 있으므로 전체를 순회한다.
	// 다만 ParryWindow를 실제로 여는 Ability는 한 개면 충분하다.
	// 첫 성공 이후에는 bOpened로 추가 Open 요청을 막아 같은 Notify 구간에서 중복 window가 생기지 않게 한다.
	bool bOpened = false;
	ForEachActiveRetrieveAbility(MeshComp, [&bOpened](URetrieveGameplayAbility& Ability)
	{
		if (!bOpened)
		{
			bOpened = Ability.OpenNotifyParryWindow();
		}
	});
}

void UAnimNotifyState_ParryWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	// Begin에서 누가 window를 열었는지 NotifyState가 직접 저장하지 않는다.
	// 이유:
	// - AnimNotifyState 객체는 montage 재생별 상태 저장소로 쓰기 애매하고,
	//   같은 notify asset이 여러 캐릭터/여러 montage instance에서 공유될 수 있다.
	// - 대신 End 시점에도 활성 Ability 전체에 Close 요청을 보내고,
	//   각 Ability가 자기 내부 handle/flag로 idempotent하게 닫는다.
	ForEachActiveRetrieveAbility(MeshComp, [](URetrieveGameplayAbility& Ability)
	{
		Ability.CloseNotifyParryWindow();
	});

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_ParryWindow::GetNotifyName_Implementation() const
{
	return TEXT("ParryWindow");
}

void UAnimNotifyState_ParryWindow::ForEachActiveRetrieveAbility(
	const USkeletalMeshComponent* MeshComp,
	TFunctionRef<void(URetrieveGameplayAbility&)> Func) const
{
	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
	if (!IsValid(ASC))
	{
		return;
	}

	// NotifyState가 UGA_GuardAttack을 직접 Cast하지 않는 것이 핵심이다.
	// 이 루프는 "현재 활성 Ability 중 ParryWindow hook을 구현한 Ability가 있으면 반응시킨다"는 중립적인 라우터다.
	//
	// GetPrimaryInstance 전제:
	// - 현재 플레이어 전투 Ability들은 InstancedPerActor 패턴을 사용한다.
	// - CDO가 아니라 활성 instance를 받아야 CachedGuardAttackData, ParryWindowHandle 같은 런타임 상태를 읽을 수 있다.
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		URetrieveGameplayAbility* Ability = Cast<URetrieveGameplayAbility>(Spec.GetPrimaryInstance());
		if (IsValid(Ability) && Ability->IsActive())
		{
			Func(*Ability);
		}
	}
}
