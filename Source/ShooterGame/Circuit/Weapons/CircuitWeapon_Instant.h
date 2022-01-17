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

	//////////////////////////////////////////////////////////////////////////
	// Input Overrides
	
	/*
		The reason we are creating new functions with BP functionality is because you can't
		add BP functionality to functions you're overriding (afaik).
	*/

public:

	/** weapon is being equipped by owner pawn */
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnEquipped(const AShooterWeapon* LastWeapon);
	virtual void OnEquip(const AShooterWeapon* LastWeapon);
	
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnEquippedFinished();
	virtual void OnEquipFinished();

	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnUnEquipped();
	virtual void OnUnEquip();

	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void StartMouseWheelUp();

	UFUNCTION(reliable, server, WithValidation)
	void ServerStartMouseWheelUp();

	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void StartMouseWheelDown();

	UFUNCTION(reliable, server, WithValidation)
	void ServerStartMouseWheelDown();

public:
	//////////////////////////////////////////////////////////////////////////
	// Circuit Additions

	//Allow running and shooting?
	UPROPERTY(EditDefaultsOnly, Category = Config)
	bool bAllowRunAndGun = true;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void NextWeapon();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void PrevWeapon();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetCameraAimVector() const;
};