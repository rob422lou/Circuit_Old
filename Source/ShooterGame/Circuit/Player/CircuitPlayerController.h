// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/ShooterPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "CircuitPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API ACircuitPlayerController : public AShooterPlayerController
{
	GENERATED_UCLASS_BODY()

	ACircuitPlayerController();

public:
	TSubclassOf<class UUserWidget> PauseMenuClass;

	UPROPERTY()
	UUserWidget* PauseMenuUI;

	bool bIsPauseMenuShowing;

	virtual void BeginPlay() override;

	//UFUNCTION(BlueprintPure)
	//float GetUIScale();

	//UFUNCTION(BlueprintCallable)
	void HideInGameMenu();

	// Requires alteration of ShooterPlayerController.h
	virtual void OnToggleInGameMenu() override;

	//UFUNCTION(BlueprintCallable)
	//void SetUIScale(float Scale);

	// Requires alteration of ShooterPlayerController.h
	virtual void ShowInGameMenu() override;
};
