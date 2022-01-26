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

UENUM(BlueprintType)
namespace EGravityType
{
	enum  Type
	{
		EGT_Default 		UMETA(DisplayName = "Default Gravity"),
		EGT_Point 			UMETA(DisplayName = "Point Gravity"),
		EGT_Directional		UMETA(DisplayName = "Directional Gravity"),
		EGT_Custom 			UMETA(DisplayName = "Custom Gravity"),
		EGT_Planet 			UMETA(DisplayName = "Planet Gravity")
	};

}

/** Struct to hold information about the "Gravity Type" . */
USTRUCT(BlueprintType)
struct  SHOOTERGAME_API FGravitySettings
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GravityPower;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector GravityDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsAdditive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasFalloff;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EGravityType::Type> GravityType;

	FGravitySettings()
	{
		GravityPower = 980.0f;
		GravityDirection = FVector(0.0f, 0.0f, -1.0f);
		bIsAdditive = false;
		GravityType = EGravityType::EGT_Default;
	}

	FGravitySettings(float NewGravityPower, FVector NewGravityDirection, TEnumAsByte<EGravityType::Type> NewGravityType)
	{
		GravityPower = NewGravityPower;
		GravityDirection = NewGravityDirection;
		bIsAdditive = false; // @TODO - fix this
		GravityType = NewGravityType;
	}

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
