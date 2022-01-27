// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Runtime/Engine/Classes/Engine/UserInterfaceSettings.h"
#include "Circuit/Player/CircuitPlayerController.h"

ACircuitPlayerController::ACircuitPlayerController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UUserWidget> MenuClassFinder(TEXT("WidgetBlueprint'/Game/Circuit/UI/MainMenu/PauseMenu.PauseMenu_C'"));
	PauseMenuClass = MenuClassFinder.Class;
	bIsPauseMenuShowing = false;
}

void ACircuitPlayerController::BeginPlay()
{
	Super::BeginPlay();

	//UE_LOG(LogTemp, Warning, TEXT("[%f] ACircuitPlayerController BeginPlay"), GetWorld()->GetRealTimeSeconds());

	PauseMenuUI = CreateWidget<UUserWidget>(this, PauseMenuClass);
}

/*
float ACircuitPlayerController::GetUIScale()
{
	UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
	return UISettings->ApplicationScale;
}
*/
void ACircuitPlayerController::HideInGameMenu()
{
	if (PauseMenuUI) {
		bIsPauseMenuShowing = false;

		SetCinematicMode(false, false, false, true, true);

		PauseMenuUI->RemoveFromParent();

		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}
}

// @todo - add "go back" functionality that accounts for options menu being open. OnToggle/HideInGameMenu should close options menu first.
void ACircuitPlayerController::OnToggleInGameMenu()
{
	if (GEngine->GameViewport == nullptr)
	{
		return;
	}

	// this is not ideal, but necessary to prevent both players from pausing at the same time on the same frame
	UWorld* GameWorld = GEngine->GameViewport->GetWorld();

	for (auto It = GameWorld->GetControllerIterator(); It; ++It)
	{
		ACircuitPlayerController* Controller = Cast<ACircuitPlayerController>(*It);
		if (Controller && Controller->IsPaused())
		{
			return;
		}
	}

	// if no one's paused, pause
	if (PauseMenuUI)
	{
		if (!bIsPauseMenuShowing) {
			bIsPauseMenuShowing = true;

			SetCinematicMode(true, false, false, true, true);

			//PauseMenuUI->IsInViewport();
			PauseMenuUI->AddToViewport();

			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(PauseMenuUI->TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

			SetInputMode(InputModeData);
			bShowMouseCursor = true;
		}
		else {
			bIsPauseMenuShowing = false;

			SetCinematicMode(false, false, false, true, true);

			PauseMenuUI->RemoveFromParent();

			FInputModeGameOnly InputMode;
			SetInputMode(InputMode);
			bShowMouseCursor = false;
		}
	}
}

/*
void ACircuitPlayerController::SetUIScale(float Scale)
{
	UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
	UISettings->ApplicationScale = FMath::Clamp(Scale, 0.25f, 2.0f);
}
*/

void ACircuitPlayerController::ShowInGameMenu()
{
	AShooterHUD* ShooterHUD = GetShooterHUD();
	if (PauseMenuUI && ShooterHUD)
	{
		OnToggleInGameMenu();
	}
}