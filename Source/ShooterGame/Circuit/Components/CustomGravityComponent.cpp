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

	if (GravityFieldArray.Num() == 0) {
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

	FVector GravityForce;

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
			GravityForce = GravitySettings.GravityDirection.GetSafeNormal() * GravitySettings.GravityPower * ComponentWeight;
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
				if (GravitySettings.GravityType == EGravityType::EGT_Point) {
					GravityForce = (GravityFieldArray[0]->GetActorLocation() - EffectedSkeletalComponent->GetBoneLocation(BoneNames[i])).GetSafeNormal() * GravitySettings.GravityPower * ComponentWeight;
				}
				EffectedSkeletalComponent->GetBodyInstance(BoneNames[i])->AddForce(GravityForce * EffectedSkeletalComponent->GetBodyInstance(BoneNames[i])->GetBodyMass());
			}
		}
	}
	else if (EffectedCharacter) {
		switch (GravitySettings.GravityType)
		{
		case EGravityType::EGT_Default:
			// @TODO - Make SetEnableGravity repnotify on skele/vehicle/staticmeshes
			//EffectedComponent->SetEnableGravity(true);
			return;
			break;
		case EGravityType::EGT_Custom:
			//EffectedComponent->SetEnableGravity(false);
			break;
		case EGravityType::EGT_Directional:
			//GravityForce = GravitySettings.GravityDirection.GetSafeNormal() * GravitySettings.GravityPower * ComponentWeight;
			break;
		case EGravityType::EGT_Point:
			//UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent TickComponent HERE"), GetWorld()->GetRealTimeSeconds());
			//GravityForce = (GravityFieldArray[0]->GetActorLocation() - GetComponentLocation()).GetSafeNormal() * GravitySettings.GravityPower * ComponentWeight;
			// This is just for debug purposes
			GravitySettings.GravityDirection = (GravityFieldArray[0]->GetActorLocation() - GetComponentLocation()).GetSafeNormal();
			break;
		case EGravityType::EGT_Planet:
			break;
		default:
			break;
		}
		return;
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
			GravityForce = GravitySettings.GravityDirection.GetSafeNormal() * GravitySettings.GravityPower * ComponentWeight;
			EffectedComponent->SetEnableGravity(false);
			break;
		case EGravityType::EGT_Point:
			GravityForce = (GravityFieldArray[0]->GetActorLocation() - GetComponentLocation()).GetSafeNormal() * GravitySettings.GravityPower * ComponentWeight;
			EffectedComponent->SetEnableGravity(false);

			// This is just for debug purposes
			GravitySettings.GravityDirection = (GravityFieldArray[0]->GetActorLocation() - GetComponentLocation()).GetSafeNormal();
			break;
		case EGravityType::EGT_Planet:
			EffectedComponent->SetEnableGravity(false);
			break;
		default:
			break;
		}

		Cast<UPrimitiveComponent>(EffectedComponent->GetAttachmentRoot())->GetBodyInstance("", true)->AddForceAtPosition(GravityForce, EffectedComponent->GetComponentLocation(), GravitySettings.bForceSubStepping, false);
		//Cast<UPrimitiveComponent>(EffectedComponent->GetAttachmentRoot())->GetBodyInstance("", true)->AddForce(GravityForce, GravitySettings.bForceSubStepping, false);
	}
}

void UCustomGravityComponent::AddToGravityFieldArray(AGravityActor* FieldToAdd) {
	GravityFieldArray.AddUnique(FieldToAdd);

	if (GravityFieldArray.Num() == 1) {
		//UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent AddToGravityFieldArray Change Num %d"), GetWorld()->GetRealTimeSeconds(), GravityFieldArray.Num());
		GravitySettings = GravityFieldArray[0]->GravitySettings;
	}
}

void UCustomGravityComponent::RemoveFromGravityFieldArray(AGravityActor* FieldToRemove) {
	int FieldIndex = GravityFieldArray.IndexOfByKey(FieldToRemove);

	GravityFieldArray.Remove(FieldToRemove);

	if (FieldIndex != INDEX_NONE) {
		if (FieldIndex == 0) {
			// Update current gravity to next in array
			if (GravityFieldArray.Num() > 0) {
				//UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent RemoveFromGravityFieldArray Change Num %d"), GetWorld()->GetRealTimeSeconds(), GravityFieldArray.Num());
				GravitySettings = GravityFieldArray[0]->GravitySettings;
				//GravitySettings.GravityPower
			}
			else {
				GravitySettings.GravityType = EGravityType::EGT_Default;
				GravitySettings.GravityDirection = FVector(0.0f, 0.0f, -1.0f);
			}
		}
	}
}
