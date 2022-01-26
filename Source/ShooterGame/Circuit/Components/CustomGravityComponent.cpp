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
	else if (Cast<UPrimitiveComponent>(GetAttachParent())) {
		EffectedComponent = Cast<UPrimitiveComponent>(GetAttachParent());
	}
	else {
		//UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent InitializeComponent null"), GetWorld()->GetRealTimeSeconds());
		return;
	}
	
	if (EffectedComponent) {
		//UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent InitializeComponent %s"), GetWorld()->GetRealTimeSeconds(), *EffectedComponent->GetName());
	}

	if (!EffectedCharacter) {
		ComponentWeight = EffectedComponent->GetMass();
	}
	else {
		ComponentWeight = 1.0f;
	}
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

	if (EffectedComponent == nullptr && EffectedSkeletalComponent == nullptr && EffectedCharacter == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent TickComponent nullptr"), GetWorld()->GetRealTimeSeconds());
		return;
	}

	if (GravityFieldArray.Num() == 0 && AdditiveGravityFieldArray.Num() == 0) {
		return;
	}

	if (GetComponentVelocity().Size() < 0.07f && GetComponentRotation().Equals(LastRotation, 0.05f)) {
		TimeSpentNotMoving += DeltaTime;
	}
	else {
		TimeSpentNotMoving = 0.0f;
	}

	if (TimeSpentNotMoving > 8.0f) {
		EffectedComponent->PutAllRigidBodiesToSleep();
		return;
	}

	LastRotation = GetComponentRotation();

	CalculateCurrentGravity();

	if (bIsSkeletalMesh) {
		TArray<FName> BoneNames;
		EffectedSkeletalComponent->GetBoneNames(BoneNames);

		for (size_t i = 0; i < BoneNames.Num(); i++)
		{
			if (EffectedSkeletalComponent->GetBodyInstance(BoneNames[i])) {
				EffectedSkeletalComponent->GetBodyInstance(BoneNames[i])->AddForce(GetGravityDirection() * GetGravityStrength() * EffectedSkeletalComponent->GetBodyInstance(BoneNames[i])->GetBodyMass());
			}
		}
	}
	else if (EffectedCharacter) {
		//CalculateCharacterGravity();
		return;
	}
	else {
		Cast<UPrimitiveComponent>(EffectedComponent->GetAttachmentRoot())->GetBodyInstance("", true)->AddForceAtPosition(GetGravityDirection() * GetGravityStrength() * ComponentWeight, EffectedComponent->GetComponentLocation(), true, false);
	}
}

void UCustomGravityComponent::AddToGravityFieldArray(UBaseGravityComponent* FieldToAdd) {
	GravityFieldArray.AddUnique(FieldToAdd);

	if (GravityFieldArray.Num() == 1) {
		//UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent AddToGravityFieldArray Change Num %d"), GetWorld()->GetRealTimeSeconds(), GravityFieldArray.Num());
		CurrentGravityDirection = GravityFieldArray[0]->CalculateGravity(this->GetComponentLocation());

		if (bIsSkeletalMesh) {
			EffectedSkeletalComponent->SetEnableGravity(false);
		}
		else if (EffectedCharacter == nullptr) {
			EffectedComponent->SetEnableGravity(false);
		}
	}
}

void UCustomGravityComponent::RemoveFromGravityFieldArray(UBaseGravityComponent* FieldToRemove) {
	int FieldIndex = GravityFieldArray.IndexOfByKey(FieldToRemove);

	GravityFieldArray.Remove(FieldToRemove);

	if (FieldIndex != INDEX_NONE) {
		if (FieldIndex == 0) {
			// Update current gravity to next in array
			if (GravityFieldArray.Num() > 0) {
				//UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent RemoveFromGravityFieldArray Change Num %d"), GetWorld()->GetRealTimeSeconds(), GravityFieldArray.Num());
				CurrentGravityDirection = GravityFieldArray[0]->CalculateGravity(this->GetComponentLocation());
			}
			else {
				CurrentGravityDirection = FVector(0.0f, 0.0f, -1.0f);

				if (bIsSkeletalMesh) {
					EffectedSkeletalComponent->SetEnableGravity(true);
				}
				else if (EffectedCharacter == nullptr) {
					EffectedComponent->SetEnableGravity(true);
				}
			}
		}
	}
}

void UCustomGravityComponent::AddToAdditiveGravityFieldArray(UBaseGravityComponent* FieldToAdd) {
	AdditiveGravityFieldArray.AddUnique(FieldToAdd);

	if (bIsSkeletalMesh) {
		EffectedSkeletalComponent->SetEnableGravity(false);
	}
	else if (EffectedCharacter == nullptr) {
		EffectedComponent->SetEnableGravity(false);
	}
}

void UCustomGravityComponent::RemoveFromAdditiveGravityFieldArray(UBaseGravityComponent* FieldToRemove) {
	int AdditiveFieldIndex = AdditiveGravityFieldArray.IndexOfByKey(FieldToRemove);
	int FieldIndex = GravityFieldArray.IndexOfByKey(FieldToRemove);

	AdditiveGravityFieldArray.Remove(FieldToRemove);

	if (FieldIndex != INDEX_NONE && AdditiveFieldIndex != INDEX_NONE) {
		if (FieldIndex == 0 && AdditiveFieldIndex == 0) {
			// Update current gravity to next in array
			if (GravityFieldArray.Num() > 0) {
				CurrentGravityDirection = GravityFieldArray[0]->CalculateGravity(this->GetComponentLocation());
			}
			else {
				CurrentGravityDirection = FVector(0.0f, 0.0f, -1.0f);

				if (bIsSkeletalMesh) {
					EffectedSkeletalComponent->SetEnableGravity(true);
				}
				else if (EffectedCharacter == nullptr) {
					EffectedComponent->SetEnableGravity(true);
				}
			}
		}
	}
}

void UCustomGravityComponent::CalculateCurrentGravity() {
	if (GravityFieldArray.Num() == 0 && AdditiveGravityFieldArray.Num() == 0) {
		return;
	}

	if (GravityFieldArray.Num() != 0) {
		CurrentGravityDirection = GravityFieldArray[0]->CalculateGravity(this->GetComponentLocation());
		CurrentGravityStrength = CurrentGravityDirection.Size();
	}
	else if (AdditiveGravityFieldArray.Num() != 0) {
		FVector CalculatedGravity = FVector(0.0f, 0.0f, 0.0f);
		for (size_t i = 0; i < AdditiveGravityFieldArray.Num(); i++)
		{
			CalculatedGravity += AdditiveGravityFieldArray[i]->CalculateGravity(this->GetComponentLocation());
		}
		CurrentGravityDirection = CalculatedGravity;
		CurrentGravityStrength = CalculatedGravity.Size();
	}
}

FVector UCustomGravityComponent::GetGravityDirection() {
	return CurrentGravityDirection.GetSafeNormal();
}

float UCustomGravityComponent::GetGravityStrength() {
	return CurrentGravityStrength;
}