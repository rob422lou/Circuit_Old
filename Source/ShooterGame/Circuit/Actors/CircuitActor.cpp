// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/ConstraintWeldComponent.h"
#include "Circuit/Online/CircuitGameState.h"
#include "Circuit/Actors/CircuitActor.h"

// Sets default values
ACircuitActor::ACircuitActor()
{
	MaxLinearVelocity = 10000.0f;

	CurrentMovementTime = 0.0f;
	InitialMovementTime = 0.0f;
	LastUpdateTime = 0.0f;

	bAlwaysRelevant = true;
	bReplicates = true;
	//bReplicateMovement = false;
	bOnlyRelevantToOwner = false;
	bNetLoadOnClient = false; //If part of the map, will load location info from server.
	SetReplicateMovement(true); // This is off otherwise jitter will happen
	
	if (GetRootComponent()) {
		GetRootComponent()->SetIsReplicated(true);
	}

	NetCullDistanceSquared = 22500000000; // @todo - make this scale with object scale. You dont need to net small objects you cant even see.

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

}

// Called when the game starts or when spawned
void ACircuitActor::BeginPlay()
{
	Super::BeginPlay();
	
	if (!GetRootComponent()) {
		return;
	}

	bUsesCustomNetworking = Cast<ACircuitGameState>(GetWorld()->GetGameState())->bUsesCustomNetworking;

	if (bReplicates && bUsesCustomNetworking) {
		// Load these values in from the game state
		const ACircuitGameState* DefGame = Cast<ACircuitGameState>(GetWorld()->GetGameState());
		if (DefGame) {
			ClientBufferTime = DefGame->ClientBufferTime;
			ServerSnapshotTime = DefGame->ServerSnapshotTime;
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("[%f] ACircuitGameState BeginPlay() DefGame null"), GetWorld()->GetRealTimeSeconds());
		}

		if (GetNetMode() == ENetMode::NM_Client) {
			// @todo - Add exceptions for components that are purely aesthetic but still should simulate physically. 
			// OLD - Check 	uint8 bReplicates:1; for actorcomponents. Also turn off physics simulation for anything that can simulate.
			//TArray<UActorComponent*> Components2;
			//GetComponents<UActorComponent>(Components2);

			// Turn off physics simulation for all primitive components (mainly static meshes and skeletal meshes)
			TArray<UPrimitiveComponent*> Components;
			GetComponents<UPrimitiveComponent>(Components);
			if (Components.Num() > 0) {
				UPrimitiveComponent* PrimitiveComponent = Components[0];
				if (PrimitiveComponent != nullptr) {
					PrimitiveComponent->SetSimulatePhysics(false);
					//PrimitiveComponent->BodyInstance.bSimulatePhysics = false;
				}
			}

			// Create 3 (average buffer amount) spots in TArray to prevent reallocation due to sporatic add/removes.
			// REMOVED - IF we are using TInlineAllocator<16> i dont think it's needed.
			//MovementBuffer.Reserve(3);
		}
		//Is a server, needs to run physics AND send interpolation updates periodically.
		else if (GetNetMode() == ENetMode::NM_ListenServer || GetNetMode() == ENetMode::NM_DedicatedServer) {
			//LastUpdateTime = GetWorld()->GetRealTimeSeconds();
		}
	}
}

