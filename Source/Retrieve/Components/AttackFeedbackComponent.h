
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Data/RetrieveDataTableTypes.h"
#include "AttackFeedbackComponent.generated.h"

class URetrieveAbilitySystemComponent;
class UDataTable;
struct FGameplayEventData;

/**
 * 공격자(플레이어) 측 전투 피드백 컴포넌트 PlayerController가 소유
 * 플레이어가 적을 가격했을때(GameplayEvent.Attack.HitSuccess) 
 * 카메라 흔들림 + 대미지 숫자 플로터 등 연출 재생
 * 빙의 시 PC에서 새 폰 ASC로 재 바인딩
 */
UCLASS(ClassGroup = "Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API UAttackFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttackFeedbackComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// PC에서 호출 이전 폰 ASC 구독 해제 후 새 폰 ASC 준비되면 재 바인딩
	void HandlePossessedPawnChanged(APawn* NewPawn);
	
private:
	// 현재 폰의 ASC를 찾아 DT 기반 필터로 HitSuccess 이벤트 구독
	void BindASC();
	void UnbindFromASC();
	
	// 구독한 HitSuccess 이벤트 콜백 본인 기준
	void HandleHitFeedback(FGameplayTag EventTag, const FGameplayEventData* Payload);
	// 매칭된 행의 카메라 흔들림 재생
	void PlayCameraShake(const FHitFeedback& Feedback) const;
	// 이벤트 -> 피드백 매핑 데이터 테이블
	UPROPERTY(EditDefaultsOnly, Category= "Feedback")
	TObjectPtr<UDataTable> HitFeedbackTable;
	// 런타임 캐시
	TMap<FGameplayTag, FHitFeedback> FeedbackCache;
	// 구독 필터
	FGameplayTagContainer SubscribedFilter;
	FDelegateHandle GameplayEventHandle;
	TWeakObjectPtr<URetrieveAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<APawn> CurrentPawn;
};
