// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CircuitActor.generated.h"

USTRUCT()
struct FActorInterpolationBuffer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(Transient)
	FVector Location;

	UPROPERTY(Transient)
	FQuat Quat;

	UPROPERTY(Transient)
	float TimeStamp;

	UPROPERTY(Transient)
	bool bIsRelativeLocation;

	FActorInterpolationBuffer() {}

	FActorInterpolationBuffer(FVector NewLocation, FQuat NewQuat, float Time) {
		bIsRelativeLocation = false;
		Location = NewLocation;
		Quat = NewQuat;
		TimeStamp = Time;
	}

	FActorInterpolationBuffer(FVector NewLocation, FQuat NewQuat, float Time, bool bHasRelativePosition) {
		bIsRelativeLocation = bHasRelativePosition;
		Location = NewLocation;
		Quat = NewQuat;
		TimeStamp = Time;
	}
};

// @todo - add byte to beginning of sent information which will tell if location is relative. Use remaing bits in byte to enhance relative location precision. See FRepMovement.
// see: https://gafferongames.com/post/snapshot_compression/ & https://gist.github.com/StagPoint/bb7edf61c2e97ce54e3e4561627f6582
USTRUCT()
struct FLinearInterpolation
{
	GENERATED_USTRUCT_BODY()

		UPROPERTY(Transient)
		uint8 bIsRelativeLocation : 1;

	UPROPERTY(Transient)
		FRotator Rotation;

	UPROPERTY(Transient)
		FVector_NetQuantize10 Location;

	// WARNING - Just using GetRealTimeSeconds() in PerdixActor instead of this will result in subtle jitter. See rolling spheres.
	UPROPERTY(Transient)
		float TimeStamp;

	FLinearInterpolation() {}

	FLinearInterpolation(FVector NewLocation, FQuat NewQuat, float NewTimeStamp) {
		bIsRelativeLocation = false;
		Location = NewLocation;
		Rotation = NewQuat.Rotator();
		TimeStamp = NewTimeStamp;
		return;

	}

	FLinearInterpolation(FVector NewLocation, FQuat NewQuat, float NewTimeStamp, bool bNewIsRelativeLocation) {
		bIsRelativeLocation = bNewIsRelativeLocation;
		Location = NewLocation;
		Rotation = NewQuat.Rotator();
		TimeStamp = NewTimeStamp;
		return;

	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;

		// @todo - Use spare bits to increase range/precision

		// pack bitfield with flags
		uint8 Flags = (bIsRelativeLocation << 0);
		Ar.SerializeBits(&Flags, 2);
		bIsRelativeLocation = (Flags & (1 << 0)) ? 1 : 0;

		if (bIsRelativeLocation) {
			bOutSuccess &= SerializeFixedVector<127, 9>(Location, Ar);
		}
		else {
			bOutSuccess &= Location.NetSerialize(Ar, Map, bOutSuccess);
		}

		Rotation.SerializeCompressedShort(Ar);
		Ar << TimeStamp;

		return true;
	}

public:
	FORCEINLINE FQuat GetQuat()
	{
		return Rotation.Quaternion();
	}

	FORCEINLINE FVector GetLocation()
	{
		return Location;
	}
};

template<>
struct TStructOpsTypeTraits<FLinearInterpolation> : public TStructOpsTypeTraitsBase2<FLinearInterpolation>
{
	enum
	{
		WithNetSerializer = true,
		WithNetSharedSerialization = true,
	};
};

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
