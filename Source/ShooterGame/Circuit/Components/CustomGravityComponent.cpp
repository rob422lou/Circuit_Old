// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/CustomGravityComponent.h"

// Sets default values for this component's properties
UCustomGravityComponent::UCustomGravityComponent()
{
	// Fire InitializeComponent()
	bWantsInitializeComponent = true;

	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UCustomGravityComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (EffectedComponent != NULL)
	{
		return;
	}

	EffectedComponent = Cast<USkeletalMeshComponent>(GetAttachParent());

	if (EffectedComponent) {
		bIsSkeletalMesh = true;
		EffectedSkeletalComponent = Cast<USkeletalMeshComponent>(GetAttachParent());
	}
	else {
		EffectedComponent = Cast<UPrimitiveComponent>(GetAttachParent());
	}
	
	ComponentWeight = EffectedComponent->GetMass();
}

// Called when the game starts
void UCustomGravityComponent::BeginPlay()
{
	Super::BeginPlay();	
}


// Called every frame
// @TODO - Clean this up by moving things into separate functions
void UCustomGravityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (EffectedComponent == nullptr && EffectedSkeletalComponent == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent TickComponent nullptr"), GetWorld()->GetRealTimeSeconds());
		return;
	}

	FVector GravityForce = GravitySettings.GravityDirection.GetSafeNormal() * GravitySettings.GravityPower * ComponentWeight;

	if (bIsSkeletalMesh) {
		switch (GravitySettings.GravityType)
		{
		case EGravityType::EGT_Default:
			// @TODO - Make SetEnableGravity repnotify on skele/vehicle/staticmeshes
			EffectedSkeletalComponent->SetEnableGravity(true);
			return;
			break;
		case EGravityType::EGT_Custom:
			EffectedSkeletalComponent->SetEnableGravity(false);
			break;
		case EGravityType::EGT_Directional:
			EffectedSkeletalComponent->SetEnableGravity(false);
			break;
		case EGravityType::EGT_Point:
			ComponentWeight = 1.0f;
			// @TODO - Idk why this is needed, but it is. Please find out why
			GravityForce *= 46;
			EffectedSkeletalComponent->SetEnableGravity(false);
			break;
		case EGravityType::EGT_Planet:
			EffectedSkeletalComponent->SetEnableGravity(false);
			break;
		default:
			break;
		}

		TArray<FName> BoneNames;
		EffectedSkeletalComponent->GetBoneNames(BoneNames);

		for (size_t i = 0; i < BoneNames.Num(); i++)
		{
			if (EffectedSkeletalComponent->GetBodyInstance(BoneNames[i])) {
				EffectedSkeletalComponent->GetBodyInstance(BoneNames[i])->AddForce(GravityForce * EffectedSkeletalComponent->GetBodyInstance(BoneNames[i])->GetBodyMass());
			}
		}
	}
	else {
		switch (GravitySettings.GravityType)
		{
		case EGravityType::EGT_Default:
			// @TODO - Make SetEnableGravity repnotify on skele/vehicle/staticmeshes
			EffectedComponent->SetEnableGravity(true);
			return;
			break;
		case EGravityType::EGT_Custom:
			EffectedComponent->SetEnableGravity(false);
			break;
		case EGravityType::EGT_Directional:
			EffectedComponent->SetEnableGravity(false);
			break;
		case EGravityType::EGT_Point:
			EffectedComponent->SetEnableGravity(false);
			break;
		case EGravityType::EGT_Planet:
			EffectedComponent->SetEnableGravity(false);
			break;
		default:
			break;
		}

		Cast<UPrimitiveComponent>(EffectedComponent->GetAttachmentRoot())->GetBodyInstance("", true)->AddForceAtPosition(GravityForce, EffectedComponent->GetComponentLocation(), GravitySettings.bForceSubStepping, false);

	}
}

