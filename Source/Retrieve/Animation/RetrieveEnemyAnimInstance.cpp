#include "Animation/RetrieveEnemyAnimInstance.h"

#include "GameFramework/Pawn.h"

void URetrieveEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const APawn* Pawn = TryGetPawnOwner();
	if (!Pawn)
	{
		Speed = 0.f;
		Direction = 0.f;
		CachedPawnForMovementSample.Reset();
		bHasMovementSampleLocation = false;
		return;
	}

	const FVector ActorLocation = Pawn->GetActorLocation();
	const FVector Velocity2D(Pawn->GetVelocity().X, Pawn->GetVelocity().Y, 0.f);
	FVector Movement2D = Velocity2D;

	if (CachedPawnForMovementSample.Get() != Pawn)
	{
		CachedPawnForMovementSample = const_cast<APawn*>(Pawn);
		bHasMovementSampleLocation = false;
	}

	if (bHasMovementSampleLocation && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		const FVector DeltaLocation2D(
			ActorLocation.X - LastMovementSampleLocation.X,
			ActorLocation.Y - LastMovementSampleLocation.Y,
			0.f);
		const FVector LocationDeltaVelocity2D = DeltaLocation2D / DeltaSeconds;
		if (LocationDeltaVelocity2D.SizeSquared() > Movement2D.SizeSquared())
		{
			Movement2D = LocationDeltaVelocity2D;
		}
	}

	LastMovementSampleLocation = ActorLocation;
	bHasMovementSampleLocation = true;

	Speed = Movement2D.Size();
	if (Speed <= KINDA_SMALL_NUMBER)
	{
		Direction = 0.f;
		return;
	}

	const FRotator ActorRotation(0.f, Pawn->GetActorRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(ActorRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(ActorRotation).GetUnitAxis(EAxis::Y);
	const FVector MoveDirection = Movement2D.GetSafeNormal();
	Direction = FMath::RadiansToDegrees(FMath::Atan2(
		FVector::DotProduct(Right, MoveDirection),
		FVector::DotProduct(Forward, MoveDirection)));
}