// Called every frame
void ACircuitActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GetRootComponent() || !GetRootComponent()->IsRegistered()) {
		return;
	}

	DebugDrawWelds();

	// Standalone doesn't update position using interpolation. And no need for interpolation if we aren't replicating.
	// We still want to limit speed though
	if (GetNetMode() == ENetMode::NM_Standalone || !bReplicates || !bUsesCustomNetworking) {
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent());
		FBodyInstance* BI = PrimitiveComponent ? PrimitiveComponent->GetBodyInstance() : nullptr;
		if (BI && BI->bSimulatePhysics)
		{
			BI->SetLinearVelocity(GetRootComponent()->GetComponentVelocity().GetClampedToSize(-MaxLinearVelocity, MaxLinearVelocity), false);
		}

		return;
	} 
	else if (GetNetMode() == ENetMode::NM_ListenServer || GetNetMode() == ENetMode::NM_DedicatedServer) {
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetRootComponent());
		FBodyInstance* BI = PrimitiveComponent ? PrimitiveComponent->GetBodyInstance() : nullptr;
		if (BI && BI->bSimulatePhysics)
		{
			BI->SetLinearVelocity(GetRootComponent()->GetComponentVelocity().GetClampedToSize(-MaxLinearVelocity, MaxLinearVelocity), false);
		}

		if ((LastUpdateTime + GetNetworkSendInterval()) < GetWorld()->GetRealTimeSeconds()) {
			ReplicateInterpolationMovement();
			LastUpdateTime = GetWorld()->GetRealTimeSeconds();
		}

		return;
	}

	// *******CLIENTS ONLY BEYOND THIS POINT*******\
	
	// No need to do anything as client
	if (MovementBuffer.Num() == 0 || InitialMovementTime <= 0.0f) {
		return;
	}

	CurrentMovementTime += DeltaTime;

	// @todo - probably buggy
