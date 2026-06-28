// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryPreviewAnimInstance.h"

void UInventoryPreviewAnimInstance::SetSourceMeshComponent(USkeletalMeshComponent* InSourceMeshComponent)
{
	SourceMeshComponent = InSourceMeshComponent;
}

bool UInventoryPreviewAnimInstance::PlayPreviewActionMontage(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		return false;
	}
	
	return Montage_Play(Montage, FMath::Max(PlayRate, 0.01f)) > 0.0f;
}
