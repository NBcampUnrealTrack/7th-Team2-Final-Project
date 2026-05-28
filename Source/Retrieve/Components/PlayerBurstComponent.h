#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerBurstComponent.generated.h"

struct FSkillCombination;
struct FBurstHitInstance;
class USphereComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RETRIEVE_API UPlayerBurstComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerBurstComponent();

    void BeginBurstSkill(const FSkillCombination* Row);

    void EndBurstSkill();

    void OnBurstHit(int32 HitIndex);

    // 현재 진행 중인 스킬 컨텍스트
    const FSkillCombination* ActiveSkill = nullptr;

private:
    // AttackType별 분기
    void DoCleaveHit(const FBurstHitInstance& Hit, int32 HitIndex);
    void DoProjectileHit(const FBurstHitInstance& Hit, int32 HitIndex);
    void DoGroundEruptionHit(const FBurstHitInstance& Hit, int32 HitIndex);
    void DoDashHit(const FBurstHitInstance& Hit, int32 HitIndex);
    void DoAoEHit(const FBurstHitInstance& Hit, int32 HitIndex);

    // HitSource → 월드 좌표
    FVector ResolveSourceLocation(const FBurstHitInstance& Hit) const;

    // 공용 Sweep+적용
    void SweepAndApply(const FBurstHitInstance& Hit, const FVector& CurrentOrigin, float Radius, int32 HitIndex);

    // 단일 타격 인플릭트
    void ApplyHitToTarget(AActor* Target, const FBurstHitInstance& Hit, const FHitResult& HitResult);

    UPROPERTY(EditDefaultsOnly, Category = "Burst|Trace", meta=(ClampMin="0.0"))
    float CleaveRadius = 60.f;

    UPROPERTY(EditDefaultsOnly, Category = "Burst|Trace", meta=(ClampMin="0.0"))
    float DashRadius = 80.f;

    UPROPERTY(EditDefaultsOnly, Category = "Burst|Trace", meta=(ClampMin="0.0"))
    float GroundEruptionRadius = 200.f;

    UPROPERTY(EditDefaultsOnly, Category = "Burst|Trace")
    bool bDebugDrawTrace = false;

    UPROPERTY()
    TObjectPtr<USphereComponent> ActiveHitboxComp;

    // GroundEruption 전용
    TWeakObjectPtr<AActor> SpawnedWorldActor;

    // HitIndex별 상태
    TArray<TSet<TObjectPtr<AActor>>> PerHitHitActors;
    TArray<FVector> PerHitPreviousOrigin;
    TArray<bool>    PerHitHasPrevious;
};
