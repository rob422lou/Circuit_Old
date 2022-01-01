// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/CircuitSkeletalMeshComponent.h"

/////////////////////////////////////////////////////////////////////////
// Property Replication

void UCircuitSkeletalMeshComponent::SetCollisionEnabled(ECollisionEnabled::Type NewType) {
	if (!GetWorld() || !GetWorld()->IsGameWorld()) {
		return;
	}
	Multi_SetCollisionEnabled(NewType);
}

bool UCircuitSkeletalMeshComponent::Multi_SetCollisionEnabled_Validate(ECollisionEnabled::Type NewType) {
	return true;
}

void UCircuitSkeletalMeshComponent::Multi_SetCollisionEnabled_Implementation(ECollisionEnabled::Type NewType) {
	Super::SetCollisionEnabled(NewType);
}
