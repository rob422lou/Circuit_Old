// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "CircuitCapsuleComponent.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API UCircuitCapsuleComponent : public UCapsuleComponent
{
	GENERATED_BODY()

public:

/////////////////////////////////////////////////////////////////////////
// Property Replication

	virtual void SetCollisionEnabled(ECollisionEnabled::Type NewType) override;

	UFUNCTION(NetMulticast, Reliable)
	void Multi_SetCollisionEnabled(ECollisionEnabled::Type NewType);
	bool Multi_SetCollisionEnabled_Validate(ECollisionEnabled::Type NewType);
	void Multi_SetCollisionEnabled_Implementation(ECollisionEnabled::Type NewType);
};
