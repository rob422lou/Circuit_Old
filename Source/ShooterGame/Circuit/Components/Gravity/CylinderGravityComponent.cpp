// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/Gravity/CylinderGravityComponent.h"

// Sets default values for this component's properties
UCylinderGravityComponent::UCylinderGravityComponent()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Game/Circuit/Meshes/Gravity/GravityCylinder.GravityCylinder"));
    UStaticMesh* Asset = MeshAsset.Object;

    if (Asset) {
        SetStaticMesh(Asset);
    }

    Range = 1000.0f; // @TODO - dynamically create
}

FVector UCylinderGravityComponent::CalculateGravity(FVector WorldPosition) {
    if (Range <= 0.0f) {
        Range = 1.0f;
    }

    if (GravityFieldType == EGravityFieldType::EGT_Directional) {
        return DirectionalGravityDirection * GravityStrength;
    }
    else {
        float Falloff = 1.0f;
        if (bHasFalloff) {
            Falloff = (1.0f - ((GetComponentLocation() - WorldPosition).Size() / Range));
        }
        if (bIsInverted) {
            return (FMath::ClosestPointOnInfiniteLine(GetComponentLocation(), GetComponentLocation() + GetUpVector(), WorldPosition) - WorldPosition).GetSafeNormal() * GravityStrength * Falloff * -1.0f;
        }

        return (FMath::ClosestPointOnInfiniteLine(GetComponentLocation(), GetComponentLocation() + GetUpVector(), WorldPosition) - WorldPosition).GetSafeNormal() * GravityStrength * Falloff;
    }
}