// If our buffer is too big, speed up CurrentMovementTime slightly
	if (GetCurrentBufferedTime() > (GetClientBufferTime() + GetNetworkSendInterval() + 0.01f)) {
		const float MaxTimeChange = GetNetworkSendInterval() * 0.15;
		float CurrentDelta = MovementBuffer[MovementBuffer.Num() - 1].TimeStamp - (CurrentMovementTime - GetClientBufferTime());

		float TimeChange = FMath::Clamp((GetCurrentBufferedTime() - GetClientBufferTime()) / (GetClientBufferTime() * 25.0f), -1.0f * MaxTimeChange, MaxTimeChange);
		CurrentMovementTime += TimeChange;

		//UE_LOG(LogTemp, Warning, TEXT("[%f] PerdixActor - Tick() MovementBuffer.Num():	%i TimeChange:	%f	GetCurrentBufferedTime():	%f"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num(), TimeChange, GetCurrentBufferedTime());
	}

	// Still buffering
	if (CurrentMovementTime <= (InitialMovementTime + GetClientBufferTime())) {
		return;
	}

	// Convert to world location if needed
	if (MovementBuffer[0].bIsRelativeLocation) {
		MovementBuffer[0].bIsRelativeLocation = false;
		MovementBuffer[0].Location = GetActorLocation() + MovementBuffer[0].Location; // @todo - This value may not be accurate if in the middle of interpolating.
	}

	if (MovementBuffer.Num() > 1 && MovementBuffer[1].bIsRelativeLocation) {
		MovementBuffer[1].bIsRelativeLocation = false;
		MovementBuffer[1].Location = MovementBuffer[0].Location + MovementBuffer[1].Location;
	}

	float LerpPercent = 0.0f;
	const float LerpLimit = 1.15f;
	float ServerDelta = (MovementBuffer.Num() > 1) ? (MovementBuffer[1].TimeStamp - MovementBuffer[0].TimeStamp) : 0.0f;
	//UE_LOG(LogTemp, Error, TEXT("[%f] [CLIENT] CircuitActor - Tick() ServerDelta: %d"), GetWorld()->GetRealTimeSeconds(), ServerDelta);
	// Has at least 1 move to make
	if (ServerDelta > SMALL_NUMBER)
	{
		// Calculate lerp percent
		float RemainingTime = MovementBuffer[1].TimeStamp - (CurrentMovementTime - GetClientBufferTime()); // We need to subtract ClientBufferTime otherwise CurrentMovementTime will be in real time, not in past buffered time.
		float CurrentSmoothTime = ServerDelta - RemainingTime;
		LerpPercent = FMath::Clamp(CurrentSmoothTime / ServerDelta, 0.0f, LerpLimit);
	}
	else {

		if (ServerDelta < 0.0f) {
			UE_LOG(LogTemp, Error, TEXT("[%f] CircuitActor - Tick() ServerDelta too small: %f"), GetWorld()->GetRealTimeSeconds(), ServerDelta);
		}
		// No more moves to make
		LerpPercent = 1.0f;

		//UE_LOG(LogTemp, Error, TEXT("[%f] [CLIENT] CircuitActor - Tick() LerpPercent: %d"), GetWorld()->GetRealTimeSeconds(), LerpPercent);
	}

	// LerpPercent is too high, need to move buffer forward and recalculate lerppercent based on new movement point
	if (LerpPercent >= (1.0f - KINDA_SMALL_NUMBER)) {
		//UE_LOG(LogTemp, Error, TEXT("[%f] [CLIENT] CircuitActor - Tick() MovementBuffer.RemoveAt(0): %d"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num());
		MovementBuffer.RemoveAt(0);
		if (MovementBuffer.Num() > 1) {
			// Convert to world location if needed
			if (MovementBuffer[0].bIsRelativeLocation) {
				MovementBuffer[0].bIsRelativeLocation = false;
				MovementBuffer[0].Location = GetActorLocation() + MovementBuffer[0].Location;
			}

			if (MovementBuffer[1].bIsRelativeLocation) {
				MovementBuffer[1].bIsRelativeLocation = false;
				MovementBuffer[1].Location = MovementBuffer[0].Location + MovementBuffer[1].Location;
			}

			ServerDelta = MovementBuffer[1].TimeStamp - MovementBuffer[0].TimeStamp;
			if (ServerDelta > SMALL_NUMBER)
			{
				// Calculate lerp percent
				float RemainingTime = MovementBuffer[1].TimeStamp - (CurrentMovementTime - GetClientBufferTime()); // We need to subtract ClientBufferTime otherwise CurrentMovementTime will be in real time, not in past buffered time.
				float CurrentSmoothTime = ServerDelta - RemainingTime;
				LerpPercent = FMath::Clamp(CurrentSmoothTime / ServerDelta, 0.0f, 1.15f);
			}
			else {
				// No more moves to make
				LerpPercent = 1.0f;
			}

			// @todo - replace with while loop (?)
			if (LerpPercent > 1.0f) {
				UE_LOG(LogTemp, Error, TEXT("[%f] CircuitActor - Tick() Second LerpPercent calculation is too large: %f"), GetWorld()->GetRealTimeSeconds(), LerpPercent);
			}

			SetActorLocationAndRotation(FMath::LerpStable(MovementBuffer[0].Location, MovementBuffer[1].Location, LerpPercent),
				FQuat::FastLerp(MovementBuffer[0].Quat, MovementBuffer[1].Quat, LerpPercent).GetNormalized(),
				false, nullptr, ETeleportType::None);
		}
		else {
			if (MovementBuffer.Num() > 0) {
				//UE_LOG(LogTemp, Error, TEXT("[%f] [CLIENT] CircuitActor - Tick() MovementBuffer.Num() : %d"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num());

				// Convert to world location if needed
				if (MovementBuffer[0].bIsRelativeLocation) {
					MovementBuffer[0].bIsRelativeLocation = false;
					MovementBuffer[0].Location = GetActorLocation() + MovementBuffer[0].Location;
				}

				InitialMovementTime = 0.0f;
				SetActorLocationAndRotation(MovementBuffer[0].Location, MovementBuffer[0].Quat, false, nullptr, ETeleportType::None);
				MovementBuffer.RemoveAt(0);
			}
		}
	}
	else {
		SetActorLocationAndRotation(FMath::LerpStable(MovementBuffer[0].Location, MovementBuffer[1].Location, LerpPercent),
			FQuat::FastLerp(MovementBuffer[0].Quat, MovementBuffer[1].Quat, LerpPercent).GetNormalized(),
			false, nullptr, ETeleportType::None);
	}
}

//////////////////////////////////////////////////////////////////////////
// Position/Attachment Replication

