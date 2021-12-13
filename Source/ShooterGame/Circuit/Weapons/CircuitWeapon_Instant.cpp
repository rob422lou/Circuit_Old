// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Weapons/CircuitWeapon_Instant.h"

FVector ACircuitWeapon_Instant::GetCameraAimVector() const
{
	return GetCameraAim();
}