#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerBurstComponent.generated.h"

struct FSkillCombination;
struct FBurstHitInstance;
class UAbilitySystemComponent;
class UGameplayEffect;
class UDataTable;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RETRIEVE_API UPlayerBurstComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerBurstComponent();

    void BeginBurstSkill(const FSkillCombination* Row);

    void EndBurstSkill();

    void OnBurstHit(int32 HitIndex);

    /** ABurstProjectile 등 외부 액터가 적중을 보고할 때 호출하는 공개 래퍼. */
    void ReportProjectileHit(AActor* Target, const FBurstHitInstance& Hit, const FHitResult& HitResult);

    // 현재 진행 중인 스킬 컨텍스트
    const FSkillCombination* ActiveSkill = nullptr;

private:
    // AttackType별 분기
    void DoCleaveHit(const FBurstHitInstance& Hit, int32 HitIndex);
    void DoProjectileHit(const FBurstHitInstance& Hit, int32 HitIndex);
    void DoWorldActorHit(const FBurstHitInstance& Hit, int32 HitIndex);
    void DoDashHit(const FBurstHitInstance& Hit, int32 HitIndex);
    void DoAoEHit(const FBurstHitInstance& Hit, int32 HitIndex);

    // HitSource → 월드 좌표
    FVector ResolveSourceLocation(const FBurstHitInstance& Hit) const;

    // 검(Sword) 히트용 멀티포인트 블레이드 트레이스 포인트 생성. GA_Attack::BuildTracePoints와 동일 방식.
    void BuildSwordTracePoints(const FBurstHitInstance& Hit, TArray<FVector>& OutPoints) const;

    // 공용 Sweep+적용
    void SweepAndApply(const FBurstHitInstance& Hit, const FVector& CurrentOrigin, float Radius, int32 HitIndex);

    // 단일 타격 인플릭트
    void ApplyHitToTarget(AActor* Target, const FBurstHitInstance& Hit, const FHitResult& HitResult);

    // 적중 VFX/사운드 재생 (FBurstHitInstance.HitVFX/HitSound)
    void PlayHitFeedback(const FBurstHitInstance& Hit, const FHitResult& HitResult, AActor* Target) const;

    // 상태 GE 부여 직전 원소 반응 검사 (ReactionTable 기반)
    void TryElementReaction(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC,
        const TSubclassOf<UGameplayEffect>& IncomingStatusGE);

    UPROPERTY(EditDefaultsOnly, Category = "Burst|Trace", meta=(ClampMin="0.0"))
    float CleaveRadius = 60.f;

    UPROPERTY(EditDefaultsOnly, Category = "Burst|Trace", meta=(ClampMin="0.0"))
    float DashRadius = 80.f;

    UPROPERTY(EditDefaultsOnly, Category = "Burst|Trace", meta=(ClampMin="0.0"))
    float WorldActorRadius = 200.f;

    UPROPERTY(EditDefaultsOnly, Category = "Burst|Projectile", meta=(ClampMin="0.0"))
    float DefaultProjectileSpeed = 1500.f;

    UPROPERTY(EditDefaultsOnly, Category = "Burst|Trace")
    bool bDebugDrawTrace = false;

    // 원소 반응 규칙 테이블 (FElementReactionRow)
    UPROPERTY(EditDefaultsOnly, Category = "Burst|Reaction")
    TObjectPtr<UDataTable> ReactionTable;

    // WorldActor 전용 (소환된 월드 액터 참조)
    TWeakObjectPtr<AActor> SpawnedWorldActor;

    // HitIndex별 상태
    TArray<TSet<TObjectPtr<AActor>>> PerHitHitActors;
    // HitIndex별 직전 프레임 트레이스 포인트(멀티포인트). 프레임 간 보간 sweep에 사용.
    TArray<TArray<FVector>> PerHitPreviousPoints;
    TArray<bool>    PerHitHasPrevious;

    // Projectile 중복 스폰 방지 (HitIndex별 1회 발사)
    TArray<bool>    PerHitProjectileSpawned;

    // Dash 중복 발사 방지 (HitIndex별 1회 발사)
    TArray<bool>    PerHitDashLaunched;
};
