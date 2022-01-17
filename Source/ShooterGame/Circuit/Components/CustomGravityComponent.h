// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Circuit/Actors/GravityActor.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"), Category = "Custom Gravity")
	FGravitySettings GravitySettings;

	UPROPERTY()
	TArray<AGravityActor*> GravityFieldArray;

	UFUNCTION()
	void AddToGravityFieldArray(AGravityActor* FieldToAdd);

	UFUNCTION()
	void RemoveFromGravityFieldArray(AGravityActor* FieldToRemove);

	// Time spent not moving. We don't need to add force if it's been sitting, that way it can sleep.
	float TimeSpentNotMoving = 0.0f;

	FVector LastPosition = FVector(0.0f, 0.0f, 0.0f);

	FRotator LastRotation;
};
