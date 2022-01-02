// Fill out your copyright notice in the Description page of Project Settings.
// @TODO - Add customizable exit points
// @TODO - Expand to allow multiple seats

#pragma once

#include "CoreMinimal.h"
#include "Circuit/Components/UsableComponent.h"
#include "Circuit/Player/CircuitCharacter.h"
#include "WheeledVehiclePawn.h"
#include "CircuitWheeledVehiclePawn.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API ACircuitWheeledVehiclePawn : public AWheeledVehiclePawn
{
	GENERATED_UCLASS_BODY()

	virtual void PostInitializeComponents() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);
public:

	/** Base lookup rate, in deg/sec. Other scaling may affect final lookup rate. */
	float BaseLookUpRate;

	UPROPERTY()
	UUsableComponent* UsableComp;

	UPROPERTY()
	ACircuitCharacter* DrivingPawn;

	// Returns true if player has space to leave vehicle
	//UPROPERTY(BlueprintCallable, Category = "Vehicle|Utility")
	bool CanPlayerExitVehicle(ACircuitCharacter* ExitingPlayer);

	void PlayerExitVehicle(ACircuitCharacter* ExitingPlayer);

	void OnUsePress();

	UFUNCTION(BlueprintNativeEvent, Category = PlayerAbility)
	void OnUse(ACircuitCharacter* InstigatingPlayer);

	UFUNCTION(reliable, server, WithValidation)
	void ServerOnUse(ACircuitCharacter* InstigatingPlayer);

	UFUNCTION(BlueprintNativeEvent, Category = PlayerAbility)
	void OnStartEnter(ACircuitCharacter* InstigatingPlayer);

	UFUNCTION(reliable, server, WithValidation)
	void ServerOnStartEnter(ACircuitCharacter* InstigatingPlayer);

	UFUNCTION(BlueprintNativeEvent, Category = PlayerAbility)
	void OnStartExit(ACircuitCharacter* InstigatingPlayer);

	UFUNCTION(reliable, server, WithValidation)
	void ServerOnStartExit(ACircuitCharacter* InstigatingPlayer);

	virtual void AddControllerPitchInput(float Val) override;
};
