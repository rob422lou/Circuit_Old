// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Public/Weapons/ShooterWeapon.h"
#include "Circuit/Player/CircuitCharacterMovement.h"
#include "Circuit/Player/CircuitCharacter.h"

ACircuitCharacter::ACircuitCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCircuitCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	MaxUseDistance = 2500.0f;
}

void ACircuitCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Controller && Controller->IsLocalController())
	{
		AActor* usable = GetActorInView();

		// End Focus
		if (FocusedUsableActor != usable)
		{
			if (FocusedUsableActor)
			{
				SetOutlineColor(EStencilColor::SC_None, FocusedUsableActor);
			}

			bHasNewFocus = true;
		}

		// Assign new Focus
		FocusedUsableActor = usable;


		// Start Focus.
		if (usable && usable->GetComponentByClass(UUsableComponent::StaticClass()))
		{
			if (bHasNewFocus)
			{
				//UE_LOG(LogTemp, Warning, TEXT("[%f] StartFocusItem"), GetWorld()->GetRealTimeSeconds());
				SetOutlineColor(EStencilColor::SC_LightBlue, FocusedUsableActor);
				bHasNewFocus = false;
			}
		}

	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ACircuitCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	//PlayerInputComponent->BindAction("MouseWheelUp", IE_Pressed, this, &ACircuitCharacter::OnStartMouseWheelUp);
	//PlayerInputComponent->BindAction("MouseWheelDown", IE_Pressed, this, &ACircuitCharacter::OnStartMouseWheelDown);
	//PlayerInputComponent->BindAction("Noclip", IE_Pressed, this, &ACircuitCharacter::Noclip);
	PlayerInputComponent->BindAction("Use", IE_Pressed, this, &ACircuitCharacter::OnStartUse);
	PlayerInputComponent->BindAction("Use", IE_Released, this, &ACircuitCharacter::OnStopUse);
	//PlayerInputComponent->BindAction("QuickMenu", IE_Pressed, this, &ACircuitCharacter::OnStartQuickMenu);
	//PlayerInputComponent->BindAction("QuickMenu", IE_Released, this, &ACircuitCharacter::OnStopQuickMenu);

	const FName name = FName("Reload");
	const FKey oldKey = FKey("R");

	const UInputSettings* DefaultInputSettings = GetDefault<UInputSettings>();
	const FInputActionKeyMapping fInputActionKeyMapping(name, oldKey);

	// This is to remove binding that's set in shootergamecharacter.cpp
	// Obviously it makes more sense to just change it there, but I'm trying to keep
	// all code in Perdix classes to make updating the engine easier.
	for (int i = 0; i < PlayerInputComponent->GetNumActionBindings(); i++) {
		if (PlayerInputComponent->GetActionBinding(i).GetActionName() == "Reload")
		{
			PlayerInputComponent->RemoveActionBinding(i);
			i = PlayerInputComponent->GetNumActionBindings();
		}
	}

	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ACircuitCharacter::OnStartReload);
	PlayerInputComponent->BindAction("Reload", IE_Released, this, &ACircuitCharacter::OnStopReload);
}

void ACircuitCharacter::OnStartReload_Implementation()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (!MyPC || !MyPC->IsGameInputAllowed() || CurrentWeapon == nullptr)
		return;

	AShooterWeapon* CircuitWeapon = Cast<AShooterWeapon>(CurrentWeapon);
	if (CircuitWeapon != nullptr) {
		if (GetLocalRole() < ROLE_Authority)
		{
			ServerOnStartReload();
		}
		CircuitWeapon->OnStartReload();
	}
}

void ACircuitCharacter::ServerOnStartReload_Implementation()
{
	OnStartReload();
}

bool ACircuitCharacter::ServerOnStartReload_Validate()
{
	return true;
}

void ACircuitCharacter::OnStopReload_Implementation()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (!MyPC || !MyPC->IsGameInputAllowed() || CurrentWeapon == nullptr)
		return;

	AShooterWeapon* CircuitWeapon = Cast<AShooterWeapon>(CurrentWeapon);
	if (CircuitWeapon != nullptr) {
		if (GetLocalRole() < ROLE_Authority)
		{
			ServerOnStopReload();
		}
		CircuitWeapon->OnStopReload();
	}
}

void ACircuitCharacter::ServerOnStopReload_Implementation()
{
	OnStopReload();
}

bool ACircuitCharacter::ServerOnStopReload_Validate()
{
	return true;
}

void ACircuitCharacter::OnStartTargeting_Implementation()
{
	
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (!MyPC || !MyPC->IsGameInputAllowed() || CurrentWeapon == nullptr)
		return;
	
	AShooterWeapon* CircuitWeapon = Cast<AShooterWeapon>(CurrentWeapon);
	if (CircuitWeapon != nullptr) {
		if (GetLocalRole() < ROLE_Authority)
		{
			ServerOnStartTargeting();
		}
		CircuitWeapon->OnStartTargeting();
		SetTargeting(false);
	}
	else {
		Super::OnStartTargeting();
	}
}

