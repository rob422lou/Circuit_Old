// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Circuit/Actors/GravityActor.h"
#include "Circuit/Components/Gravity/BaseGravityComponent.h"
#include "Circuit/Player/CircuitCharacter.h"
#include "Circuit/CircuitHelper.h"
#include "CustomGravityComponent.generated.h"

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

	ACircuitCharacter* EffectedCharacter;
	UPrimitiveComponent* EffectedComponent;
	USkeletalMeshComponent* EffectedSkeletalComponent; // Using this so not constantly casting

	float ComponentWeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"), Category = "Gravity")
	FVector CurrentGravityDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"), Category = "Gravity")
	float CurrentGravityStrength;

	UPROPERTY()
	TArray<UBaseGravityComponent*> GravityFieldArray;

	UPROPERTY()
	TArray<UBaseGravityComponent*> AdditiveGravityFieldArray;

	UFUNCTION()
	void AddToGravityFieldArray(UBaseGravityComponent* FieldToAdd);

	UFUNCTION()
	void RemoveFromGravityFieldArray(UBaseGravityComponent* FieldToRemove);

	UFUNCTION()
	void AddToAdditiveGravityFieldArray(UBaseGravityComponent* FieldToAdd);

	UFUNCTION()
	void RemoveFromAdditiveGravityFieldArray(UBaseGravityComponent* FieldToRemove);

	UFUNCTION()
	void CalculateCurrentGravity();

	UFUNCTION()
	FVector GetGravityDirection();

	UFUNCTION()
	float GetGravityStrength();

	// Time spent not moving. We don't need to add force if it's been sitting, that way it can sleep. Only used by physics bodies
	float TimeSpentNotMoving = 0.0f;

	// Used in putting physics actors to sleep after not moving
	FRotator LastRotation;

	// Used in putting physics actors to sleep after not moving
	FVector LastPosition = FVector(0.0f, 0.0f, 0.0f);
};
