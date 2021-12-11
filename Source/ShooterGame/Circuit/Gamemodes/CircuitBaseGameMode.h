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

	/** initialize replicated game data */
	virtual void InitGameState() override;

public:
	//////////////////////////////////////////////////////////////////////////
	// Buffer Related

		//[Clients] How long to buffer before updating position. Higher values better for packet loss.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Networking")
	bool bUsesCustomNetworking = true;

	//[Clients] How long to buffer before updating position. Higher values better for packet loss.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Networking")
	float ClientBufferTime;

	//[Server] Rate server sends out position updates to clients.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Networking")
	float ServerSnapshotTime;

	//////////////////////////////////////////////////////////////////////////
	// Utilities

	UFUNCTION(BlueprintPure, Category = "Utilities", meta = (DisplayName = "Project Version", CompactNodeTitle = "ProjectVersion"))
	static FString GetProjectVersion();

	// @todo - move this somewhere more appropriate
	//UFUNCTION(BlueprintPure, Category = "Utilities")
	//static FString GetCurrentScreenResolution();

	// @todo - move this somewhere more appropriate
	//UFUNCTION(BlueprintPure, Category = "Utilities")
	//static bool GetIsFullscreen();

//////////////////////////////////////////////////////////////////////////
// Debug
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Packet Lag"))
		int32 PktLag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Packet Lag Variance"))
		int32 PktLagVariance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (DisplayName = "Packet Loss"))
		int32 PktLoss;
};
