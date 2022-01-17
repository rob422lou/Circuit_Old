// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/ShooterCharacterMovement.h"
#include "Circuit/CircuitHelper.h"
#include "Circuit/Components/CustomGravityComponent.h"
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

protected:

/////////////////////////////////////////////////////////
// CharacterMovementComponent Overrides

	virtual void AdjustFloorHeight();

	virtual void ApplyAccumulatedForces(float DeltaSeconds);

	virtual FVector CalcAnimRootMotionVelocity(const FVector& RootMotionDeltaMove, float DeltaSeconds, const FVector& CurrentVelocity) const;

	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration);

	virtual bool CheckLedgeDirection(const FVector& OldLocation, const FVector& SideStep, const FVector& GravDir) const;

	virtual void ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FFindFloorResult& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult = nullptr) const;

	virtual FVector ComputeGroundMovementDelta(const FVector& Delta, const FHitResult& RampHit, const bool bHitFromLineTrace) const override;

	virtual bool ComputePerchResult(const float TestRadius, const FHitResult& InHit, const float InMaxFloorDist, FFindFloorResult& OutPerchFloorResult) const;

	virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const;

	virtual bool DoJump(bool bReplayingMoves);

	virtual void FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const;

	virtual bool FloorSweepTest(
		FHitResult& OutHit,
		const FVector& Start,
		const FVector& End,
		ECollisionChannel TraceChannel,
		const struct FCollisionShape& CollisionShape,
		const struct FCollisionQueryParams& Params,
		const struct FCollisionResponseParams& ResponseParam
	) const;

	virtual FVector GetImpartedMovementBaseVelocity() const;

	virtual FVector GetLedgeMove(const FVector& OldLocation, const FVector& Delta, const FVector& GravDir) const;

	virtual void HandleImpact(const FHitResult& Impact, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector);

	virtual bool IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const;

	virtual bool IsWalkable(const FHitResult& Hit) const;

	virtual void JumpOff(AActor* MovementBaseActor);

	virtual void MaintainHorizontalGroundVelocity();

	virtual void MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult);

	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode);

	virtual void PerformMovement(float DeltaSeconds);

	virtual void PhysFalling(float deltaTime, int32 Iterations);

	virtual void PhysFlying(float deltaTime, int32 Iterations);

	virtual void PhysWalking(float deltaTime, int32 Iterations);

	virtual void SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode = 0);

	/** Use new physics after landing. Defaults to swimming if in water, walking otherwise. */
	virtual void SetPostLandedPhysics(const FHitResult& Hit);

	virtual bool ShouldCheckForValidLandingSpot(float DeltaTime, const FVector& Delta, const FHitResult& Hit) const;

	virtual bool ShouldComputePerchResult(const FHitResult& InHit, bool bCheckRadius) const;

	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact);

	virtual void SimulateMovement(float DeltaSeconds);

	virtual void StartFalling(int32 Iterations, float remainingTime, float timeTick, const FVector& Delta, const FVector& subLoc);

	virtual bool StepUp(const FVector& GravDir, const FVector& Delta, const FHitResult& InHit, FStepDownResult* OutStepDownResult);

	virtual void UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation);

	virtual void UpdateBasedMovement(float DeltaSeconds);

/////////////////////////////////////////////////////////
// Circuit Additions

	// When moving debug lines are shown - velocity, acceleration
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowDebugLines = false;

	FQuat GetCapsuleRotation() const;

	FVector GetCapsuleAxisX();
	
	FVector GetCapsuleAxisZ() const;

	FVector GetComponentDesiredAxisZ();

	UFUNCTION(Category = "Pawn|Components|CharacterMovement", BlueprintCallable)
	FVector GetGravityDirection() const;

	FVector GetGravityScaled();

	FVector GetSafeNormalPrecise(const FVector& V);

	bool IsWithinEdgeToleranceCircuit(const FVector& CapsuleLocation, const FVector& CapsuleDown, const FVector& TestImpactPoint, const float CapsuleRadius) const;

	void UpdateComponentRotation();
};
