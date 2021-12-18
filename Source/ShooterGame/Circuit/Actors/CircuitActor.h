// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Circuit/CircuitHelper.h"
#include "CircuitActor.generated.h"

UCLASS()
class SHOOTERGAME_API ACircuitActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACircuitActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	float MaxLinearVelocity;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

//////////////////////////////////////////////////////////////////////////
// Position/Attachment Replication

	TArray<FActorInterpolationBuffer, TInlineAllocator<32>> MovementBuffer;

	bool bUsesCustomNetworking;
	float ClientBufferTime;
	float ServerSnapshotTime;


	// [Server] Send periodic world location updates to clients just in case a relative location packet is dropped.
	float LastWorldUpdateTime;
	FVector LastWorldLocation;
	float CurrentMovementTime;
	float InitialMovementTime;

	bool bWasRelative;

	//[Server] Last time server updated clients of position.
	float LastUpdateTime;

	/* We use this instead of the default AttachmentReplication because
	void AActor::GatherCurrentMovement() doesn't let us move welded
	objects without removing their attachments. And there's no
	in Actor.cpp to override properly to fix it.
	*/
	UPROPERTY(ReplicatedUsing = OnRep_CustomAttachmentReplication)
	struct FRepAttachment CustomAttachmentReplication;

	//[Server + Client]
	UPROPERTY()
	FLinearInterpolation ReplicatedInterpolationMovement;

	/** Called on client when updated AttachmentReplication value is received for this actor. */
	virtual void OnRep_AttachmentReplication();

	// Used for custom networked attachments
	UFUNCTION()
	virtual void OnRep_CustomAttachmentReplication();

	//UFUNCTION()
	//virtual void OnRep_ReplicatedInterpolationMovement();

	// [Server] Update clients of world position of this
	void ReplicateInterpolationMovement();

	UFUNCTION(NetMulticast, Reliable)
	void Multi_UpdateInterpMovement(FLinearInterpolation newRep);
	bool Multi_UpdateInterpMovement_Validate(FLinearInterpolation newRep);
	void Multi_UpdateInterpMovement_Implementation(FLinearInterpolation newRep);


	void Client_UpdateReplicatedInterpMovement();

//////////////////////////////////////////////////////////////////////////
// Helpers

public:
	virtual void K2_DestroyActor();

	float GetClientBufferTime();

	float GetCurrentBufferedTime();
	
	int GetMovementBufferNum();

	float GetNetworkSendInterval();

	bool GetUsesCustomNetworking();

	// Returns the top-most parented actor
	UFUNCTION(BlueprintCallable, Category = "Circuit|Helper")
	ACircuitActor* GetTopAttachedParent(ACircuitActor* Child);

//////////////////////////////////////////////////////////////////////////
// Debug

	void DebugDrawWelds();
};
