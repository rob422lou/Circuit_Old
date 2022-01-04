// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CustomGravityComponent.generated.h"

UENUM(BlueprintType)
namespace EGravityType
{
	enum  Type
	{
		EGT_Default 	UMETA(DisplayName = "Default Gravity"),
		EGT_Point 	UMETA(DisplayName = "Point Gravity"),
		EGT_Directional	UMETA(DisplayName = "Directional Gravity"),
		EGT_Custom 	UMETA(DisplayName = "Custom Gravity"),
		EGT_Planet 	UMETA(DisplayName = "Planet Gravity")
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
	bool bUseAccel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bForceSubStepping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EGravityType::Type> GravityType;

	FGravitySettings()
	{
		GravityPower = 980.0f;
		GravityDirection = FVector(0.0f, 0.0f, -1.0f);
		bUseAccel = true;
		bForceSubStepping = true;
		GravityType = EGravityType::EGT_Default;
	}

	FGravitySettings(float NewGravityPower, FVector NewGravityDirection, bool bNewAccel, bool bShouldUseStepping, TEnumAsByte<EGravityType::Type> NewGravityType)
	{
		GravityPower = NewGravityPower;
		GravityDirection = NewGravityDirection;
		bUseAccel = bNewAccel;
		bForceSubStepping = bShouldUseStepping;
		GravityType = NewGravityType;
	}

};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SHOOTERGAME_API UCustomGravityComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCustomGravityComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	virtual void InitializeComponent() override;
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool bIsSkeletalMesh = false;

	UPrimitiveComponent* EffectedComponent;
	USkeletalMeshComponent* EffectedSkeletalComponent; // Using this so not constantly casting

	float ComponentWeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"), Category = "Custom Gravity")
	FGravitySettings GravitySettings;
};
