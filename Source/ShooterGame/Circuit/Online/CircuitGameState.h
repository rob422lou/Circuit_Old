// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Online/ShooterGameState.h"
#include "CircuitGameState.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API ACircuitGameState : public AShooterGameState
{
	GENERATED_BODY()
	
public:

	/** number of teams in current game (doesn't deprecate when no players are left in a team) */
	UPROPERTY(Transient, Replicated, BlueprintReadOnly, Category = "Networking")
	bool bUsesCustomNetworking;

	// Used in Circuit actor position replication
	UPROPERTY(Transient, Replicated, BlueprintReadOnly, Category = "Networking")
	float ClientBufferTime;
	UPROPERTY(Transient, Replicated, BlueprintReadOnly, Category = "Networking")
	float ServerSnapshotTime;
};
