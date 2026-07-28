#include "UI/HUD/RetrieveDamageDirectionWidget.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void URetrieveDamageDirectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 첫 피격 전까지는 숨김. BP의 PlayDamageAnimation이 Set Visibility로 노출한다.
	SetVisibility(ESlateVisibility::Hidden);

	if (UWorld* World = GetWorld())
	{
		DamageListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveDamageDealtPayload>(
			RetrieveGameplayTags::Channel_Combat_DamageDealt,
			[this](FGameplayTag Channel, const FRetrieveDamageDealtPayload& Payload)
			{
				HandleDamageDealt(Channel, Payload);
			});
	}
}

void URetrieveDamageDirectionWidget::NativeDestruct()
{
	if (DamageListener.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(DamageListener);
		}
		DamageListener = FGameplayMessageListenerHandle();
	}

	Super::NativeDestruct();
}

void URetrieveDamageDirectionWidget::HandleDamageDealt(FGameplayTag /*Channel*/, const FRetrieveDamageDealtPayload& Payload)
{
	// 로컬 플레이어가 피격 대상일 때만 처리(다른 액터 피격/내가 가한 공격은 무시).
	const APawn* LocalPawn = GetOwningPlayerPawn();
	if (LocalPawn == nullptr || Payload.Target != LocalPawn)
	{
		return;
	}

	float AngleDeg = 0.f;
	if (ComputeDirectionAngle(Payload.Instigator, AngleDeg) == false)
	{
		// 공격자 위치를 알 수 없으면 정면(0도)으로 표시.
		AngleDeg = 0.f;
	}

	PlayDamageAnimation(AngleDeg);
}

bool URetrieveDamageDirectionWidget::ComputeDirectionAngle(const AActor* Attacker, float& OutAngleDeg) const
{
	const APawn* LocalPawn = GetOwningPlayerPawn();
	if (LocalPawn == nullptr || Attacker == nullptr || Attacker == LocalPawn)
	{
		return false;
	}

	const FVector ToAttacker = Attacker->GetActorLocation() - LocalPawn->GetActorLocation();
	if (ToAttacker.IsNearlyZero())
	{
		return false;
	}

	// 공격자 방향의 월드 yaw.
	const float WorldYawToAttacker = ToAttacker.Rotation().Yaw;

	// 시점(카메라) yaw — 카메라 매니저 우선, 없으면 컨트롤 회전.
	float ViewYaw = 0.f;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (PC->PlayerCameraManager != nullptr)
		{
			ViewYaw = PC->PlayerCameraManager->GetCameraRotation().Yaw;
		}
		else
		{
			ViewYaw = PC->GetControlRotation().Yaw;
		}
	}

	// 0 = 정면, +우측 / -좌측 / ±180 = 후방.
	float Relative = FRotator::NormalizeAxis(WorldYawToAttacker - ViewYaw);

	if (FrontDeadZoneDegrees > 0.f && FMath::Abs(Relative) <= FrontDeadZoneDegrees)
	{
		Relative = 0.f;
	}

	OutAngleDeg = Relative;
	return true;
}
