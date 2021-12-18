// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CircuitHelper.generated.h"

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

USTRUCT()
struct FInterpolationBuffer
{
	GENERATED_USTRUCT_BODY()

		UPROPERTY(Transient)
		FVector LinearVelocity;

	UPROPERTY(Transient)
		FVector Location;

	UPROPERTY(Transient)
		FQuat Quat;

	UPROPERTY(Transient)
		float TimeStamp;

	UPROPERTY(Transient)
		UPrimitiveComponent* NewBase;

	UPROPERTY(Transient)
		FName NewBaseBoneName;

	UPROPERTY(Transient)
		bool bHasBase;

	UPROPERTY(Transient)
		bool bBaseRelativePosition;

	UPROPERTY(Transient)
		uint8 ServerMovementMode;

	UPROPERTY(Transient)
		bool bRelativeRotation;

	FInterpolationBuffer() {}

	FInterpolationBuffer(FVector NewLinearVelocity, FVector NewLocation, FQuat NewQuat, float Time) {
		LinearVelocity = NewLinearVelocity;
		Location = NewLocation;
		Quat = NewQuat;
		TimeStamp = Time;
	}

	FInterpolationBuffer(FVector NewLinearVelocity, FVector NewLocation, FQuat NewQuat, float Time, UPrimitiveComponent* Base, FName BaseBoneName, bool bNewHasBase, bool bNewBaseRelativePosition, uint8 NewServerMovementMode) {
		LinearVelocity = NewLinearVelocity;
		Location = NewLocation;
		Quat = NewQuat;
		TimeStamp = Time;
		NewBase = Base;
		NewBaseBoneName = BaseBoneName;
		bHasBase = bNewHasBase;
		bBaseRelativePosition = bNewBaseRelativePosition;
		ServerMovementMode = NewServerMovementMode;
		bRelativeRotation = false;
	}

	FInterpolationBuffer(FVector NewLinearVelocity, FVector NewLocation, FQuat NewQuat, float Time, UPrimitiveComponent* Base, FName BaseBoneName, bool bNewHasBase, bool bNewBaseRelativePosition, uint8 NewServerMovementMode, bool bNewRelativeRotation) {
		LinearVelocity = NewLinearVelocity;
		Location = NewLocation;
		Quat = NewQuat;
		TimeStamp = Time;
		NewBase = Base;
		NewBaseBoneName = BaseBoneName;
		bHasBase = bNewHasBase;
		bBaseRelativePosition = bNewBaseRelativePosition;
		ServerMovementMode = NewServerMovementMode;
		bRelativeRotation = bNewRelativeRotation;
	}
};

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API UCircuitHelper : public UObject
{
	GENERATED_BODY()
	
};
