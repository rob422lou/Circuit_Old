// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/CircuitSkeletalMeshComponent.h"

UCircuitSkeletalMeshComponent::UCircuitSkeletalMeshComponent() {
	//SetIsReplicatedByDefault(true);
}

void UCircuitSkeletalMeshComponent::BeginPlay() {
	Super::BeginPlay();
}

// Called every frame
void UCircuitSkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

/////////////////////////////////////////////////////////////////////////
// Property Replication

void UCircuitSkeletalMeshComponent::OnRep_CollisionEnabledRep() {
	Super::SetCollisionEnabled(CollisionEnabledRep);
}

void UCircuitSkeletalMeshComponent::SetCollisionEnabled(ECollisionEnabled::Type NewType) {
	if (!GetWorld() || !GetWorld()->IsGameWorld()) {
		return;
	}

	// Client only, pass new value to server.
	if (GetOwner()->GetLocalRole() != ENetRole::ROLE_Authority) {
		ServerChangeCollisionEnabled(NewType);
	}
	else {
		CollisionEnabledRep = NewType;
		Super::SetCollisionEnabled(NewType);
	}
}

void UCircuitSkeletalMeshComponent::ServerChangeCollisionEnabled_Implementation(ECollisionEnabled::Type NewType) {
	CollisionEnabledRep = NewType;
}

void UCircuitSkeletalMeshComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCircuitSkeletalMeshComponent, CollisionEnabledRep);
}
