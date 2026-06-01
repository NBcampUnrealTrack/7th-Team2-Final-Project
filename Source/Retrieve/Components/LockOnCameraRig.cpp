

#include "LockOnCameraRig.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Data/LockOnCameraConfig.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

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
		if (IsValid(Pawn) == false || IsValid(PC) == false 
			|| IsValid(Target) == false
			|| IsValid(SA) == false || IsValid(Config) == false)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[LockOnCameraRig] Tracking stopped: runtime reference became invalid (Pawn=%d, PC=%d, Target=%d, SpringArm=%d, Config=%d)"),
				IsValid(Pawn), IsValid(PC), IsValid(Target), IsValid(SA), IsValid(Config));
			StopTracking();
			return;
		}
	
		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);
	
		const FVector TargetLoc = Target->GetActorLocation() + FVector(0.f, 0.f, Config->TargetVerticalOffset);
		FRotator DesiredRot = (TargetLoc - CamLoc).Rotation();
		DesiredRot.Roll = 0.f;
		DesiredRot.Pitch = FMath::ClampAngle(DesiredRot.Pitch, Config->MinPitch, Config->MaxPitch);

		const FRotator CurrentRot = PC->GetControlRotation();
		const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, Config->CameraInterpSpeed);
		PC->SetControlRotation(NewRot);

		const FVector NewOffset = FMath::VInterpTo(SA->SocketOffset, Config->LockOnSocketOffset, DeltaTime, Config->OffsetInterpSpeed);
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
		const float OffsetSpeed = IsValid(Config) ? Config->OffsetInterpSpeed : 6.f;
		const FVector NewOffset = FMath::VInterpTo(SA->SocketOffset, SavedSocketOffset, DeltaTime, OffsetSpeed);
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

void ULockOnCameraRig::SetConfig(ULockOnCameraConfig* InConfig)
{
	if (IsValid(InConfig) == false)
	{
		return;
	}
	Config = InConfig;
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
	if (IsValid(Pawn) == false || IsValid(MoveComp) == false || IsValid(SA) == false || IsValid(Config) == false)
	{
		UE_LOG(LogTemp, Error,
	TEXT("[LockOnCameraRig] StartTracking rejected: dependency invalid (Pawn=%d, MoveComp=%d, SpringArm=%d, Config=%d)"),
	IsValid(Pawn), IsValid(MoveComp), IsValid(SA), IsValid(Config));
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
		SavedSocketOffset = SA->SocketOffset;
	}

	// 회전 모드 전환은 GAS 태그로. 캐릭터의 OnLockOnTagChanged 콜백이
	// 자동으로 SetDesiredRotationMode(ViewDirection)를 호출합니다.
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
	{
		ASC->AddLooseGameplayTag(RetrieveGameplayTags::LockOn_Active);
	}

	CurrentTarget = Target;
	bIsTracking = true;
	bIsReturning = false;
	SetComponentTickEnabled(true);
}

void ULockOnCameraRig::StopTracking(bool bImmediateRestore)
{
	if (bIsTracking == false && bIsReturning == false)
	{
		return;
	}
	
	const bool bWasTracking = bIsTracking;

	APawn* Pawn = OwnerPawn.Get();
	USpringArmComponent* SA = SpringArm.Get();

	if (bWasTracking && IsValid(Pawn))
	{
		// 회전 모드 복귀는 GAS 태그 제거로. 캐릭터 콜백이 자동으로 VelocityDirection 복귀.
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
		{
			ASC->RemoveLooseGameplayTag(RetrieveGameplayTags::LockOn_Active);
		}
	}

	CurrentTarget = nullptr;
	bIsTracking = false;
	if (bImmediateRestore)
	{
		if (IsValid(SA))
		{
			SA->SocketOffset = SavedSocketOffset;
		}

		bIsReturning = false;
		SetComponentTickEnabled(false);
		return;
	}

	if (IsValid(SA))
	{
		bIsReturning = true;
		SetComponentTickEnabled(true);
	}
	else
	{
		bIsReturning = false;
		SetComponentTickEnabled(false);
	}
	//SetComponentTickEnabled(false);
}