void ACircuitCharacter::ServerOnStartTargeting_Implementation()
{
	OnStartTargeting();
}

bool ACircuitCharacter::ServerOnStartTargeting_Validate()
{
	return true;
}

void ACircuitCharacter::OnStopTargeting_Implementation()
{
	AShooterPlayerController* MyPC = Cast<AShooterPlayerController>(Controller);
	if (!MyPC || !MyPC->IsGameInputAllowed() || CurrentWeapon == nullptr)
		return;

	AShooterWeapon* CircuitWeapon = Cast<AShooterWeapon>(CurrentWeapon);
	if (CircuitWeapon != nullptr) {
		if (GetLocalRole() < ROLE_Authority)
		{
			ServerOnStopTargeting();
		}

		CircuitWeapon->OnStopTargeting();
		SetTargeting(false);
	}
	else {
		Super::OnStopTargeting();
	}
}

void ACircuitCharacter::ServerOnStopTargeting_Implementation()
{
	OnStopTargeting();
}

bool ACircuitCharacter::ServerOnStopTargeting_Validate()
{
	return true;
}

void ACircuitCharacter::OnStartUse_Implementation()
{
	if (GetLocalRole() < ROLE_Authority) {
		ServerOnStartUse();
	}

	UUsableComponent* Usable = GetUsableComponentInView();
	if (Usable)
	{
		//UE_LOG(LogTemp, Warning, TEXT("[%f] OnStartUse_Implementation"), GetWorld()->GetRealTimeSeconds());
		CurrentlyUsed = Usable;
		Usable->OnBeginUse.Broadcast();
	}

}

void ACircuitCharacter::ServerOnStartUse_Implementation()
{
	OnStartUse();
}

bool ACircuitCharacter::ServerOnStartUse_Validate()
{
	// No special server-side validation performed.
	return true;
}

void ACircuitCharacter::OnStopUse_Implementation()
{
	if (GetLocalRole() < ROLE_Authority) {
		ServerOnStopUse();
	}

	if (CurrentlyUsed)
	{
		CurrentlyUsed->OnEndUse.Broadcast();
		CurrentlyUsed = nullptr;
	}
}

void ACircuitCharacter::ServerOnStopUse_Implementation()
{
	OnStopUse();
}

bool ACircuitCharacter::ServerOnStopUse_Validate()
{
	// No special server-side validation performed.
	return true;
}

/*
Performs raytrace to find closest looked-at UsableActor.
*/
UUsableComponent* ACircuitCharacter::GetUsableComponentInView()
{
	FVector camLoc;
	FRotator camRot;

	if (Controller == NULL)
		return NULL;

	Controller->GetPlayerViewPoint(camLoc, camRot);

	const FVector start_trace = camLoc;
	const FVector direction = camRot.Vector();
	const FVector end_trace = start_trace + (direction * MaxUseDistance);


	FCollisionQueryParams TraceParams(FName(TEXT("UsableTrace")), true, this);
	//TraceParams.bTraceAsyncScene = true;
	TraceParams.bReturnPhysicalMaterial = false;
	TraceParams.bTraceComplex = true;

	FHitResult Hit(ForceInit);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, start_trace, end_trace, ECC_GameTraceChannel2, TraceParams);

	/*DrawDebugLine(
		GetWorld(),
		start_trace,
		end_trace,
		FColor(255, 0, 0),
		false, 1, 0,
		12.333);*/

	if (bHit) {
		return Cast<UUsableComponent>(Hit.GetActor()->GetComponentByClass(UUsableComponent::StaticClass()));
	}

	return nullptr;
}

AActor* ACircuitCharacter::GetActorInView()
{
	FVector camLoc;
	FRotator camRot;

	if (Controller == NULL)
		return NULL;

	Controller->GetPlayerViewPoint(camLoc, camRot);

	const FVector start_trace = camLoc;
	const FVector direction = camRot.Vector();
	const FVector end_trace = start_trace + (direction * MaxUseDistance);

	FCollisionQueryParams TraceParams(FName(TEXT("")), true, this);
	//TraceParams.bTraceAsyncScene = true;
	TraceParams.bReturnPhysicalMaterial = false;
	TraceParams.bTraceComplex = true;

	FHitResult Hit(ForceInit);
	GetWorld()->LineTraceSingleByChannel(Hit, start_trace, end_trace, ECC_GameTraceChannel2, TraceParams);

	/*DrawDebugLine(
		GetWorld(),
		start_trace,
		end_trace,
		FColor(255, 0, 0),
		false, 1, 0,
		12.333);*/

	return Hit.GetActor();
}

