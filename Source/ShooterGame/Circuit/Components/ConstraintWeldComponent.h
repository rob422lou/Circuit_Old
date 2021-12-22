// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Circuit/Components/CircuitConstraintComponent.h"
#include "Components/SceneComponent.h"
#include "ConstraintWeldComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SHOOTERGAME_API UConstraintWeldComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UConstraintWeldComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	USceneComponent* AttachedParentRoot;

	//<Child, Parent>. Child stores what it thinks it's attached to. Is actually attached to Root.
	TMultiMap<UPrimitiveComponent*, UPrimitiveComponent*> WeldGraph;
	TMultiMap<UPrimitiveComponent*, UCircuitConstraintComponent*> ConstraintGraph;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	//////////////////////////////////////////////////////////////////////////
	// Constraints
	 
	bool AddConstraint(UPrimitiveComponent* Comp1, UPrimitiveComponent* Comp2, FName Bone1, FName Bone2);

	bool DoesConstraintExist(UPrimitiveComponent* Comp1, UPrimitiveComponent* Comp2, FName Bone1, FName Bone2);

	//////////////////////////////////////////////////////////////////////////
	// Weld Functions

	UFUNCTION(BlueprintCallable, Category = "Circuit|Constraint Weld")
	bool AddWeld(UPrimitiveComponent* Comp1, UPrimitiveComponent* Comp2, FName Bone1, FName Bone2);

	UFUNCTION(BlueprintCallable, Category = "Circuit|Constraint Weld")
	bool RemoveWeld(UPrimitiveComponent* KeyComp, UPrimitiveComponent* ValueComp, FName Bone1, FName Bone2);

	UFUNCTION(BlueprintCallable, Category = "Circuit|Constraint Weld")
	void RemoveAllWeldsFrom(UPrimitiveComponent* Comp);

	bool IsDirectlyWeldedTo(UPrimitiveComponent* Comp1, UPrimitiveComponent* Comp2, FName Bone1, FName Bone2);

	// Looks for an indirect connection between 2 actors using a DFS algorithm
	bool IsIndirectlyWeldedTo(UPrimitiveComponent* Comp1, UPrimitiveComponent* Comp2, FName Bone1, FName Bone2);

	///////////////////////////////////////////////////
	// Helper Functions

	void CombineConstraintWeldComponents(UConstraintWeldComponent* AddingFrom);

	bool DFS(UPrimitiveComponent* V, UPrimitiveComponent* SearchingFor, TArray<UPrimitiveComponent*>& Visited);

	UFUNCTION(BlueprintCallable, Category = "Circuit|Constraint Weld")
	UConstraintWeldComponent* GetConstraintWeldComponent(USceneComponent* Comp);
	UConstraintWeldComponent* GetConstraintWeldComponentRoot(USceneComponent* Comp);

	USkeletalMeshComponent* GetSkeletalMeshComponent(UPrimitiveComponent* Comp);

	void RerootGraph();

	void DebugDrawWelds();

	UFUNCTION(BlueprintPure, Category = "Circuit|Debug")
	int DebugGetWeldGraphNum();

	UFUNCTION(BlueprintPure, Category = "Circuit|Debug")
	FString DebugGetAttachedParentRootName();
};
