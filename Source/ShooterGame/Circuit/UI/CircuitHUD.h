// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ShooterHUD.h"
#include "CircuitHUD.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API ACircuitHUD : public AShooterHUD
{
	GENERATED_BODY()
	
	virtual void DrawHUD() override;

	/** Helper for drawing text-in-a-box. */
	void DrawDebugInfoString(const FString& Text, float PosX, float PosY, bool bAlignLeft, bool bAlignTop, const FColor& TextColor);
};
