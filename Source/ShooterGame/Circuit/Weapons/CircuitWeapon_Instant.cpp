// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Weapons/CircuitWeapon_Instant.h"

FVector ACircuitWeapon_Instant::GetCameraAimVector() const
{
	return GetCameraAim();
}


void ACircuitWeapon_Instant::OnStartFire_Implementation()
{
	Super::StartFire();
}

void ACircuitWeapon_Instant::StartFire()
{
	OnStartFire();
}

void ACircuitWeapon_Instant::OnStopFire_Implementation()
{
	Super::StopFire();
}

void ACircuitWeapon_Instant::StopFire()
{
	OnStopFire();
}