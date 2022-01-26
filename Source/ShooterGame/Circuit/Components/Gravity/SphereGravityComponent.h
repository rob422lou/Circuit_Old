// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Circuit/Components/Gravity/BaseGravityComponent.h"
#include "SphereGravityComponent.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SHOOTERGAME_API USphereGravityComponent : public UBaseGravityComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	USphereGravityComponent();

public:
	UFUNCTION()
	virtual FVector CalculateGravity(FVector WorldPosition) override;
};
