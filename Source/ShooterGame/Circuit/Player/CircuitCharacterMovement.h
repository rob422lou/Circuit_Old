// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Circuit/CircuitHelper.h"
#include "Player/ShooterCharacterMovement.h"
#include "CircuitCharacterMovement.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API UCircuitCharacterMovement : public UShooterCharacterMovement
{
	GENERATED_BODY()
	
	/** Default UObject constructor. */
	UCircuitCharacterMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;

	/** Replicate position correction to client, associated with a timestamped servermove.  Client will replay subsequent moves after applying adjustment.  */
	virtual void ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLoc, FVector NewVel, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode) override;

	// Changed to move our character using MovementBuffer data
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

	// Needed to prevent client from changing movement mode outside of interpolation buffer.
	bool DoJump(bool bReplayingMoves);

	// Needed to smooth welded base movement
	virtual void MaybeUpdateBasedMovement(float DeltaSeconds) override;

	//
	virtual void PerformMovement(float DeltaSeconds) override;

	// When transitioning from falling to walking change prevents sliding on moving objects by forcing velocity to equal new base velocity.
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;

	// Need this primarily because we override the PerformMovement() function it uses.
	virtual void ReplicateMoveToServer(float DeltaTime, const FVector& NewAcceleration) override;

	//
	virtual void PostPhysicsTickComponent(float DeltaTime, FCharacterMovementComponentPostPhysicsTickFunction& ThisTickFunction) override;

	// Changed to call our overridden version of ClientUpdatePositionAfterServerUpdate which moves character.
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

//////////////////////////////////////////////////////////////////////////
// Circuit Position/Attachment Replication

protected:
	// [Server+Client] Used in PhysFalling
	bool bWasFalling;

	bool InitialMove;
	float CurrentMovementTime;
	float InitialMovementTime;

	//[Client] Holds list of movements that need to be replayed when NET_Linear is being used.
	TArray<FInterpolationBuffer> MovementBuffer;

private:
	// Populated using CircuitGameState values
	float ClientBufferTime;
	float ServerSnapshotTime;

public:

	void AddToMovementBuffer(FVector NewLocation, FQuat NewQuat, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, float TimeStamp, bool bRelativeRotation);

	void ClientPerformMovement(float DeltaSeconds);

	void MoveClient(FInterpolationBuffer StartPosition, FInterpolationBuffer EndPosition, float LerpPercent);

//////////////////////////////////////////////////////////////////////////
// Helpers

protected:
	bool bUseCustomNetworking;

public:
	float GetClientBufferTime();
	float GetNetworkSendInterval();
	bool GetUsesCustomNetworking();

	bool GetIsClient() { return (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client)); };
};
