
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Data/RetrieveDataTableTypes.h"
#include "AttackFeedbackComponent.generated.h"

class URetrieveAbilitySystemComponent;
class UDataTable;
class URetrieveDamageFloaterWidget;
struct FRetrieveDamageDealtPayload;
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
	virtual void BeginPlay() override;
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
	// GMS Channel.Combat.DamageDealt 리스너 콜백 — 플로터 스폰(자기판정 포함)
	void HandleDamageDealt(FGameplayTag Channel, const FRetrieveDamageDealtPayload& Payload);
	// 타겟 머리 위에 대미지 숫자 플로터 스폰(또는 풀에서 재사용). 스폰 시 1회 투영.
	void SpawnDamageFloater(const AActor* Target, const FVector& HitLocation, float DamageValue, const FHitFeedback& Feedback);
	// 애니메이션이 끝난 플로터를 뷰포트에서 떼고 풀로 회수(OnFinished에 바인딩됨)
	void ReleaseFloater(URetrieveDamageFloaterWidget* Floater);
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
	// GMS 대미지 리스너 핸들
	FGameplayMessageListenerHandle DamageListener;
	// 대미지 플로터
	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Damage Floater")
	TSubclassOf<URetrieveDamageFloaterWidget> DamageFloaterClass;

	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Damage Floater", meta = (ClampMin = "1"))
	int32 MaxDamageFloaters = 16;

	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Damage Floater")
	int32 FloaterZOrder = 5;
	// 생성한 모든 플로터(정리용). FloaterPool은 이 중 재사용 대기 항목.
	UPROPERTY()
	TArray<TObjectPtr<URetrieveDamageFloaterWidget>> AllFloaters;
	// 재사용 대기 중인 플로터(프리 리스트)
	UPROPERTY()
	TArray<TObjectPtr<URetrieveDamageFloaterWidget>> FloaterPool;
};
