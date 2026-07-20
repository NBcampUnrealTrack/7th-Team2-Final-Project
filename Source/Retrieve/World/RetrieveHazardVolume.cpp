#include "World/RetrieveHazardVolume.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/BoxComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

ARetrieveHazardVolume::ARetrieveHazardVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	HazardVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("HazardVolume"));
	HazardVolume->SetupAttachment(SceneRoot);
	HazardVolume->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	HazardVolume->SetCollisionProfileName(TEXT("Trigger"));
	HazardVolume->SetGenerateOverlapEvents(true);

	DamageSetByCallerTag = RetrieveGameplayTags::Data_Damage_Fall;
}

void ARetrieveHazardVolume::BeginPlay()
{
	Super::BeginPlay();

	if (HazardVolume)
	{
		HazardVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARetrieveHazardVolume::OnHazardBeginOverlap);
		HazardVolume->OnComponentEndOverlap.AddUniqueDynamic(this, &ARetrieveHazardVolume::OnHazardEndOverlap);
	}
}

void ARetrieveHazardVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimerHandle);
	}
	ActorsInside.Empty();

	Super::EndPlay(EndPlayReason);
}

void ARetrieveHazardVolume::OnHazardBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	// 데미지·사망은 서버 권위.
	if (!HasAuthority() || !IsAffectable(OtherActor))
	{
		return;
	}

	ActorsInside.Add(OtherActor);

	if (bDamageOnEnter)
	{
		ApplyDamageTo(OtherActor);
	}

	// 아직 타이머가 안 돌고 있으면 시작.
	if (!GetWorldTimerManager().IsTimerActive(DamageTimerHandle))
	{
		GetWorldTimerManager().SetTimer(
			DamageTimerHandle, this, &ARetrieveHazardVolume::ApplyPeriodicDamage, DamageInterval, true);
	}
}

void ARetrieveHazardVolume::OnHazardEndOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!OtherActor)
	{
		return;
	}

	ActorsInside.Remove(OtherActor);

	// 무효(GC/파괴) 항목 청소 후 아무도 없으면 타이머 정지.
	for (auto It = ActorsInside.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	if (ActorsInside.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(DamageTimerHandle);
	}
}

void ARetrieveHazardVolume::ApplyPeriodicDamage()
{
	if (!HasAuthority())
	{
		return;
	}

	bool bAnyValid = false;
	for (auto It = ActorsInside.CreateIterator(); It; ++It)
	{
		AActor* TargetActor = It->Get();
		if (!TargetActor)
		{
			It.RemoveCurrent();
			continue;
		}
		// 부활 텔레포트/충돌 해제 등으로 EndOverlap을 놓친 잔존 항목 방어 —
		// 실제로 더 이상 겹쳐 있지 않으면 데미지 없이 목록에서 제거한다.
		if (HazardVolume && !HazardVolume->IsOverlappingActor(TargetActor))
		{
			It.RemoveCurrent();
			continue;
		}
		bAnyValid = true;
		ApplyDamageTo(TargetActor);
	}

	if (!bAnyValid)
	{
		GetWorldTimerManager().ClearTimer(DamageTimerHandle);
	}
}

void ARetrieveHazardVolume::ApplyDamageTo(AActor* TargetActor)
{
	if (!HasAuthority() || !DamageEffect || !IsAffectable(TargetActor))
	{
		return;
	}

	IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(TargetActor);
	UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return;
	}

	// 시네마틱 연출로 이동 중인 대상(State.Player.Cinematic)은 환경 데미지 제외 —
	// 시퀀서 경로가 볼륨을 지나도 데미지/사망이 연출을 끊지 않는다. 주기 타이머는 계속 돌므로
	// 종료 후에도 볼륨 안에 있으면 데미지가 재개된다.
	if (TargetASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Cinematic))
	{
		return;
	}

	// 소스 없는 환경 데미지 — 대상 자기 ASC에 자가 적용(낙하 데미지와 동일 경로).
	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddInstigator(this, this);

	const FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(DamageEffect, 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}

	if (DamageSetByCallerTag.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(DamageSetByCallerTag, DamagePerTick);
	}
	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);

	OnHazardDamageApplied(TargetActor);
}

bool ARetrieveHazardVolume::IsAffectable(AActor* OtherActor) const
{
	return OtherActor && OtherActor != this && Cast<IAbilitySystemInterface>(OtherActor) != nullptr;
}
