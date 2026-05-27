

#include "LockOnCameraRig.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

ULockOnCameraRig::ULockOnCameraRig()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void ULockOnCameraRig::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (bIsTracking == false && bIsReturning == false)
	{
		SetComponentTickEnabled(false);
		return;
	}
	
	APawn* Pawn = OwnerPawn.Get();
	APlayerController* PC = OwnerPC.Get();
	AActor* Target = CurrentTarget.Get();
	USpringArmComponent* SA = SpringArm.Get();
	if (bIsTracking)
	{
		// 추적 중 의존성이 무효화되면 모드 갇힘 방지 위해 정리
		if (IsValid(Pawn) == false || IsValid(PC) == false || IsValid(Target) == false || IsValid(SA) == false)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[LockOnCameraRig] Tracking stopped: runtime reference became invalid (Pawn=%d, PC=%d, Target=%d, SpringArm=%d)"),
				IsValid(Pawn), IsValid(PC), IsValid(Target), IsValid(SA));
			StopTracking();
			return;
		}
	
		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);
	
		const FVector TargetLoc = Target->GetActorLocation() + FVector(0.f, 0.f, TargetVerticalOffset);
		FRotator DesiredRot = (TargetLoc - CamLoc).Rotation();
		DesiredRot.Roll = 0.f;
		DesiredRot.Pitch = FMath::ClampAngle(DesiredRot.Pitch, MinPitch, MaxPitch);
	
		const FRotator CurrentRot = PC->GetControlRotation();
		const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, CameraInterpSpeed);
		PC->SetControlRotation(NewRot);
	
		const FVector NewOffset = FMath::VInterpTo(SA->SocketOffset, LockOnSocketOffset, DeltaTime, OffsetInterpSpeed);
		SA->SocketOffset = NewOffset;
	}
	else
	{
		if (IsValid(SA) == false)
		{
			bIsReturning = false;
			SetComponentTickEnabled(false);
			return;
		}
		// SocketOffset 백업값으로 보간 복원(모드/카메라 회전은 이미 자유 상태)
		const FVector NewOffset = FMath::VInterpTo(SA->SocketOffset, SavedSocketOffset, DeltaTime, OffsetInterpSpeed);
		SA->SocketOffset = NewOffset;
		// 거의 도달했으면 종료 (VInterpTo는 점근적이라 명시적 종료 필요)
		if (FVector::DistSquared(NewOffset, SavedSocketOffset) < 0.25f)
		{
			SA->SocketOffset = SavedSocketOffset;
			bIsReturning = false;
			SetComponentTickEnabled(false);
		}
	}
}

void ULockOnCameraRig::Initialize()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	ACharacter* OwnerChar = Cast<ACharacter>(Pawn);
	if (IsValid(Pawn) == false || IsValid(OwnerChar) == false)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[LockOnCameraRig] Initialize failed: Owner must be an ACharacter."));
		return;
	}

	
	UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement();
	USpringArmComponent* SA = OwnerChar->FindComponentByClass<USpringArmComponent>();
	if (IsValid(MoveComp) == false || IsValid(SA) == false)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[LockOnCameraRig] Initialize failed: required component missing (MoveComp=%d, SpringArm=%d)"),
			IsValid(MoveComp), IsValid(SA));
		return;
	}

	
	OwnerPawn = Pawn;
	MovementComp = MoveComp;
	SpringArm = SA;
	OwnerPC = Cast<APlayerController>(Pawn->GetController());
}

void ULockOnCameraRig::StartTracking(AActor* Target)
{
	if (IsValid(Target) == false)
	{
		return;
	}
	
	if (bIsTracking)
	{
		CurrentTarget = Target;
		return;
	}
	
	APawn* Pawn = OwnerPawn.Get();
	UCharacterMovementComponent* MoveComp = MovementComp.Get();
	USpringArmComponent* SA = SpringArm.Get();
	if (IsValid(Pawn) == false || IsValid(MoveComp) == false || IsValid(SA) == false)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[LockOnCameraRig] StartTracking rejected: dependency invalid (Pawn=%d, MoveComp=%d, SpringArm=%d)"),
			IsValid(Pawn), IsValid(MoveComp), IsValid(SA));
		return;
	}
	
	APlayerController* PC = OwnerPC.Get();
	if (IsValid(PC) == false)
	{
		PC = Cast<APlayerController>(Pawn->GetController());
		OwnerPC = PC;                                         
	}
	
	if (IsValid(PC) == false)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[LockOnCameraRig] StartTracking rejected: PlayerController unavailable."));
		return;
	}

	if (bIsReturning == false)
	{
		bSavedOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
		bSavedUseControllerRotationYaw = Pawn->bUseControllerRotationYaw;
		SavedSocketOffset = SA->SocketOffset;
	}
	
	MoveComp->bOrientRotationToMovement = false;
	Pawn->bUseControllerRotationYaw = true;
	
	CurrentTarget = Target;
	bIsTracking = true;
	bIsReturning = false;
	SetComponentTickEnabled(true);
}

void ULockOnCameraRig::StopTracking()
{
	if (bIsTracking == false)
	{
		return;
	}
	
	APawn* Pawn = OwnerPawn.Get();
	UCharacterMovementComponent* MoveComp = MovementComp.Get();
	//USpringArmComponent* SA = SpringArm.Get();
	
	if (IsValid(MoveComp))
	{
		MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
	}

	if (IsValid(Pawn))
	{
		Pawn->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
	}

	// if (IsValid(SA))
	// {
	// 	SA->SocketOffset = SavedSocketOffset;
	// }
	
	CurrentTarget = nullptr;
	bIsTracking = false;
	bIsReturning = true;
	
	//SetComponentTickEnabled(false);
}