void ACircuitActor::OnRep_AttachmentReplication()
{
	if (!GetUsesCustomNetworking()) {
		//Super::OnRep_AttachmentReplication();
	}
}

void ACircuitActor::OnRep_CustomAttachmentReplication()
{
	if (CustomAttachmentReplication.AttachParent)
	{
		if (RootComponent)
		{
			USceneComponent* AttachParentComponent = (CustomAttachmentReplication.AttachComponent ? CustomAttachmentReplication.AttachComponent : CustomAttachmentReplication.AttachParent->GetRootComponent());

			if (AttachParentComponent)
			{
				//UE_LOG(LogTemp, Warning, TEXT("[CLIENT] ACircuitActor - OnRep_CustomAttachmentReplication() AttachParentComponent %f"), CustomAttachmentReplication.LocationOffset.X);
				//MovementBuffer.Empty();
				
				AttachToComponent(AttachParentComponent, FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true), CustomAttachmentReplication.AttachSocket);
				RootComponent->SetRelativeLocation(CustomAttachmentReplication.LocationOffset);
				RootComponent->SetRelativeRotation(CustomAttachmentReplication.RotationOffset);
				RootComponent->SetRelativeScale3D(CustomAttachmentReplication.RelativeScale3D);

				// @TODO - this is meant as an optimization but it's not working
				if (!bUsesCustomNetworking) {
					UE_LOG(LogTemp, Warning, TEXT("[CLIENT] ACircuitActor - OnRep_CustomAttachmentReplication() 1"));
					TArray<UPrimitiveComponent*> Components;
					GetComponents<UPrimitiveComponent>(Components);
					
					if (Components.Num() > 0) {
						UE_LOG(LogTemp, Warning, TEXT("[CLIENT] ACircuitActor - OnRep_CustomAttachmentReplication() 2"));
						if (Components[0] != nullptr) {
							for (size_t i = 0; i < Components.Num(); i++)
							{
								UE_LOG(LogTemp, Warning, TEXT("[CLIENT] ACircuitActor - OnRep_CustomAttachmentReplication() %s"), *Components[i]->GetName());
								Components[i]->SetSimulatePhysics(false);
								if (Components[i]->IsSimulatingPhysics()) {
									UE_LOG(LogTemp, Warning, TEXT("[CLIENT] ACircuitActor - OnRep_CustomAttachmentReplication() HERE"));
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CLIENT] ACircuitActor - OnRep_CustomAttachmentReplication() Detach"));
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);


		// @TODO - this is meant as an optimization but it's not working
		if (!bUsesCustomNetworking) {
			TArray<UPrimitiveComponent*> Components;
			GetComponents<UPrimitiveComponent>(Components);

			if (Components.Num() > 0) {
				if (Components[0] != nullptr) {
					for (size_t i = 0; i < Components.Num(); i++)
					{
						UE_LOG(LogTemp, Warning, TEXT("[CLIENT] ACircuitActor - OnRep_CustomAttachmentReplication() Detach %s"), *Components[i]->GetName());
						Components[i]->SetSimulatePhysics(true);
						if (Components[i]->IsSimulatingPhysics()) {
							UE_LOG(LogTemp, Warning, TEXT("[CLIENT] ACircuitActor - OnRep_CustomAttachmentReplication() Detach HERE"));
						}
					}
				}
			}
		}
		// Handle the case where an object was both detached and moved on the server in the same frame.
		// Calling this extraneously does not hurt but will properly fire events if the movement state changed while attached.
		// This is needed because client side movement is ignored when attached
		//OnRep_ReplicatedMovement(); // @PERDIX - Unneeded?
	}
}

/** Replaced with Client_UpdateReplicatedInterpMovement multicast
// @todo - Take another look at reducing the number of replicated values that are the same as the last replicated values
void ACircuitActor::OnRep_ReplicatedInterpolationMovement()
{
	return;
	if (CustomAttachmentReplication.AttachParent != nullptr) {
		return;
	}

	if ((ReplicatedInterpolationMovement.GetLocation() - GetActorLocation()).Size() < 0.5f
		&& ReplicatedInterpolationMovement.GetQuat().Equals(GetActorQuat(), 0.0001f)) {
		UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] ACircuitActor OnRep_ReplicatedInterpolationMovement() New position same as current position. Actor: %s"), GetWorld()->GetRealTimeSeconds(), *GetName());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] ACircuitActor OnRep_ReplicatedInterpolationMovement() MovementBuffer.Num() %d %f"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num(), ReplicatedInterpolationMovement.TimeStamp);

	// Add initial location first so we have 2 points to interpolate between
	if (MovementBuffer.Num() == 0) {

		InitialMovementTime = ReplicatedInterpolationMovement.TimeStamp - GetNetworkSendInterval();
		CurrentMovementTime = ReplicatedInterpolationMovement.TimeStamp;
		MovementBuffer.Add(
			FActorInterpolationBuffer(
				GetActorLocation(),
				GetActorQuat(),
				ReplicatedInterpolationMovement.TimeStamp - GetNetworkSendInterval(),
				false) // Time calculated is a best guess.
		);
	}

	//UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] ACircuitActor OnRep_ReplicatedInterpolationMovement() %d %d"), GetWorld()->GetRealTimeSeconds(), ReplicatedInterpolationMovement.TimeStamp, MovementBuffer[MovementBuffer.Num() - 1].TimeStamp);
	if (ReplicatedInterpolationMovement.TimeStamp > MovementBuffer[MovementBuffer.Num() - 1].TimeStamp) {
		//UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] ACircuitActor OnRep_ReplicatedInterpolationMovement() HERE 2"), GetWorld()->GetRealTimeSeconds());

		bWasRelative = ReplicatedInterpolationMovement.bIsRelativeLocation;
		MovementBuffer.Add(
			FActorInterpolationBuffer(
				ReplicatedInterpolationMovement.GetLocation(),
				ReplicatedInterpolationMovement.GetQuat(),
				ReplicatedInterpolationMovement.TimeStamp,
				ReplicatedInterpolationMovement.bIsRelativeLocation)
		);
	}

	// @fix - This purges relative locations. Convert to world location.
	if (MovementBuffer.Num() > 32) {
		MovementBuffer.RemoveAt(0, 1);
		while (MovementBuffer.Num() > 0 && MovementBuffer[0].bIsRelativeLocation == true) {
			MovementBuffer.RemoveAt(0, 1);
		}
		UE_LOG(LogTemp, Warning, TEXT("[%f] ACircuitActor OnRep_ReplicatedInterpolationMovement() Purging buffer. > 32 Actor: %s"), GetWorld()->GetRealTimeSeconds(), *GetName());
	}
}
*/

void ACircuitActor::ReplicateInterpolationMovement()
{
	if (CustomAttachmentReplication.AttachParent != nullptr) {
		return;
	}
	//UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] ACircuitActor ReplicateInterpolationMovement() HERE 0"), GetWorld()->GetRealTimeSeconds());

	// If we haven't moved then don't send update.
// @todo - Update this so that values are checked against NetSerialized values.
	if (ReplicatedInterpolationMovement.bIsRelativeLocation) {
		if ((GetActorLocation() - LastWorldLocation).Size() < 0.5f
			&& ReplicatedInterpolationMovement.GetQuat().Equals(GetActorQuat(), 0.0001f)) {
			return;
		}
	}
	else {
		if ((ReplicatedInterpolationMovement.GetLocation() - GetActorLocation()).Size() < 0.5f
			&& ReplicatedInterpolationMovement.GetQuat().Equals(GetActorQuat(), 0.0001f)) {
			return;
		}
	}

	// If this is a duplicate timestamp (check why this would happen), don't update.
	if (ReplicatedInterpolationMovement.TimeStamp == GetWorld()->GetRealTimeSeconds()) {
		UE_LOG(LogTemp, Warning, TEXT("[%f] [SERVER] ACircuitActor ReplicateInterpolationMovement() ReplicatedInterpolationMovement.TimeStamp == GetWorld()->GetRealTimeSeconds()"), GetWorld()->GetRealTimeSeconds());
		return;
	}

	// @TODO - causing micro jitter. Best seen at slow speeds.
	if (false && FMath::Abs(GetActorLocation().X - LastWorldLocation.X) < 127 &&
		FMath::Abs(GetActorLocation().Y - LastWorldLocation.Y) < 127 &&
		FMath::Abs(GetActorLocation().Z - LastWorldLocation.Z) < 127 &&
		(GetWorld()->GetRealTimeSeconds() - LastWorldUpdateTime) < (GetNetworkSendInterval() * 4)) {

		UE_LOG(LogTemp, Warning, TEXT("[%f] [SERVER] ACircuitActor ReplicateInterpolationMovement() 1"), GetWorld()->GetRealTimeSeconds());

		ReplicatedInterpolationMovement = FLinearInterpolation(GetActorLocation() - LastWorldLocation, GetActorQuat(), GetWorld()->GetRealTimeSeconds(), true);

		Multi_UpdateInterpMovement(ReplicatedInterpolationMovement);

		ForceNetUpdate();
		LastWorldLocation = GetActorLocation();
	}
	else {
		LastWorldLocation = GetActorLocation();
		LastWorldUpdateTime = GetWorld()->GetRealTimeSeconds();
		ReplicatedInterpolationMovement = FLinearInterpolation(GetActorLocation(), GetActorQuat(), GetWorld()->GetRealTimeSeconds(), false);
		UE_LOG(LogTemp, Warning, TEXT("[%f] [SERVER] ACircuitActor ReplicateInterpolationMovement() %f"), GetWorld()->GetRealTimeSeconds(), ReplicatedInterpolationMovement.TimeStamp);

		Multi_UpdateInterpMovement(ReplicatedInterpolationMovement);

		ForceNetUpdate();
	}
}

//////////////////////////////////////////////////////////////////////////
// New Replication Code 12/17/2021 (Organize after into the above section this code has been proven)

void ACircuitActor::Client_UpdateReplicatedInterpMovement() {
	if (CustomAttachmentReplication.AttachParent != nullptr) {
		return;
	}

	if ((ReplicatedInterpolationMovement.GetLocation() - GetActorLocation()).Size() < 0.5f
		&& ReplicatedInterpolationMovement.GetQuat().Equals(GetActorQuat(), 0.0001f)) {
		UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] ACircuitActor Client_UpdateReplicatedInterpMovement() New position same as current position. Actor: %s"), GetWorld()->GetRealTimeSeconds(), *GetName());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] ACircuitActor Client_UpdateReplicatedInterpMovement() MovementBuffer.Num() %d %f"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num(), ReplicatedInterpolationMovement.TimeStamp);

	// Add initial location first so we have 2 points to interpolate between
	if (MovementBuffer.Num() == 0) {

		InitialMovementTime = ReplicatedInterpolationMovement.TimeStamp - GetNetworkSendInterval();
		CurrentMovementTime = ReplicatedInterpolationMovement.TimeStamp;
		MovementBuffer.Add(
			FActorInterpolationBuffer(
				GetActorLocation(),
				GetActorQuat(),
				ReplicatedInterpolationMovement.TimeStamp - GetNetworkSendInterval(),
				false) // Time calculated is a best guess.
		);
	}

	//UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] ACircuitActor OnRep_ReplicatedInterpolationMovement() %d %d"), GetWorld()->GetRealTimeSeconds(), ReplicatedInterpolationMovement.TimeStamp, MovementBuffer[MovementBuffer.Num() - 1].TimeStamp);
	if (ReplicatedInterpolationMovement.TimeStamp > MovementBuffer[MovementBuffer.Num() - 1].TimeStamp) {
		//UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] ACircuitActor OnRep_ReplicatedInterpolationMovement() HERE 2"), GetWorld()->GetRealTimeSeconds());

		bWasRelative = ReplicatedInterpolationMovement.bIsRelativeLocation;
		MovementBuffer.Add(
			FActorInterpolationBuffer(
				ReplicatedInterpolationMovement.GetLocation(),
				ReplicatedInterpolationMovement.GetQuat(),
				ReplicatedInterpolationMovement.TimeStamp,
				ReplicatedInterpolationMovement.bIsRelativeLocation)
		);
	}

	// @fix - This purges relative locations. Convert to world location.
	if (MovementBuffer.Num() > 32) {
		MovementBuffer.RemoveAt(0, 1);
		while (MovementBuffer.Num() > 0 && MovementBuffer[0].bIsRelativeLocation == true) {
			MovementBuffer.RemoveAt(0, 1);
		}
		UE_LOG(LogTemp, Warning, TEXT("[%f] ACircuitActor Client_UpdateReplicatedInterpMovement() Purging buffer. > 32 Actor: %s"), GetWorld()->GetRealTimeSeconds(), *GetName());
	}
}

bool ACircuitActor::Multi_UpdateInterpMovement_Validate(FLinearInterpolation newRep) {
	return true;
}

void ACircuitActor::Multi_UpdateInterpMovement_Implementation(FLinearInterpolation newRep) {
	if (GetNetMode() == ENetMode::NM_Client) {
		ReplicatedInterpolationMovement = newRep;
		Client_UpdateReplicatedInterpMovement();
		UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] ACircuitActor Multi_UpdateInterpMovement_Implementation() %f"), GetWorld()->GetRealTimeSeconds(), newRep.TimeStamp);
	}
}

//////////////////////////////////////////////////////////////////////////
// Helpers

void ACircuitActor::K2_DestroyActor()
{
	UConstraintWeldComponent* conWeldComp = FindComponentByClass<UConstraintWeldComponent>();
	
	// Unweld all
	conWeldComp->RemoveAllWeldsFrom(this);

	// Unconstrain all


	Super::K2_DestroyActor();
}

float ACircuitActor::GetClientBufferTime()
{
	return ClientBufferTime;
}

float ACircuitActor::GetCurrentBufferedTime()
{
	if (MovementBuffer.Num() > 1) {
		return MovementBuffer[MovementBuffer.Num() - 1].TimeStamp - MovementBuffer[0].TimeStamp;
	}
	return 0.0f;
}

void ACircuitActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate to every client, no special condition required
	//DOREPLIFETIME(ACircuitActor, ReplicatedInterpolationMovement);
	DOREPLIFETIME(ACircuitActor, CustomAttachmentReplication);
}

int ACircuitActor::GetMovementBufferNum()
{
	return MovementBuffer.Num();
}

float ACircuitActor::GetNetworkSendInterval()
{
	return ServerSnapshotTime;
}

bool ACircuitActor::GetUsesCustomNetworking()
{
	return bUsesCustomNetworking;
}

ACircuitActor* ACircuitActor::GetTopAttachedParent(ACircuitActor* Child)
{
	if (!Child)
	{
		return NULL;
	}

	ACircuitActor* TopParent = Child;

	while (TopParent && TopParent->GetAttachParentActor())
	{
		TopParent = Cast<ACircuitActor>(TopParent->GetAttachParentActor());
	}

	return TopParent;
}

//////////////////////////////////////////////////////////////////////////
// Debug

void ACircuitActor::DebugDrawWelds() {
	return;
	ACircuitActor* top = GetTopAttachedParent(this);
	if (top == nullptr) {
		return;
	}
	DrawDebugBox(GetWorld(), top->GetActorLocation(), FVector(55.0f, 55.0f, 55.0f), FColor::Blue, false, -1.0f, 0, 2.0f);

	DrawDebugDirectionalArrow(GetWorld(), GetActorLocation(), top->GetActorLocation(), 15.0f, FColor::Cyan, false, -1.0f, 0, 1);
}
