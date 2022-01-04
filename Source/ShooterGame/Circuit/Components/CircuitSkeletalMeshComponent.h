// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../Interfaces/SetValuesInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Circuit/CircuitHelper.h"
#include "CircuitSkeletalMeshComponent.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, ClassGroup = (Rendering, Common), hidecategories = Object, meta = (BlueprintSpawnableComponent))
class SHOOTERGAME_API UCircuitSkeletalMeshComponent : public USkeletalMeshComponent, public ISetValuesInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCircuitSkeletalMeshComponent();

protected:
	virtual void BeginPlay() override;

public:

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

/////////////////////////////////////////////////////////////////////////
// Property Replication

	// Original SetCollisionEnabled which needs replication
	virtual void SetCollisionEnabled(ECollisionEnabled::Type NewType) override;

	UFUNCTION()
	void OnRep_CollisionEnabledRep();
	UPROPERTY(ReplicatedUsing = OnRep_CollisionEnabledRep)
	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabledRep;

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerChangeCollisionEnabled(ECollisionEnabled::Type NewType);
	virtual bool ServerChangeCollisionEnabled_Validate(ECollisionEnabled::Type NewType) { return true; };
	virtual void ServerChangeCollisionEnabled_Implementation(ECollisionEnabled::Type NewType);
};
