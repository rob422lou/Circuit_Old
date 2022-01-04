// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/CustomGravityComponent.h"
#include "Circuit/Actors/GravityActor.h"

// Sets default values
AGravityActor::AGravityActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AGravityActor::BeginPlay()
{
	Super::BeginPlay();

    TArray<UActorComponent*> Components = GetComponentsByClass(UPrimitiveComponent::StaticClass());
    for (size_t i = 0; i < Components.Num(); i++)
    {
        if (Cast<UPrimitiveComponent>(Components[i])) {
            Cast<UPrimitiveComponent>(Components[i])->OnComponentBeginOverlap.AddDynamic(this, &AGravityActor::BeginOverlap);
            Cast<UPrimitiveComponent>(Components[i])->OnComponentEndOverlap.AddDynamic(this, &AGravityActor::EndOverlap);
        }
    }
}

// Called every frame
void AGravityActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AGravityActor::BeginOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherComp) {
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
        UE_LOG(LogTemp, Warning, TEXT("[%f] AGravityActor BeginOverlap: %s"), GetWorld()->GetRealTimeSeconds(), *OtherComp->GetName());
        GravComponent->GravitySettings.GravityDirection = GravityDirection;
    }
}

void AGravityActor::EndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherComp) {
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
        GravComponent->GravitySettings.GravityDirection = FVector(0.0f, 0.0f, 1.0f);
    }
}