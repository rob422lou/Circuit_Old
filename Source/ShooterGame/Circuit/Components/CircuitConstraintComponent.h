// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "CircuitConstraintComponent.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = Physics, meta = (BlueprintSpawnableComponent), HideCategories = (Activation, "Components|Activation", Physics, Mobility), ShowCategories = ("Physics|Components|PhysicsConstraint"))
class SHOOTERGAME_API UCircuitConstraintComponent : public UPhysicsConstraintComponent
{
	GENERATED_BODY()
	
	UCircuitConstraintComponent();

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	///////////////////////////////////////////////////
	// Helper Functions

	FConstraintInstance GetConstraintInstance();

	void DebugDrawConstraints(UPrimitiveComponent* comp1, UPrimitiveComponent* comp2);
};
