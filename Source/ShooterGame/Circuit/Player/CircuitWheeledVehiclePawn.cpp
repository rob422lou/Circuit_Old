// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Player/CircuitWheeledVehiclePawn.h"


ACircuitWheeledVehiclePawn::ACircuitWheeledVehiclePawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UsableComp = ObjectInitializer.CreateDefaultSubobject<UUsableComponent>(this, TEXT("UsableComponent"));
	
	BaseLookUpRate = 45.f;
}

void ACircuitWheeledVehiclePawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UsableComp->OnBeginUse.AddDynamic(this, &ACircuitWheeledVehiclePawn::OnUse);
}

bool ACircuitWheeledVehiclePawn::CanPlayerExitVehicle(ACircuitCharacter* ExitingPlayer) {
	UCapsuleComponent* PlayerCapsule = ExitingPlayer->GetCapsuleComponent();

	if (!ExitingPlayer || !PlayerCapsule) {
		return false;
	}
	float CapsuleRadius, CapsuleHalfHeight;

	PlayerCapsule->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	UCapsuleComponent* TestCapsule = NewObject<UCapsuleComponent>(UCapsuleComponent::StaticClass());
	TestCapsule->SetCapsuleRadius(CapsuleRadius * 1.5f);
	TestCapsule->SetCapsuleHalfHeight(CapsuleHalfHeight);

	FBox VehicleBox = CalculateComponentsBoundingBoxInLocalSpace();
	VehicleBox = VehicleBox.ExpandBy(FVector(CapsuleRadius * 2.5f, CapsuleRadius * 2.5f, CapsuleHalfHeight));
	FVector VehicleCenter, VehicleBoxExtent;
	VehicleBox.GetCenterAndExtents(VehicleCenter, VehicleBoxExtent);
	VehicleCenter = VehicleCenter + GetActorLocation();

	// Check right, left, and front first
	FVector CheckPoint = (GetMesh()->GetRightVector() * CheckPoint.Size()) + VehicleCenter;
	CheckPoint.Z += CapsuleHalfHeight;

	TestCapsule->SetWorldLocation(CheckPoint);
	bool bIsOverlapping = GetWorld()->OverlapBlockingTestByProfile(CheckPoint, GetActorRotation().Quaternion(), PlayerCapsule->GetCollisionProfileName(), TestCapsule->GetCollisionShape(), FCollisionQueryParams::DefaultQueryParam);
	if (!bIsOverlapping) {
		TestCapsule->DestroyComponent();
		return true;
	}

	CheckPoint = (GetMesh()->GetRightVector() * -CheckPoint.Size()) + VehicleCenter;
	CheckPoint.Z += CapsuleHalfHeight;

	TestCapsule->SetWorldLocation(CheckPoint);
	bIsOverlapping = GetWorld()->OverlapBlockingTestByProfile(CheckPoint, GetActorRotation().Quaternion(), PlayerCapsule->GetCollisionProfileName(), TestCapsule->GetCollisionShape(), FCollisionQueryParams::DefaultQueryParam);
	if (!bIsOverlapping) {
		TestCapsule->DestroyComponent();
		return true;
	}

	CheckPoint = (GetMesh()->GetForwardVector() * VehicleBoxExtent.X) + VehicleCenter;
	CheckPoint.Z += CapsuleHalfHeight;

	TestCapsule->SetWorldLocation(CheckPoint);
	bIsOverlapping = GetWorld()->OverlapBlockingTestByProfile(CheckPoint, GetActorRotation().Quaternion(), PlayerCapsule->GetCollisionProfileName(), TestCapsule->GetCollisionShape(), FCollisionQueryParams::DefaultQueryParam);
	if (!bIsOverlapping) {
		TestCapsule->DestroyComponent();
		return true;
	}

	DrawDebugCapsule(GetWorld(), CheckPoint, CapsuleHalfHeight, CapsuleRadius, GetActorRotation().Quaternion(), FColor::Cyan, false, 10.0f);
	DrawDebugBox(GetWorld(), VehicleCenter, VehicleBox.GetExtent(), GetActorRotation().Quaternion(), FColor::Emerald, true, 10.0f);

	TestCapsule->DestroyComponent();
	return false;
}

