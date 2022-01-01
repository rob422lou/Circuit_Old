// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SetValuesInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class USetValuesInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
 * 
 */
class SHOOTERGAME_API ISetValuesInterface
{
	GENERATED_IINTERFACE_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Collision")
	ECollisionEnabled::Type GetECollisionEnabled(FName VariableName);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Collision")
	void SetECollisionEnabled(FName VariableName, ECollisionEnabled::Type NewValue);
};
