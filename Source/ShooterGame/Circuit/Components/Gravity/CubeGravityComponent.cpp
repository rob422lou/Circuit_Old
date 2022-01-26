// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/Gravity/CubeGravityComponent.h"

// Sets default values for this component's properties
UCubeGravityComponent::UCubeGravityComponent()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Game/Circuit/Meshes/Gravity/GravityCube.GravityCube"));
    UStaticMesh* Asset = MeshAsset.Object;

    if (Asset) {
        SetStaticMesh(Asset);
    }

    Range = 1000.0f; // @TODO - dynamically create
}

FVector UCubeGravityComponent::CalculateGravity(FVector WorldPosition) {
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
            return (GetComponentLocation() - WorldPosition).GetSafeNormal() * GravityStrength * -1.0f * Falloff;
        }
        return (GetComponentLocation() - WorldPosition).GetSafeNormal() * GravityStrength * Falloff;
    }
}