void ACircuitWheeledVehiclePawn::PlayerExitVehicle(ACircuitCharacter* ExitingPlayer) {
	UCapsuleComponent* PlayerCapsule = ExitingPlayer->GetCapsuleComponent();

	if (!ExitingPlayer || !PlayerCapsule) {
		return;
	}

	float CapsuleRadius, CapsuleHalfHeight;

	PlayerCapsule->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	FBox VehicleBox = CalculateComponentsBoundingBoxInLocalSpace();
	VehicleBox = VehicleBox.ExpandBy(FVector(CapsuleRadius * 3.0f, CapsuleRadius * 3.0f, CapsuleHalfHeight));

	FVector VehicleCenter, VehicleBoxExtent;
	VehicleBox.GetCenterAndExtents(VehicleCenter, VehicleBoxExtent);
	VehicleCenter = VehicleCenter + GetActorLocation();

	UCapsuleComponent* TestCapsule = NewObject<UCapsuleComponent>(UCapsuleComponent::StaticClass());
	TestCapsule->SetCapsuleRadius(CapsuleRadius * 1.5f);
	TestCapsule->SetCapsuleHalfHeight(CapsuleHalfHeight);

	// Check right, left, and front first
	FVector CheckPoint = (GetMesh()->GetRightVector() * VehicleBoxExtent.Y) + VehicleCenter;
	CheckPoint.Z += CapsuleHalfHeight;

	TestCapsule->SetWorldLocation(CheckPoint);
	bool bIsOverlapping = GetWorld()->OverlapBlockingTestByProfile(CheckPoint, GetActorRotation().Quaternion(), PlayerCapsule->GetCollisionProfileName(), TestCapsule->GetCollisionShape(), FCollisionQueryParams::DefaultQueryParam);
	if (!bIsOverlapping) {
		ExitingPlayer->SetActorLocation(CheckPoint);
		TestCapsule->DestroyComponent();
		return;
	}

	DrawDebugCapsule(GetWorld(), CheckPoint, CapsuleHalfHeight, CapsuleRadius, GetActorRotation().Quaternion(), FColor::Cyan, false, 10.0f);
	DrawDebugBox(GetWorld(), VehicleCenter, VehicleBox.GetExtent(), GetActorRotation().Quaternion(), FColor::Green, true, 10.0f);

	CheckPoint = (GetMesh()->GetRightVector() * -VehicleBoxExtent.Y) + VehicleCenter;
	CheckPoint.Z += CapsuleHalfHeight;

	TestCapsule->SetWorldLocation(CheckPoint);
	bIsOverlapping = GetWorld()->OverlapBlockingTestByProfile(CheckPoint, GetActorRotation().Quaternion(), PlayerCapsule->GetCollisionProfileName(), TestCapsule->GetCollisionShape(), FCollisionQueryParams::DefaultQueryParam);
	if (!bIsOverlapping) {
		ExitingPlayer->SetActorLocation(CheckPoint);
		TestCapsule->DestroyComponent();
		return;
	}

	DrawDebugCapsule(GetWorld(), CheckPoint, CapsuleHalfHeight, CapsuleRadius, GetActorRotation().Quaternion(), FColor::Cyan, false, 10.0f);
	DrawDebugBox(GetWorld(), VehicleCenter, VehicleBox.GetExtent(), GetActorRotation().Quaternion(), FColor::Green, true, 10.0f);

	CheckPoint = (GetMesh()->GetForwardVector() * VehicleBoxExtent.X) + VehicleCenter;
	CheckPoint.Z += CapsuleHalfHeight;

	TestCapsule->SetWorldLocation(CheckPoint);
	bIsOverlapping = GetWorld()->OverlapBlockingTestByProfile(CheckPoint, GetActorRotation().Quaternion(), PlayerCapsule->GetCollisionProfileName(), TestCapsule->GetCollisionShape(), FCollisionQueryParams::DefaultQueryParam);
	if (!bIsOverlapping) {
		ExitingPlayer->SetActorLocation(CheckPoint);
		TestCapsule->DestroyComponent();
		return;
	}

	DrawDebugCapsule(GetWorld(), CheckPoint, CapsuleHalfHeight, CapsuleRadius, GetActorRotation().Quaternion(), FColor::Cyan, false, 10.0f);
	DrawDebugBox(GetWorld(), VehicleCenter, VehicleBox.GetExtent(), GetActorRotation().Quaternion(), FColor::Green, true, 10.0f);
	TestCapsule->DestroyComponent();
}

