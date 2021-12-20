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
	UE_LOG(LogTemp, Warning, TEXT("UCircuitConstraintComponent - TickComponent()"));

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	FName temp;
	UPrimitiveComponent* comp1;
	UPrimitiveComponent* comp2;
	this->GetConstrainedComponents(comp1, temp, comp2, temp);
	if (comp1 != nullptr && comp2 != nullptr) {
		DebugDrawConstraints(comp1, comp2);
	}
}

FConstraintInstance UCircuitConstraintComponent::GetConstraintInstance() {
	return ConstraintInstance;
}

void UCircuitConstraintComponent::DebugDrawConstraints(UPrimitiveComponent* comp1, UPrimitiveComponent* comp2) {
	DrawDebugBox(GetWorld(), comp1->GetComponentLocation(), FVector(55.0f, 55.0f, 55.0f), FColor::Orange, false, -1.0f, 0, 2.0f);
	DrawDebugBox(GetWorld(), comp2->GetComponentLocation(), FVector(55.0f, 55.0f, 55.0f), FColor::Purple, false, -1.0f, 0, 2.0f);

	DrawDebugDirectionalArrow(GetWorld(), this->GetComponentLocation(), comp1->GetComponentLocation(), 15.0f, FColor::Emerald, false, -1.0f, 0, 1);
	DrawDebugDirectionalArrow(GetWorld(), this->GetComponentLocation(), comp2->GetComponentLocation(), 15.0f, FColor::Magenta, false, -1.0f, 0, 1);
}