//////////////////////////////////////////////////////////////////////////
// Renderer Functions
// This requires "Enabled with stencil" in Project Settings->Rendering and the post process material to work
void ACircuitCharacter::SetOutlineColor(EStencilColor OutlineColor, AActor* highlightActor)
{
	//Cosmetics don't need to be drawn for dedicated servers
	if (GetNetMode() != ENetMode::NM_DedicatedServer) {
		TArray<UActorComponent*> ComponentArray = highlightActor->GetComponentsByClass(UStaticMeshComponent::StaticClass());

		//Turn off outline (may yield renderer processing benefits?)
		if (OutlineColor == EStencilColor::SC_None) {
			for (UActorComponent* Comp : ComponentArray)
			{
				Cast<UStaticMeshComponent>(Comp)->SetRenderCustomDepth(false);
				Cast<UStaticMeshComponent>(Comp)->SetCustomDepthStencilValue((int32)OutlineColor);
			}
		}
		else {
			for (UActorComponent* Comp : ComponentArray)
			{
				Cast<UStaticMeshComponent>(Comp)->SetCustomDepthStencilValue((int32)OutlineColor);
				Cast<UStaticMeshComponent>(Comp)->SetRenderCustomDepth(true);
			}
		}
	}
}

void ACircuitCharacter::OnRep_ReplicatedBasedMovement()
{
	// If this is us, then use this.
	if (Controller) {
		Super::OnRep_ReplicatedBasedMovement();
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("[%f] OnRep_ReplicatedBasedMovement - HERE"), GetWorld()->GetRealTimeSeconds());

	if (GetMovementComponent() && GetWorld()->GetGameState()) {
		//Super::OnRep_ReplicatedBasedMovement();
		if (ReplicatedBasedMovement.HasRelativeLocation()) {
			if (ReplicatedBasedMovement.MovementBase != nullptr) {
				//UE_LOG(LogTemp, Warning, TEXT("[%f] OnRep_ReplicatedBasedMovement - Location: %s Base: %s"), GetWorld()->GetRealTimeSeconds(), *ReplicatedBasedMovement.Location.ToCompactString(), *ReplicatedBasedMovement.MovementBase->GetName());
			}
			else {
				//UE_LOG(LogTemp, Warning, TEXT("[%f] OnRep_ReplicatedBasedMovement - Location: %s Base: None"), GetWorld()->GetRealTimeSeconds(), *ReplicatedBasedMovement.Location.ToCompactString());
			}
			Cast<UCircuitCharacterMovement>(GetMovementComponent())->AddToMovementBuffer(GetReplicatedBasedMovement().Location, GetReplicatedBasedMovement().Rotation.Quaternion(), GetReplicatedMovement().LinearVelocity, GetReplicatedBasedMovement().MovementBase, GetReplicatedBasedMovement().BoneName, (GetReplicatedBasedMovement().MovementBase != nullptr), GetReplicatedBasedMovement().HasRelativeLocation(), GetReplicatedMovementMode(), GetWorld()->GetTimeSeconds(), GetReplicatedBasedMovement().bRelativeRotation);
		}
	}
}

void ACircuitCharacter::OnRep_ReplicatedMovement()
{
	// If this is us, then use this.
	if (Controller) {
		Super::OnRep_ReplicatedMovement();
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("[%f] OnRep_ReplicatedMovement - HERE"), GetWorld()->GetRealTimeSeconds());

	if (GetMovementComponent() && GetWorld()->GetGameState()) {
		if (GetMovementBase()) {
			if (ReplicatedBasedMovement.MovementBase != nullptr) {
				//UE_LOG(LogTemp, Warning, TEXT("[%f] OnRep_ReplicatedMovement - Location: %s ReplicatedMovementBase: %s"), GetWorld()->GetRealTimeSeconds(), *ReplicatedMovement.Location.ToCompactString(), *ReplicatedBasedMovement.MovementBase->GetName());
			}
			else {
				if (GetMovementBase() != nullptr) {
					//UE_LOG(LogTemp, Warning, TEXT("[%f] OnRep_ReplicatedMovement - Location: %s Base: %s"), GetWorld()->GetRealTimeSeconds(), *ReplicatedMovement.Location.ToCompactString(), *GetMovementBase()->GetName());
				}
			}
		}
		else {
		}
		
		if (!GetReplicatedBasedMovement().HasRelativeLocation()) {
			Cast<UCircuitCharacterMovement>(GetMovementComponent())->AddToMovementBuffer(GetReplicatedMovement().Location, GetReplicatedMovement().Rotation.Quaternion(), GetReplicatedMovement().LinearVelocity, NULL, "", false, false, GetReplicatedMovementMode(), GetWorld()->GetTimeSeconds(), false); //FIX ME - TimeStamp is wrong.
		}
	}
}