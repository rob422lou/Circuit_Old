// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/NoExportTypes.h"
#include "CircuitHelper.generated.h"

//Stencil colors, defined in PPI_StencilColor
UENUM(BlueprintType)
enum class EStencilColor : uint8
{
	SC_None = 0		UMETA(DisplayName = "None"),
	SC_LightBlue = 252 	UMETA(DisplayName = "Green"),
	SC_Red = 253	UMETA(DisplayName = "Light Blue"),
	SC_White = 254	UMETA(DisplayName = "Red") //255 is the max value
};

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API UCircuitHelper : public UObject
{
	GENERATED_BODY()
	
	UFUNCTION(BlueprintPure, Category = "Utilities", meta = (DisplayName = "Project Version", CompactNodeTitle = "ProjectVersion"))
	static FString GetProjectVersion();
};
