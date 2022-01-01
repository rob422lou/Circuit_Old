// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Online/ShooterGameMode.h"
#include "Player/ShooterPersistentUser.h"
#include "CircuitBaseGameMode.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API ACircuitBaseGameMode : public AShooterGameMode
{
	GENERATED_UCLASS_BODY()

	ACircuitBaseGameMode();

	virtual void StartPlay() override;

//////////////////////////////////////////////////////////////////////////
// Debug @TODO - Move me somewhere else
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Packet Lag"))
	int32 PktLag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Packet Lag Variance"))
	int32 PktLagVariance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Packet Loss"))
	int32 PktLoss;
};
