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
    /*
    TArray<UActorComponent*> Components = GetComponentsByClass(UPrimitiveComponent::StaticClass());
    for (size_t i = 0; i < Components.Num(); i++)
    {
        if (Cast<UPrimitiveComponent>(Components[i])) {
            Cast<UPrimitiveComponent>(Components[i])->OnComponentBeginOverlap.AddDynamic(this, &AGravityActor::BeginOverlap);
            Cast<UPrimitiveComponent>(Components[i])->OnComponentEndOverlap.AddDynamic(this, &AGravityActor::EndOverlap);
        }
    }

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
                        // @TODO - Doesn't use BeginOverlap(), idk if this matters
                        GravityComp->AddToGravityFieldArray(this);
                        break;
                    }
                }
            }
        }, 0.1f, false);
        */
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
    /*
    //UE_LOG(LogTemp, Warning, TEXT("[%f] AGravityActor BeginOverlap Change 1 %s"), GetWorld()->GetRealTimeSeconds(), *OtherComp->GetName());

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
        if (GravitySettings.bIsAdditive) {
            GravComponent->AddToAdditiveGravityFieldArray(this);
        }
        else {
            GravComponent->AddToGravityFieldArray(this);
        }
    }
    */
}

void AGravityActor::EndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    /*
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
        if (GravitySettings.bIsAdditive) {
            GravComponent->RemoveFromAdditiveGravityFieldArray(this);
        }
        else {
            GravComponent->RemoveFromGravityFieldArray(this);
        }
    }
    */
}
