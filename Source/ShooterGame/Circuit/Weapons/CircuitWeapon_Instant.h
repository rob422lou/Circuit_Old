// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/ShooterWeapon_Instant.h"
#include "CircuitWeapon_Instant.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API ACircuitWeapon_Instant : public AShooterWeapon_Instant
{
	GENERATED_BODY()
	
public:
	//////////////////////////////////////////////////////////////////////////
	// Circuit Additions

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetCameraAimVector() const;
};
