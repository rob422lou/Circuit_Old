// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Online/CircuitGameState.h"
#include "Circuit/UI/CircuitHUD.h"
#include "Circuit/Gamemodes/CircuitBaseGameMode.h"

ACircuitBaseGameMode::ACircuitBaseGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnOb(TEXT("/Game/Circuit/Blueprints/Pawns/CircuitPlayerPawn"));
	DefaultPawnClass = PlayerPawnOb.Class;
	HUDClass = ACircuitHUD::StaticClass();
	GameStateClass = ACircuitGameState::StaticClass();

	bAllowBots = false;
	MinRespawnDelay = 2.0f;

	ClientBufferTime = 0.07f; //Time client waits to start interpolating so that it has time to build a buffer. (Default value is too high and slow)
	ServerSnapshotTime = 0.035f;   //Rate at which server samples and sends position data for interpolation. (Default value is too high and slow)

	//Defaults for networking debug
	PktLag = 0;
	PktLagVariance = 0;
	PktLoss = 0;
}

void ACircuitBaseGameMode::StartPlay()
{
	Super::StartPlay();

#if !UE_BUILD_SHIPPING
	APlayerController* PController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PController)
	{
		PController->ConsoleCommand(*FString::Printf(TEXT("Net PktLag=%i"), PktLag), true);
		PController->ConsoleCommand(*FString::Printf(TEXT("Net PktLagVariance=%i"), PktLagVariance), true);
		PController->ConsoleCommand(*FString::Printf(TEXT("Net PktLoss=%i"), PktLoss), true);
	}
#endif
}

void ACircuitBaseGameMode::InitGameState()
{
	Super::InitGameState();

	ACircuitGameState* const MyGameState = Cast<ACircuitGameState>(GameState);
	if (MyGameState)
	{
		MyGameState->bUsesCustomNetworking = bUsesCustomNetworking;
	}
}

//////////////////////////////////////////////////////////////////////////
// Utilities

FString ACircuitBaseGameMode::GetProjectVersion()
{
	FString ProjectVersion;

	GConfig->GetString(
		TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("ProjectVersion"),
		ProjectVersion,
		GGameIni
	);

	return ProjectVersion;
}