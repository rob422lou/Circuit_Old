// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

	AActor* AttachedParentRoot;

	//<Child, Parent>. Child stores what it thinks it's attached to. Is actually attached to Root.
	TMultiMap<AActor*, AActor*> WeldGraph;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//////////////////////////////////////////////////////////////////////////
	// Weld Functions

	UFUNCTION(BlueprintCallable, Category = "Circuit|Constraint Weld")
	bool AddWeld(AActor* Actor1, AActor* Actor2);

	UFUNCTION(BlueprintCallable, Category = "Circuit|Constraint Weld")
	bool RemoveWeld(AActor* KeyActor, AActor* ValueActor);

	UFUNCTION(BlueprintCallable, Category = "Circuit|Constraint Weld")
	void RemoveAllWeldsFrom(AActor* Actor);

	bool IsDirectlyWeldedTo(AActor* Actor1, AActor* Actor2);

	// Looks for an indirect connection between 2 actors using a DFS algorithm
	bool IsIndirectlyWeldedTo(AActor* Actor1, AActor* Actor2);

	///////////////////////////////////////////////////
	// Helper Functions

	void CombineConstraintWeldComponents(UConstraintWeldComponent* AddingFrom);

	bool DFS(AActor* V, AActor* SearchingFor, TArray<AActor*>& Visited);

	UFUNCTION(BlueprintCallable, Category = "Circuit|Constraint Weld")
	UConstraintWeldComponent* GetConstraintWeldComponent(AActor* Actor);
	UConstraintWeldComponent* GetConstraintWeldComponentRoot(AActor* Actor);

	void RerootGraph();

	void DebugDrawWelds();

	UFUNCTION(BlueprintPure, Category = "Circuit|Debug")
	int DebugGetWeldGraphNum();

	UFUNCTION(BlueprintPure, Category = "Circuit|Debug")
	FString DebugGetAttachedParentRootName();
};
