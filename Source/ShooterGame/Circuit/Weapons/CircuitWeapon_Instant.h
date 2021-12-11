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

//////////////////////////////////////////////////////////////////////////
// Input Overrides

/*
	The reason we are creating new functions with BP functionality is because you can't
	add BP functionality to functions you're overriding (afaik).
*/

/** weapon is being equipped by owner pawn */
public:
	/** [local + server] start weapon fire */
	UFUNCTION(BlueprintNativeEvent, Category = "Circuit|Weapon")
	void OnStartFire();
	virtual void StartFire() override;

	/** [local + server] stop weapon fire */
	UFUNCTION(BlueprintNativeEvent, Category = "Circuit|Weapon")
	void OnStopFire();
	virtual void StopFire() override;
};
