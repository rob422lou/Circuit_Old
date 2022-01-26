// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/CircuitCapsuleComponent.h"

/////////////////////////////////////////////////////////////////////////
// Property Replication

void UCircuitCapsuleComponent::SetCollisionEnabled(ECollisionEnabled::Type NewType) {
	if (!GetWorld() || !GetWorld()->IsGameWorld()) {
		return;
	}
	Multi_SetCollisionEnabled(NewType);
}

bool UCircuitCapsuleComponent::Multi_SetCollisionEnabled_Validate(ECollisionEnabled::Type NewType) {
	return true;
}

void UCircuitCapsuleComponent::Multi_SetCollisionEnabled_Implementation(ECollisionEnabled::Type NewType) {
	Super::SetCollisionEnabled(NewType);
	//FComponentBeginOverlapSignature
}