//////////////////////////////////////////////////////////////////////////
// Input

void ACircuitWheeledVehiclePawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Use", IE_Pressed, this, &ACircuitWheeledVehiclePawn::OnUsePress);
	//PlayerInputComponent->BindAction("Use", IE_Released, this, &ACircuitCharacter::OnStopUse);

	PlayerInputComponent->BindAxis("LookUp", this, &ACircuitWheeledVehiclePawn::AddControllerPitchInput);
}

// @TODO - I don't like that we use this instead of calling OnUse directly, but I can't get BindAction to work with it
void ACircuitWheeledVehiclePawn::OnUsePress() {
	OnUse(DrivingPawn);
}

void ACircuitWheeledVehiclePawn::OnUse_Implementation(ACircuitCharacter* InstigatingPlayer)
{
	if (GetLocalRole() < ROLE_Authority)
	{
		DrivingPawn = InstigatingPlayer;
		//DrivingPawn->GetMesh()->SetVisibility(false);
		if (IsLocallyControlled()) {
			ServerOnUse(InstigatingPlayer);
		}
	}
	else {
		if (DrivingPawn == nullptr) {
			DrivingPawn = InstigatingPlayer;
			//DrivingPawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			//DrivingPawn->GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			//DrivingPawn->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_None);
			//DrivingPawn->GetRootComponent()->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform, "Seat");

			DrivingPawn->Controller->Possess(this);
			//OnStartEnter(InstigatingPlayer);
		}
		else {
			if (CanPlayerExitVehicle(DrivingPawn)) {
				PlayerExitVehicle(DrivingPawn);
			}
			DrivingPawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			DrivingPawn->GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

			DrivingPawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Controller->Possess(DrivingPawn);
			DrivingPawn = nullptr;

			//OnStartExit(InstigatingPlayer);
		}
	}
}

void ACircuitWheeledVehiclePawn::ServerOnUse_Implementation(ACircuitCharacter* InstigatingPlayer)
{
	OnUse(InstigatingPlayer);
}

bool ACircuitWheeledVehiclePawn::ServerOnUse_Validate(ACircuitCharacter* InstigatingPlayer)
{
	return true;
}

void ACircuitWheeledVehiclePawn::OnStartEnter_Implementation(ACircuitCharacter* InstigatingPlayer)
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (!MyPC || !MyPC->IsGameInputAllowed())
		return;

	if (GetLocalRole() < ROLE_Authority)
	{
		ServerOnStartEnter(InstigatingPlayer);
	}
}

void ACircuitWheeledVehiclePawn::ServerOnStartEnter_Implementation(ACircuitCharacter* InstigatingPlayer)
{
	OnStartEnter(InstigatingPlayer);
}

bool ACircuitWheeledVehiclePawn::ServerOnStartEnter_Validate(ACircuitCharacter* InstigatingPlayer)
{
	return true;
}

void ACircuitWheeledVehiclePawn::OnStartExit_Implementation(ACircuitCharacter* InstigatingPlayer)
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (!MyPC || !MyPC->IsGameInputAllowed())
		return;

	if (GetLocalRole() < ROLE_Authority)
	{
		ServerOnStartExit(InstigatingPlayer);
	}
}

void ACircuitWheeledVehiclePawn::ServerOnStartExit_Implementation(ACircuitCharacter* InstigatingPlayer)
{
	OnStartExit(InstigatingPlayer);
}

bool ACircuitWheeledVehiclePawn::ServerOnStartExit_Validate(ACircuitCharacter* InstigatingPlayer)
{
	return true;
}

void ACircuitWheeledVehiclePawn::AddControllerPitchInput(float Val) {
	Super::AddControllerPitchInput(Val);
}