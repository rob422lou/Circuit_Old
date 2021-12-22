// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/CircuitConstraintComponent.h"

UCircuitConstraintComponent::UCircuitConstraintComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

// Called every frame
void UCircuitConstraintComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	UPrimitiveComponent* comp1;
	UPrimitiveComponent* comp2;
	this->GetConstrainedComponents(comp1, ConstraintInstance.ConstraintBone1, comp2, ConstraintInstance.ConstraintBone2);

	if (comp1 != nullptr && comp2 != nullptr) {
		DebugDrawConstraints(comp1, comp2);
	}
}

FConstraintInstance UCircuitConstraintComponent::GetConstraintInstance() {
	return ConstraintInstance;
}

void UCircuitConstraintComponent::DebugDrawConstraints(UPrimitiveComponent* comp1, UPrimitiveComponent* comp2) {
	DrawDebugBox(GetWorld(), GetComponentLocation(), FVector(65.0f, 65.0f, 65.0f), FColor::Black, false, -1.0f, 0, 2.0f);
	DrawDebugBox(GetWorld(), comp1->GetComponentLocation(), FVector(55.0f, 55.0f, 55.0f), FColor::Orange, false, -1.0f, 0, 2.0f);
	DrawDebugBox(GetWorld(), comp2->GetComponentLocation(), FVector(55.0f, 55.0f, 55.0f), FColor::Purple, false, -1.0f, 0, 2.0f);

	//DrawDebugDirectionalArrow(GetWorld(), comp2->GetComponentLocation(), comp1->GetComponentLocation(), 15.0f, FColor::Silver, false, -1.0f, 0, 1);
	DrawDebugDirectionalArrow(GetWorld(), this->GetComponentLocation(), comp1->GetComponentLocation(), 15.0f, FColor::Emerald, false, -1.0f, 0, 1);
	DrawDebugDirectionalArrow(GetWorld(), this->GetComponentLocation(), comp2->GetComponentLocation(), 15.0f, FColor::Magenta, false, -1.0f, 0, 1);
}