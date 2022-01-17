// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Weapons/CircuitWeapon_Instant.h"

void ACircuitWeapon_Instant::OnEquipped_Implementation(const AShooterWeapon* LastWeapon)
{
	Super::OnEquip(LastWeapon);
}

void ACircuitWeapon_Instant::OnEquip(const AShooterWeapon* LastWeapon)
{
	OnEquipped(LastWeapon);
}

void ACircuitWeapon_Instant::OnEquippedFinished_Implementation()
{
	Super::OnEquipFinished();
	SetActorTickEnabled(true);
}

void ACircuitWeapon_Instant::OnEquipFinished()
{
	OnEquippedFinished();
}

void ACircuitWeapon_Instant::OnUnEquipped_Implementation()
{
	Super::OnUnEquip();
	SetActorTickEnabled(false);
}

void ACircuitWeapon_Instant::OnUnEquip()
{
	OnUnEquipped();
}

void ACircuitWeapon_Instant::StartMouseWheelUp_Implementation()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerStartMouseWheelUp();
	}
}

bool ACircuitWeapon_Instant::ServerStartMouseWheelUp_Validate()
{
	return true;
}

void ACircuitWeapon_Instant::ServerStartMouseWheelUp_Implementation()
{
	StartMouseWheelUp();
}

void ACircuitWeapon_Instant::StartMouseWheelDown_Implementation()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		ServerStartMouseWheelDown();
	}
}

bool ACircuitWeapon_Instant::ServerStartMouseWheelDown_Validate()
{
	return true;
}

void ACircuitWeapon_Instant::ServerStartMouseWheelDown_Implementation()
{
	StartMouseWheelDown();
}

void ACircuitWeapon_Instant::NextWeapon()
{
	MyPawn->OnNextWeapon();
}

void ACircuitWeapon_Instant::PrevWeapon()
{
	MyPawn->OnPrevWeapon();
}

FVector ACircuitWeapon_Instant::GetCameraAimVector() const
{
	return GetCameraAim();
}