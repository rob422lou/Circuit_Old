// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "Player/ShooterPlayerCameraManager.h"

AShooterPlayerCameraManager::AShooterPlayerCameraManager(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NormalFOV = 90.0f;
	TargetingFOV = 60.0f;
	ViewPitchMin = -87.0f;
	ViewPitchMax = 87.0f;
	bAlwaysApplyModifiers = true;

	ViewRollMin = 0.0f;
	ViewRollMax = 360.0f;
}

void AShooterPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	AShooterCharacter* MyPawn = PCOwner ? Cast<AShooterCharacter>(PCOwner->GetPawn()) : NULL;
	if (MyPawn && MyPawn->IsFirstPerson())
	{
		const float TargetFOV = MyPawn->IsTargeting() ? TargetingFOV : NormalFOV;
		DefaultFOV = FMath::FInterpTo(DefaultFOV, TargetFOV, DeltaTime, 20.0f);
	} 


	if (ViewTarget.GetTargetPawn()) {

		// Shows correct roll
		//UE_LOG(LogTemp, Error, TEXT("[%f] AShooterPlayerCameraManager POV TP: %f %f AR: %f %f %s"), GetWorld()->GetRealTimeSeconds(), ViewTarget.GetTargetPawn()->GetViewRotation().Roll, ViewTarget.GetTargetPawn()->GetViewRotation().Yaw, ViewTarget.GetTargetPawn()->GetActorRotation().Roll, ViewTarget.GetTargetPawn()->GetActorRotation().Yaw, *ViewTarget.GetTargetPawn()->GetName());
	}

	Super::UpdateCamera(DeltaTime); // Camera flips correctly without this statement

	// Shows incorrect roll
	//UE_LOG(LogTemp, Error, TEXT("[%f] AShooterPlayerCameraManager POV 2 %f %f %f"), GetWorld()->GetRealTimeSeconds(), ViewTarget.POV.Rotation.Pitch, ViewTarget.POV.Rotation.Yaw, ViewTarget.POV.Rotation.Roll);



	if (MyPawn && MyPawn->IsFirstPerson())
	{
		//MyPawn->OnCameraUpdate(GetCameraLocation(), GetCameraRotation());
	}
}
