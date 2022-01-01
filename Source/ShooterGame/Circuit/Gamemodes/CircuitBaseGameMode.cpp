// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Online/CircuitBaseGameState.h"
#include "Circuit/UI/CircuitHUD.h"
#include "Circuit/Player/CircuitPlayerController.h"
#include "Circuit/Gamemodes/CircuitBaseGameMode.h"

ACircuitBaseGameMode::ACircuitBaseGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnOb(TEXT("/Game/Circuit/Blueprints/Pawns/CircuitPlayerPawn"));
	DefaultPawnClass = PlayerPawnOb.Class;
	HUDClass = ACircuitHUD::StaticClass();
	PlayerControllerClass = ACircuitPlayerController::StaticClass();
	GameStateClass = ACircuitBaseGameState::StaticClass();

	bAllowBots = false;
	MinRespawnDelay = 2.0f;

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