// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/CustomGravityComponent.h"
#include "Circuit/Components/Gravity/BaseGravityComponent.h"

// Sets default values for this component's properties
UBaseGravityComponent::UBaseGravityComponent()
{
    GravityStrength = 980.0f;
    bHasFalloff = false;
    bIsAdditive = false;
    DirectionalGravityDirection = FVector(0.0f, 0.0f, -1.0f);
    GravityFieldType = EGravityFieldType::EGT_Directional;
    Range = 1000.0f; // @TODO - dynamically create

    SetGenerateOverlapEvents(true);

    SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);

    PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void UBaseGravityComponent::BeginPlay()
{
    Super::BeginPlay();

    //UE_LOG(LogTemp, Warning, TEXT("[%f] UBaseGravityComponent BeginPlay"), GetWorld()->GetRealTimeSeconds());

    if (GetStaticMesh() == nullptr) {
        UE_LOG(LogTemp, Warning, TEXT("[%f] UBaseGravityComponent GetStaticMesh() == nullptr"), GetWorld()->GetRealTimeSeconds());
        return;
    }

    OnComponentBeginOverlap.AddDynamic(this, &UBaseGravityComponent::OnOverlapBegin);
    OnComponentEndOverlap.AddDynamic(this, &UBaseGravityComponent::OnOverlapEnd);

    // Delay is needed because of the order UE generates objects
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [&]()
        {
            TSet<UPrimitiveComponent*> OverlappingComponents;
            GetOverlappingComponents(OverlappingComponents);

            for (UPrimitiveComponent* Element : OverlappingComponents)
            {
                TArray<USceneComponent*> Components = Element->GetAttachChildren();

                for (size_t i = 0; i < Components.Num(); i++)
                {
                    UCustomGravityComponent* GravityComp = Cast<UCustomGravityComponent>(Components[i]);

                    if (GravityComp) {
                        if (bIsAdditive) {
                            GravityComp->AddToAdditiveGravityFieldArray(this);
                        }
                        else {
                            GravityComp->AddToGravityFieldArray(this);
                        }
                        break;
                    }
                }
            }
        }, 0.1f, false);
}

void UBaseGravityComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult) {

    if (!OtherComp) {
        return;
    }

    UCustomGravityComponent* GravComponent = nullptr;

    TArray<USceneComponent*> Components = OtherComp->GetAttachChildren();
    for (size_t i = 0; i < Components.Num(); i++)
    {
        //UE_LOG(LogTemp, Warning, TEXT("[%f] AGravityActor BeginOverlap Change 2 %s"), GetWorld()->GetRealTimeSeconds(), *Components[i]->GetName());
        if (Cast<UCustomGravityComponent>(Components[i])) {
            //UE_LOG(LogTemp, Warning, TEXT("[%f] AGravityActor BeginOverlap Change 3 %s"), GetWorld()->GetRealTimeSeconds(), *OtherComp->GetName());
            GravComponent = Cast<UCustomGravityComponent>(Components[i]);
            break;
        }
    }


    if (GravComponent) {
        if (bIsAdditive) {
            GravComponent->AddToAdditiveGravityFieldArray(this);
        }
        else {
            GravComponent->AddToGravityFieldArray(this);
        }
    }
}

void UBaseGravityComponent::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
    if (!OtherComp) {
        UE_LOG(LogTemp, Warning, TEXT("[%f] UBaseGravityComponent OnOverlapEnd() OtherComp == nullptr"), GetWorld()->GetRealTimeSeconds());
        return;
    }

    UCustomGravityComponent* GravComponent = nullptr;
 
    TArray<USceneComponent*> Components = OtherComp->GetAttachChildren();
    for (size_t i = 0; i < Components.Num(); i++)
    {
        if (Cast<UCustomGravityComponent>(Components[i])) {
            GravComponent = Cast<UCustomGravityComponent>(Components[i]);
            break;
        }
    }
    if (GravComponent) {
        if (bIsAdditive) {
            GravComponent->RemoveFromAdditiveGravityFieldArray(this);
        }
        else {
            GravComponent->RemoveFromGravityFieldArray(this);
        }
    }
}

FVector UBaseGravityComponent::CalculateGravity(FVector WorldPosition) {
    if (GravityFieldType == EGravityFieldType::EGT_Directional) {
        return DirectionalGravityDirection * GravityStrength;
    }
    return FVector(0.0f, 0.0f, -1.0f) * GravityStrength;
}