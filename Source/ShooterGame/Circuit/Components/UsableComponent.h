// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "UsableComponent.generated.h"


class ACircuitCharacter;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SHOOTERGAME_API UUsableComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UUsableComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeginUse, ACircuitCharacter*, Instigator);
	UPROPERTY(BlueprintAssignable, Category = "Circuit|Interaction")
	FOnBeginUse OnBeginUse;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEndUse, ACircuitCharacter*, Instigator);
	UPROPERTY(BlueprintAssignable, Category = "Circuit|Interaction")
	FOnEndUse OnEndUse;
};
