// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Player/CircuitCharacter.h"

#include "Engine/NetworkObjectList.h"

#include "Circuit/Player/CircuitCharacterMovement.h"

const float VERTICAL_SLOPE_NORMAL_Z = 0.001f; // Slope is vertical if Abs(Normal.Z) <= this threshold. Accounts for precision problems that sometimes angle normals slightly off horizontal for vertical surface.
const float MAX_STEP_SIDE_Z = 0.08f;	// maximum z value for the normal on the vertical side of steps

UCircuitCharacterMovement::UCircuitCharacterMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

// DONE
void UCircuitCharacterMovement::AdjustFloorHeight()
{
	//SCOPE_CYCLE_COUNTER(STAT_CharAdjustFloorHeight);

	// If we have a floor check that hasn't hit anything, don't adjust height.
	if (!CurrentFloor.IsWalkableFloor())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent !IsWalkableFloor"), GetWorld()->GetRealTimeSeconds());
		return;
	}

	float OldFloorDist = CurrentFloor.FloorDist;
	if (CurrentFloor.bLineTrace)
	{
		if (OldFloorDist < MIN_FLOOR_DIST && CurrentFloor.LineDist >= MIN_FLOOR_DIST)
		{
			// This would cause us to scale unwalkable walls
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("Adjust floor height aborting due to line trace with small floor distance (line: %.2f, sweep: %.2f)"), CurrentFloor.LineDist, CurrentFloor.FloorDist);
			return;
		}
		else
		{
			// Falling back to a line trace means the sweep was unwalkable (or in penetration). Use the line distance for the vertical adjustment.
			OldFloorDist = CurrentFloor.LineDist;
		}
	}

	// Move up or down to maintain floor height.
	if (OldFloorDist < MIN_FLOOR_DIST || OldFloorDist > MAX_FLOOR_DIST)
	{
		FHitResult AdjustHit(1.f);
		//const float InitialZ = UpdatedComponent->GetComponentLocation().Z;
		const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;
		const float MoveDist = AvgFloorDist - OldFloorDist;
		const FVector CapsuleUp = GetCapsuleAxisZ();// @CIRCUIT - addition
		const FVector InitialLocation = UpdatedComponent->GetComponentLocation(); // @CIRCUIT - addition

		SafeMoveUpdatedComponent(CapsuleUp * MoveDist, UpdatedComponent->GetComponentQuat(), true, AdjustHit);
		//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("Adjust floor height %.3f (Hit = %d)"), MoveDist, AdjustHit.bBlockingHit);

		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist;
		}
		else if (MoveDist > 0.f)
		{
			// @CIRCUIT - alteration
			//const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;
			CurrentFloor.FloorDist += (InitialLocation - UpdatedComponent->GetComponentLocation()) | CapsuleUp;
		}
		else
		{
			checkSlow(MoveDist < 0.f);
			//const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;
			CurrentFloor.FloorDist = (AdjustHit.Location - UpdatedComponent->GetComponentLocation()) | CapsuleUp; // @CIRCUIT - altered
			if (IsWalkable(AdjustHit))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
		}

		// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
		// Also avoid it if we moved out of penetration
		bJustTeleported |= !bMaintainHorizontalGroundVelocity || (OldFloorDist < 0.f);

		// If something caused us to adjust our height (especially a depentration) we should ensure another check next frame or we will keep a stale result.
		if (CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
		{
			bForceNextFloorCheck = true;
		}
	}
}

// DONE
void UCircuitCharacterMovement::ApplyAccumulatedForces(float DeltaSeconds)
{
	if ((!PendingImpulseToApply.IsZero() || !PendingForceToApply.IsZero()) && IsMovingOnGround())
	{
		const FVector Impulse = PendingImpulseToApply + PendingForceToApply * DeltaSeconds + GetGravityScaled() * DeltaSeconds;

		// Check to see if applied momentum is enough to overcome gravity.
		if ((Impulse | GetCapsuleAxisZ()) > SMALL_NUMBER)
		{
			SetMovementMode(MOVE_Falling);
		}
	}

	Velocity += PendingImpulseToApply + PendingForceToApply * DeltaSeconds;

	PendingImpulseToApply = FVector::ZeroVector;
	PendingForceToApply = FVector::ZeroVector;
}

// DONE
FVector UCircuitCharacterMovement::CalcAnimRootMotionVelocity(const FVector& RootMotionDeltaMove, float DeltaSeconds, const FVector& CurrentVelocity) const
{
	if (ensure(DeltaSeconds > 0.f))
	{
		FVector RootMotionVelocity = RootMotionDeltaMove / DeltaSeconds;
		
		// @CIRCUIT - addition
		// Do not override vertical velocity if in falling physics, we want to keep the effect of gravity.
		if (IsFalling())
		{
			const FVector GravityDir = GetGravityDirection();
			RootMotionVelocity = FVector::VectorPlaneProject(RootMotionVelocity, GravityDir) + GravityDir * (Velocity | GravityDir);
		}

		return RootMotionVelocity;
	}
	else
	{
		return CurrentVelocity;
	}
}

// DONE
void UCircuitCharacterMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Do not update velocity when using root motion or when SimulatedProxy and not simulating root motion - SimulatedProxy are repped their Velocity
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
	{
		return;
	}

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = GetMaxAcceleration();
	float MaxSpeed = GetMaxSpeed();

	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		bZeroRequestedAcceleration = false;
	}

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
		}

		AnalogInputModifier = 1.f;
	}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	const float MaxInputSpeed = FMath::Max(MaxSpeed * AnalogInputModifier, GetMinAnalogSpeed());
	MaxSpeed = FMath::Max(RequestedSpeed, MaxInputSpeed);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}

	// Apply input acceleration
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}

	// Apply additional requested acceleration
	if (!bZeroRequestedAcceleration)
	{
		const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
	}

	if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}

	if (bShowDebugLines)
	{
		UE_LOG(LogTemp, Log, TEXT("Requested acceleration %s    Acceleration %s    Velocity %s"), *RequestedAcceleration.ToString(), *Acceleration.ToString(), *Velocity.ToString());
	}
}

// DONE
bool UCircuitCharacterMovement::CheckLedgeDirection(const FVector& OldLocation, const FVector& SideStep, const FVector& GravDir) const
{
	const FVector SideDest = OldLocation + SideStep;
	const FQuat CapsuleRotation = GetCapsuleRotation(); // @CIRCUIT - addition
	FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CheckLedgeDirection), false, CharacterOwner);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(CapsuleParams, ResponseParam);
	const FCollisionShape CapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_None);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
	FHitResult Result(1.f);
	GetWorld()->SweepSingleByChannel(Result, OldLocation, SideDest, CapsuleRotation, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParam);

	if (!Result.bBlockingHit || IsWalkable(Result))
	{
		if (!Result.bBlockingHit)
		{
			GetWorld()->SweepSingleByChannel(Result, SideDest, SideDest + GravDir * (MaxStepHeight + LedgeCheckThreshold), CapsuleRotation, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParam);
		}
		if ((Result.Time < 1.f) && IsWalkable(Result))
		{
			return true;
		}
	}
	return false;
}

// DONE
void UCircuitCharacterMovement::ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FFindFloorResult& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult) const
{
	//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("[Role:%d] ComputeFloorDist: %s at location %s"), (int32)CharacterOwner->GetLocalRole(), *GetNameSafe(CharacterOwner), *CapsuleLocation.ToString());
	OutFloorResult.Clear();

	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	const FVector CapsuleDown = GetCapsuleAxisZ() * -1.0f;

	bool bSkipSweep = false;
	if (DownwardSweepResult != NULL && DownwardSweepResult->IsValidBlockingHit())
	{
		const float Dot = CapsuleDown | ((DownwardSweepResult->TraceEnd - DownwardSweepResult->TraceStart).GetSafeNormal());

		// Only if the supplied sweep was vertical and downward.
		// @CIRCUIT - Alteration
		if (Dot >= THRESH_NORMALS_ARE_PARALLEL)
		{
			// Reject hits that are barely on the cusp of the radius of the capsule @CIRCUIT - Alteration
			if (IsWithinEdgeToleranceCircuit(DownwardSweepResult->Location, CapsuleDown, DownwardSweepResult->ImpactPoint, PawnRadius))
			{
				// Don't try a redundant sweep, regardless of whether this sweep is usable.
				bSkipSweep = true;

				const bool bIsWalkable = IsWalkable(*DownwardSweepResult);
				const float FloorDist = (CapsuleLocation - DownwardSweepResult->Location).Size(); // @CIRCUIT - changed
				OutFloorResult.SetFromSweep(*DownwardSweepResult, FloorDist, bIsWalkable);

				if (bIsWalkable)
				{
					// Use the supplied downward sweep as the floor hit result.			
					return;
				}
			}
		}
	}

	// We require the sweep distance to be >= the line distance, otherwise the HitResult can't be interpreted as the sweep result.
	if (SweepDistance < LineDistance)
	{
		ensure(SweepDistance >= LineDistance);
		return;
	}

	bool bBlockingHit = false;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComputeFloorDist), false, CharacterOwner);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

	// Sweep test
	if (!bSkipSweep && SweepDistance > 0.f && SweepRadius > 0.f)
	{
		// Use a shorter height to avoid sweeps giving weird results if we start on a surface.
		// This also allows us to adjust out of penetrations.
		const float ShrinkScale = 0.9f;
		const float ShrinkScaleOverlap = 0.1f;
		float ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScale);
		float TraceDist = SweepDistance + ShrinkHeight;
		FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(SweepRadius, PawnHalfHeight - ShrinkHeight);

		FHitResult Hit(1.f);
		// @CIRCUIT - altered
		bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + CapsuleDown * TraceDist, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
			// Check 2D distance to impact point, reject if within a tolerance from radius. @CIRCUIT - alteration
			if (Hit.bStartPenetrating || !IsWithinEdgeToleranceCircuit(CapsuleLocation, CapsuleDown, Hit.ImpactPoint, CapsuleShape.Capsule.Radius))
			{
				// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
				// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
				CapsuleShape.Capsule.Radius = FMath::Max(0.f, CapsuleShape.Capsule.Radius - SWEEP_EDGE_REJECT_DISTANCE - KINDA_SMALL_NUMBER);
				if (!CapsuleShape.IsNearlyZero())
				{
					ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
					TraceDist = SweepDistance + ShrinkHeight;
					CapsuleShape.Capsule.HalfHeight = FMath::Max(PawnHalfHeight - ShrinkHeight, CapsuleShape.Capsule.Radius);
					Hit.Reset(1.f, false);

					// @CIRCUIT - Alteration
					bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + CapsuleDown * TraceDist, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
				}
			}

			// Reduce hit distance by ShrinkHeight because we shrank the capsule for the trace.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloorResult.SetFromSweep(Hit, SweepResult, false);
			if (Hit.IsValidBlockingHit() && IsWalkable(Hit))
			{
				if (SweepResult <= SweepDistance)
				{
					// Hit within test distance.
					OutFloorResult.bWalkableFloor = true;
					return;
				}
			}
		}
	}

	// Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything.
	// We do however want to try a line trace if the sweep was stuck in penetration.
	if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
	{
		OutFloorResult.FloorDist = SweepDistance;
		return;
	}

	// Line trace
	if (LineDistance > 0.f)
	{
		const float ShrinkHeight = PawnHalfHeight;
		const FVector LineTraceStart = CapsuleLocation;
		const float TraceDist = LineDistance + ShrinkHeight;
		//const FVector Down = FVector(0.f, 0.f, -TraceDist); // @CIRCUIT - Removal
		QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

		FHitResult Hit(1.f);
		// @CIRCUIT - alteration
		bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + CapsuleDown * TraceDist, CollisionChannel, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			if (Hit.Time > 0.f)
			{
				// Reduce hit distance by ShrinkHeight because we started the trace higher than the base.
				// We allow negative distances here, because this allows us to pull out of penetrations.
				const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
				const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

				OutFloorResult.bBlockingHit = true;
				if (LineResult <= LineDistance && IsWalkable(Hit))
				{
					OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
					return;
				}
			}
		}
	}

	// No hits were acceptable.
	OutFloorResult.bWalkableFloor = false;
	OutFloorResult.FloorDist = SweepDistance; // @CIRCUIT - Addition

	//UE_LOG(LogTemp, Warning, TEXT("[%f] UCustomGravityComponent FloorDis"), GetWorld()->GetRealTimeSeconds(), SweepDistance);
}

// DONE
FVector UCircuitCharacterMovement::ComputeGroundMovementDelta(const FVector& Delta, const FHitResult& RampHit, const bool bHitFromLineTrace) const
{
	const FVector FloorNormal = RampHit.ImpactNormal;
	const FVector ContactNormal = GetCapsuleAxisZ(); // @CIRCUIT - Alteration

	if (/*FloorNormal.Z < (1.f - KINDA_SMALL_NUMBER) && FloorNormal.Z > KINDA_SMALL_NUMBER && ContactNormal.Z > KINDA_SMALL_NUMBER &&*/ !bHitFromLineTrace && IsWalkable(RampHit)) // @CIRCUIT - Removal
	{
		/* @CIRCUIT - removed
		// Compute a vector that moves parallel to the surface, by projecting the horizontal movement direction onto the ramp.
		const float FloorDotDelta = (FloorNormal | Delta);
		FVector RampMovement(Delta.X, Delta.Y, -FloorDotDelta / FloorNormal.Z);

		const FVector DeltaNormal = Delta.GetSafeNormal();
		FVector NewDelta = FQuat(ContactNormal ^ DeltaNormal, FMath::Acos(FloorNormal | ContactNormal)).RotateVector(Delta);

		if (bShowDebugLines)
		{
			DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + NewDelta * 30, FColor::Red, true, 10.f);
		}

		if (bMaintainHorizontalGroundVelocity)
		{
			// @CIRCUIT - Addition/Removal
			const FVector NewDeltaNormal = NewDelta.GetSafeNormal();
			NewDelta = NewDeltaNormal * (Delta.Size() / (DeltaNormal | NewDeltaNormal));
			return NewDelta;

			//return RampMovement;
		}
		else
		{
			return RampMovement.GetSafeNormal() * Delta.Size();
		}
		*/
		// Compute a vector that moves parallel to the surface, by projecting the horizontal movement direction onto the ramp.
		// We can't just project Delta onto the plane defined by FloorNormal because the direction changes on spherical geometry.
		const FVector DeltaNormal = Delta.GetSafeNormal();
		FVector NewDelta = FQuat(ContactNormal ^ DeltaNormal, FMath::Acos(FloorNormal | ContactNormal)).RotateVector(Delta);

		if (bShowDebugLines)
		{
			DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + NewDelta * 30, FColor::Red, true, 10.f);
		}

		if (bMaintainHorizontalGroundVelocity)
		{
			const FVector NewDeltaNormal = NewDelta.GetSafeNormal();
			NewDelta = NewDeltaNormal * (Delta.Size() / (DeltaNormal | NewDeltaNormal));
		}

		return NewDelta;
	}

	return Delta;
}

// DONE
bool UCircuitCharacterMovement::ComputePerchResult(const float TestRadius, const FHitResult& InHit, const float InMaxFloorDist, FFindFloorResult& OutPerchFloorResult) const
{
	if (InMaxFloorDist <= 0.f)
	{
		return false;
	}

	// Sweep further than actual requested distance, because a reduced capsule radius means we could miss some hits that the normal radius would contact.
	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	/* @CIRCUIT - removed
	const float InHitAboveBase = FMath::Max(0.f, InHit.ImpactPoint.Z - (InHit.Location.Z - PawnHalfHeight));
	const float PerchLineDist = FMath::Max(0.f, InMaxFloorDist - InHitAboveBase);
	const float PerchSweepDist = FMath::Max(0.f, InMaxFloorDist);

	const float ActualSweepDist = PerchSweepDist + PawnRadius;
	ComputeFloorDist(InHit.Location, PerchLineDist, ActualSweepDist, OutPerchFloorResult, TestRadius);

	if (!OutPerchFloorResult.IsWalkableFloor())
	{
		return false;
	}
	else if (InHitAboveBase + OutPerchFloorResult.FloorDist > InMaxFloorDist)
	{
		// Hit something past max distance
		OutPerchFloorResult.bWalkableFloor = false;
		return false;
	}
	*/

	// Sweep further than actual requested distance, because a reduced capsule radius means we could miss some hits that the normal radius would contact.
	//float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	const FVector CapsuleDown = GetCapsuleAxisZ() * -1.0f;
	const float InHitAboveBase = (InHit.Location + CapsuleDown * ((InHit.ImpactPoint - InHit.Location) | CapsuleDown) -
		(InHit.Location + CapsuleDown * PawnHalfHeight)).Size();
	const float PerchLineDist = FMath::Max(0.0f, InMaxFloorDist - InHitAboveBase);
	const float PerchSweepDist = FMath::Max(0.0f, InMaxFloorDist);

	const float ActualSweepDist = PerchSweepDist + PawnRadius;
	ComputeFloorDist(InHit.Location, PerchLineDist, ActualSweepDist, OutPerchFloorResult, TestRadius);

	if (!OutPerchFloorResult.IsWalkableFloor())
	{
		return false;
	}
	else if (InHitAboveBase + OutPerchFloorResult.FloorDist > InMaxFloorDist)
	{
		// Hit something past max distance.
		OutPerchFloorResult.bWalkableFloor = false;
		return false;
	}

	return true;
}

// DONE
FVector UCircuitCharacterMovement::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	// walking or falling pawns ignore up/down sliding
	if ((IsMovingOnGround() || IsFalling()))
	{
		return FVector::VectorPlaneProject(InputAcceleration, GetCapsuleAxisZ());
	}

	return InputAcceleration;
}

// DONE
bool UCircuitCharacterMovement::DoJump(bool bReplayingMoves)
{
	if (CharacterOwner && CharacterOwner->CanJump())
	{
		const FVector JumpDir = GetCapsuleAxisZ();
		// Don't jump if we can't move up/down.
		if (!bConstrainToPlane || FMath::Abs(PlaneConstraintNormal | JumpDir) < 0.5f)
		{
			// Set to zero the vertical component of velocity.
			Velocity = FVector::VectorPlaneProject(Velocity, JumpDir);

			// Perform jump.
			Velocity += JumpDir * JumpZVelocity;
			SetMovementMode(MOVE_Falling);
			return true;
		}
	}

	return false;
}

// DONE
void UCircuitCharacterMovement::FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const
{
	//SCOPE_CYCLE_COUNTER(STAT_CharFindFloor);
	//UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement CapsuleLocation %f"), GetWorld()->GetRealTimeSeconds(), CapsuleLocation.X);
	// No collision, no floor...
	if (!HasValidData() || !UpdatedComponent->IsQueryCollisionEnabled())
	{
		OutFloorResult.Clear();
		return;
	}

	//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("[Role:%d] FindFloor: %s at location %s"), (int32)CharacterOwner->GetLocalRole(), *GetNameSafe(CharacterOwner), *CapsuleLocation.ToString());
	check(CharacterOwner->GetCapsuleComponent());

	// Increase height check slightly if walking, to prevent floor height adjustment from later invalidating the floor result.
	const float HeightCheckAdjust = (IsMovingOnGround() ? MAX_FLOOR_DIST + KINDA_SMALL_NUMBER : -MAX_FLOOR_DIST);

	float FloorSweepTraceDist = FMath::Max(MAX_FLOOR_DIST, MaxStepHeight + HeightCheckAdjust);
	float FloorLineTraceDist = FloorSweepTraceDist;
	bool bNeedToValidateFloor = true;

	// Sweep floor
	if (FloorLineTraceDist > 0.f || FloorSweepTraceDist > 0.f)
	{
		UCircuitCharacterMovement* MutableThis = const_cast<UCircuitCharacterMovement*>(this);

		if (bAlwaysCheckFloor || !bCanUseCachedLocation || bForceNextFloorCheck || bJustTeleported)
		{
			MutableThis->bForceNextFloorCheck = false;
			ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius(), DownwardSweepResult);
		}
		else
		{
			// Force floor check if base has collision disabled or if it does not block us.
			UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
			const AActor* BaseActor = MovementBase ? MovementBase->GetOwner() : NULL;
			const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

			if (MovementBase != NULL)
			{
				MutableThis->bForceNextFloorCheck = !MovementBase->IsQueryCollisionEnabled()
					|| MovementBase->GetCollisionResponseToChannel(CollisionChannel) != ECR_Block
					|| MovementBaseUtility::IsDynamicBase(MovementBase);
			}

			const bool IsActorBasePendingKill = BaseActor && BaseActor->IsPendingKill();

			if (!bForceNextFloorCheck && !IsActorBasePendingKill && MovementBase)
			{
				//UE_LOG(LogCharacterMovement, Log, TEXT("%s SKIP check for floor"), *CharacterOwner->GetName());
				OutFloorResult = CurrentFloor;
				bNeedToValidateFloor = false;
			}
			else
			{
				MutableThis->bForceNextFloorCheck = false;
				ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius(), DownwardSweepResult);
			}
		}
	}

	// OutFloorResult.HitResult is now the result of the vertical floor check.
	// See if we should try to "perch" at this location.
	if (bNeedToValidateFloor && OutFloorResult.bBlockingHit && !OutFloorResult.bLineTrace)
	{
		const bool bCheckRadius = true;
		if (ShouldComputePerchResult(OutFloorResult.HitResult, bCheckRadius))
		{
			float MaxPerchFloorDist = FMath::Max(MAX_FLOOR_DIST, MaxStepHeight + HeightCheckAdjust);
			if (IsMovingOnGround())
			{
				MaxPerchFloorDist += FMath::Max(0.f, PerchAdditionalHeight);
			}

			FFindFloorResult PerchFloorResult;
			if (ComputePerchResult(GetValidPerchRadius(), OutFloorResult.HitResult, MaxPerchFloorDist, PerchFloorResult))
			{
				// Don't allow the floor distance adjustment to push us up too high, or we will move beyond the perch distance and fall next time.
				const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;
				const float MoveUpDist = (AvgFloorDist - OutFloorResult.FloorDist);
				if (MoveUpDist + PerchFloorResult.FloorDist >= MaxPerchFloorDist)
				{
					OutFloorResult.FloorDist = AvgFloorDist;
				}

				// If the regular capsule is on an unwalkable surface but the perched one would allow us to stand, override the normal to be one that is walkable.
				if (!OutFloorResult.bWalkableFloor)
				{
					// Floor distances are used as the distance of the regular capsule to the point of collision, to make sure AdjustFloorHeight() behaves correctly.
					OutFloorResult.SetFromLineTrace(PerchFloorResult.HitResult, OutFloorResult.FloorDist, FMath::Min(PerchFloorResult.FloorDist, PerchFloorResult.LineDist), true); // @CIRCUIT - changed
				}
			}
			else
			{
				// We had no floor (or an invalid one because it was unwalkable), and couldn't perch here, so invalidate floor (which will cause us to start falling).
				OutFloorResult.bWalkableFloor = false;
			}
		}
	}
}

// KINDA DONE
bool UCircuitCharacterMovement::FloorSweepTest(
	FHitResult& OutHit,
	const FVector& Start,
	const FVector& End,
	ECollisionChannel TraceChannel,
	const struct FCollisionShape& CollisionShape,
	const struct FCollisionQueryParams& Params,
	const struct FCollisionResponseParams& ResponseParam
) const
{
	bool bBlockingHit = false;

	if (!bUseFlatBaseForFloorChecks)
	{
		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, GetCapsuleRotation(), TraceChannel, CollisionShape, Params, ResponseParam);
		//UE_LOG(LogTemp, Error, TEXT("[%f] FloorSweepTest HERE 1 %f %f"), GetWorld()->GetRealTimeSeconds(), Start.X, End.X);
	}
	else
	{
		// Test with a box that is enclosed by the capsule.
		const float CapsuleRadius = CollisionShape.GetCapsuleRadius();
		const float CapsuleHeight = CollisionShape.GetCapsuleHalfHeight();
		const FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector(CapsuleRadius * 0.707f, CapsuleRadius * 0.707f, CapsuleHeight));

		// First test with the box rotated so the corners are along the major axes (ie rotated 45 degrees).
		// @CIRCUIT - unsure unchanged
		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat(FVector(0.f, 0.f, -1.f), PI * 0.25f), TraceChannel, BoxShape, Params, ResponseParam);
		//UE_LOG(LogTemp, Error, TEXT("[%f] FloorSweepTest HERE 2"), GetWorld()->GetRealTimeSeconds());
		if (!bBlockingHit)
		{
			// Test again with the same box, not rotated.
			OutHit.Reset(1.f, false);
			bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, GetCapsuleRotation(), TraceChannel, BoxShape, Params, ResponseParam);
		}
	}

	return bBlockingHit;
}

FVector UCircuitCharacterMovement::GetImpartedMovementBaseVelocity() const
{
	FVector Result = FVector::ZeroVector;
	if (CharacterOwner)
	{
		UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
		if (MovementBaseUtility::IsDynamicBase(MovementBase))
		{
			FVector BaseVelocity = MovementBaseUtility::GetMovementBaseVelocity(MovementBase, CharacterOwner->GetBasedMovement().BoneName);

			if (bImpartBaseAngularVelocity)
			{
				// @CIRCUIT - removed
				//const FVector CharacterBasePosition = (UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
				//const FVector BaseTangentialVel = MovementBaseUtility::GetMovementBaseTangentialVelocity(MovementBase, CharacterOwner->GetBasedMovement().BoneName, CharacterBasePosition);
				//BaseVelocity += BaseTangentialVel;

				const FVector CharacterBasePosition = (UpdatedComponent->GetComponentLocation() - GetCapsuleAxisZ() * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
				const FVector BaseTangentialVel = MovementBaseUtility::GetMovementBaseTangentialVelocity(MovementBase, CharacterOwner->GetBasedMovement().BoneName, CharacterBasePosition);
				BaseVelocity += BaseTangentialVel;
			}

			if (bImpartBaseVelocityX)
			{
				Result.X = BaseVelocity.X;
			}
			if (bImpartBaseVelocityY)
			{
				Result.Y = BaseVelocity.Y;
			}
			if (bImpartBaseVelocityZ)
			{
				Result.Z = BaseVelocity.Z;
			}
		}
	}

	return Result;
}

// DONE
FVector UCircuitCharacterMovement::GetLedgeMove(const FVector& OldLocation, const FVector& Delta, const FVector& GravDir) const
{
	if (!HasValidData() || Delta.IsZero())
	{
		return FVector::ZeroVector;
	}

	FVector SideDir = FVector::VectorPlaneProject(Delta, GravDir); // @CIRCUIT - alteration

	// try left
	SideDir = FQuat(GravDir, PI * 0.5f).RotateVector(SideDir); // @CIRCUIT - addition
	if (CheckLedgeDirection(OldLocation, SideDir, GravDir))
	{
		return SideDir;
	}

	// try right
	SideDir *= -1.f;
	if (CheckLedgeDirection(OldLocation, SideDir, GravDir))
	{
		return SideDir;
	}

	return FVector::ZeroVector;
}

void UCircuitCharacterMovement::HandleImpact(const FHitResult& Impact, float TimeSlice, const FVector& MoveDelta)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharHandleImpact);

	/*
	if (CharacterOwner)
	{
		CharacterOwner->MoveBlockedBy(Impact);
	}

	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		// Also notify path following!
		PFAgent->OnMoveBlockedBy(Impact);
	}

	APawn* OtherPawn = Impact.HitObjectHandle.FetchActor<APawn>();
	if (OtherPawn)
	{
		NotifyBumpedPawn(OtherPawn);
	}

	if (bEnablePhysicsInteraction)
	{
		const FVector ForceAccel = Acceleration + (IsFalling() ? FVector(0.f, 0.f, GetGravityZ()) : FVector::ZeroVector);
		ApplyImpactPhysicsForces(Impact, ForceAccel, Velocity);
	}
	*/

	if (CharacterOwner)
	{
		CharacterOwner->MoveBlockedBy(Impact);
	}

	APawn* OtherPawn = Cast<APawn>(Impact.GetActor());
	if (OtherPawn)
	{
		NotifyBumpedPawn(OtherPawn);
	}

	if (bEnablePhysicsInteraction)
	{
		const FVector ForceAccel = Acceleration + (IsFalling() ? GetGravityScaled() : FVector::ZeroVector);
		ApplyImpactPhysicsForces(Impact, ForceAccel, Velocity);
	}
}

// DONE
bool UCircuitCharacterMovement::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const
{
	//UE_LOG(LogTemp, Warning, TEXT("[%f] IsValidLandingSpot 1 %f"), GetWorld()->GetRealTimeSeconds(), CapsuleLocation.X);
	if (!Hit.bBlockingHit)
	{
		return false;
	}

	const FVector CapsuleDown = GetCapsuleAxisZ() * -1.0f;

	// Skip some checks if penetrating. Penetration will be handled by the FindFloor call (using a smaller capsule)
	if (!Hit.bStartPenetrating)
	{
		//UE_LOG(LogTemp, Warning, TEXT("[%f] IsValidLandingSpot 2 %f"), GetWorld()->GetRealTimeSeconds(), CapsuleLocation.X);
		// Reject unwalkable floor normals.
		if (!IsWalkable(Hit))
		{
			return false;
		}

		float PawnRadius, PawnHalfHeight;
		CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

		// Get the axis of the capsule bounded by the following two end points.
		const FVector BottomPoint = Hit.Location + CapsuleDown * FMath::Max(0.0f, PawnHalfHeight - PawnRadius);
		const FVector TopPoint = Hit.Location - CapsuleDown;
		const FVector Segment = TopPoint - BottomPoint;

		// Project the impact point on the segment.
		const float Alpha = ((Hit.ImpactPoint - BottomPoint) | Segment) / Segment.SizeSquared();

		//UE_LOG(LogTemp, Warning, TEXT("[%f] IsValidLandingSpot 3 %f"), GetWorld()->GetRealTimeSeconds(), CapsuleLocation.X);

		// Reject hits that are above our lower hemisphere (can happen when sliding "down" a vertical surface).
		if (Alpha >= 0.0f)
		{
			//UE_LOG(LogTemp, Warning, TEXT("[%f] IsValidLandingSpot 4 %f"), GetWorld()->GetRealTimeSeconds(), CapsuleLocation.X);
			return false;
		}

		// Reject hits that are barely on the cusp of the radius of the capsule.
		if (!IsWithinEdgeToleranceCircuit(Hit.Location, CapsuleDown, Hit.ImpactPoint, PawnRadius))
		{
			//UE_LOG(LogTemp, Warning, TEXT("[%f] IsValidLandingSpot 5 PawnRadius: %f Hit.Location %f Hit.ImpactPoint: %f"), GetWorld()->GetRealTimeSeconds(), PawnRadius, Hit.Location.X, Hit.ImpactPoint.X);
			return false;
		}

		/* @CIRCUIT - removed
		// Reject hits that are above our lower hemisphere (can happen when sliding down a vertical surface).
		const float LowerHemisphereZ = Hit.Location.Z - PawnHalfHeight + PawnRadius;
		if (Hit.ImpactPoint.Z >= LowerHemisphereZ)
		{
			return false;
		}

		// Reject hits that are barely on the cusp of the radius of the capsule
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			return false;
		}
		*/
	}
	else
	{
		/* @CIRCUIT - removed
		// Penetrating
		if (Hit.Normal.Z < KINDA_SMALL_NUMBER)
		{
			// Normal is nearly horizontal or downward, that's a penetration adjustment next to a vertical or overhanging wall. Don't pop to the floor.
			return false;
		}
		*/

		// Penetrating.
		if ((Hit.Normal | CapsuleDown) > -KINDA_SMALL_NUMBER)
		{
			// Normal is nearly horizontal or downward, that's a penetration adjustment next to a vertical or overhanging wall. Don't pop to the floor.
			return false;
		}
	}	
	FFindFloorResult FloorResult;
	FindFloor(CapsuleLocation, FloorResult, false, &Hit);

	if (!FloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}

// DONE
bool UCircuitCharacterMovement::IsWalkable(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit())
	{
		// No hit, or starting in penetration.
		return false;
	}

	float TestWalkableZ = GetWalkableFloorZ();

	// See if this component overrides the walkable floor z.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (HitComponent)
	{
		const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
		TestWalkableZ = SlopeOverride.ModifyWalkableFloorZ(TestWalkableZ);
	}

	// Can't walk on this surface if it is too steep.
	if ((Hit.ImpactNormal | GetCapsuleAxisZ()) < TestWalkableZ)
	{
		return false;
	}

	return true;
}

// DONE
void UCircuitCharacterMovement::JumpOff(AActor* MovementBaseActor)
{
	if (!bPerformingJumpOff)
	{
		bPerformingJumpOff = true;
		if (CharacterOwner)
		{
			/* @CIRCUIT - removed
			const float MaxSpeed = GetMaxSpeed() * 0.85f;
			Velocity += MaxSpeed * GetBestDirectionOffActor(MovementBaseActor);
			if (Velocity.Size2D() > MaxSpeed)
			{
				Velocity = MaxSpeed * Velocity.GetSafeNormal();
			}
			Velocity.Z = JumpOffJumpZFactor * JumpZVelocity;
			SetMovementMode(MOVE_Falling);
			*/

			const float MaxSpeed = GetMaxSpeed() * 0.85f;
			Velocity += GetBestDirectionOffActor(MovementBaseActor) * MaxSpeed;

			const FVector JumpDir = GetCapsuleAxisZ();
			FVector Velocity2D = FVector::VectorPlaneProject(Velocity, JumpDir);

			if (Velocity2D.Size() > MaxSpeed)
			{
				Velocity2D = FVector::VectorPlaneProject(Velocity.GetSafeNormal() * MaxSpeed, JumpDir);
			}

			Velocity = Velocity2D + JumpDir * (JumpZVelocity * JumpOffJumpZFactor);
			SetMovementMode(MOVE_Falling);
		}
		bPerformingJumpOff = false;
	}
}

void UCircuitCharacterMovement::MaintainHorizontalGroundVelocity()
{
	return; // @CIRCUIT - addition

	if (Velocity.Z != 0.f)
	{
		if (bMaintainHorizontalGroundVelocity)
		{
			// Ramp movement already maintained the velocity, so we just want to remove the vertical component.
			Velocity.Z = 0.f;
		}
		else
		{
			// Rescale velocity to be horizontal but maintain magnitude of last update.
			Velocity = Velocity.GetSafeNormal2D() * Velocity.Size();
		}
	}
}

// DONE
void UCircuitCharacterMovement::MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	// Move along the current floor
	const FVector CapsuleUp = GetCapsuleAxisZ(); // @CIRCUIT - Additions

	const FVector Delta = FVector::VectorPlaneProject(InVelocity, CapsuleUp) * DeltaSeconds;;
	FHitResult Hit(1.f);
	FVector RampVector = ComputeGroundMovementDelta(Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);
	float LastMoveTimeSlice = DeltaSeconds;

	if (bShowDebugLines)
	{
		DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + (RampVector * 20), FColor::Blue, true, 10.f, 10, 6.0f);
	}

	if (Hit.bStartPenetrating)
	{
		// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch.
		HandleImpact(Hit);
		SlideAlongSurface(Delta, 1.f, Hit.Normal, Hit, true);

		if (Hit.bStartPenetrating)
		{
			OnCharacterStuckInGeometry(&Hit);
		}
	}
	else if (Hit.IsValidBlockingHit())
	{
		// We impacted something (most likely another ramp, but possibly a barrier).
		float PercentTimeApplied = Hit.Time;
		if (Hit.Time > 0.0f && (Hit.Normal | CapsuleUp) > KINDA_SMALL_NUMBER && IsWalkable(Hit))
		{
			// Another walkable ramp.
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			RampVector = ComputeGroundMovementDelta(Delta * InitialPercentRemaining, Hit, false);
			LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
			SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);

			const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
		}

		if (Hit.IsValidBlockingHit())
		{
			if (CanStepUp(Hit) || (CharacterOwner->GetMovementBase() != nullptr && Hit.HitObjectHandle == CharacterOwner->GetMovementBase()->GetOwner()))
			{
				// hit a barrier, try to step up
				const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
				const FVector GravDir = GetGravityDirection();
				if (!StepUp(GravDir, Delta * (1.f - PercentTimeApplied), Hit, OutStepDownResult))
				{
					//UE_LOG(LogCharacterMovement, Verbose, TEXT("- StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					HandleImpact(Hit, LastMoveTimeSlice, RampVector);
					SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
				}
				else
				{
					//UE_LOG(LogCharacterMovement, Verbose, TEXT("+ StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					if (!bMaintainHorizontalGroundVelocity && false) // @CIRCUIT - Addition
					{
						// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments. Only consider horizontal movement.
						bJustTeleported = true;
						const float StepUpTimeSlice = (1.f - PercentTimeApplied) * DeltaSeconds;
						if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && StepUpTimeSlice >= KINDA_SMALL_NUMBER)
						{
							Velocity = (UpdatedComponent->GetComponentLocation() - PreStepUpLocation) / StepUpTimeSlice;
							Velocity.Z = 0; // @CIRCUIT - unsure kept in
						}
					}
				}
			}
			else if (Hit.Component.IsValid() && !Hit.Component.Get()->CanCharacterStepUp(CharacterOwner))
			{
				HandleImpact(Hit, LastMoveTimeSlice, RampVector);
				SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
			}
		}
	}
}

// DONE
void UCircuitCharacterMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (!HasValidData())
	{
		return;
	}

	// Update collision settings if needed
	if (MovementMode == MOVE_NavWalking)
	{
		// Reset cached nav location used by NavWalking
		CachedNavLocation = FNavLocation();

		SetGroundMovementMode(MovementMode);
		//GroundMovementMode = MovementMode; // @CIRCUIT - using above instead
		
		// Walking uses only XY velocity
		Velocity.Z = 0.f; // @CIRCUIT - unsure remove at some point?
		SetNavWalkingPhysics(true);
	}
	else if (PreviousMovementMode == MOVE_NavWalking)
	{
		if (MovementMode == DefaultLandMovementMode || IsWalking())
		{
			const bool bSucceeded = TryToLeaveNavWalking();
			if (!bSucceeded)
			{
				return;
			}
		}
		else
		{
			SetNavWalkingPhysics(false);
		}
	}

	// React to changes in the movement mode.
	if (MovementMode == MOVE_Walking)
	{
		if (GetNetMode() == ENetMode::NM_Client) {
			UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] MovementMode Walking"), GetWorld()->GetRealTimeSeconds());
		}
		// Walking uses only XY velocity, and must be on a walkable floor, with a Base.
		//Velocity.Z = 0.f; // @CIRCUIT - removed
		bCrouchMaintainsBaseLocation = true;
		SetGroundMovementMode(MovementMode);
		//GroundMovementMode = MovementMode; // @CIRCUIT - using above instead

		// make sure we update our new floor/base on initial entry of the walking physics
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false, nullptr);

		UpdateComponentRotation(); // @CIRCUIT - addition

		AdjustFloorHeight();
		SetBaseFromFloor(CurrentFloor);

		// Walking uses only horizontal velocity.
		MaintainHorizontalGroundVelocity(); // @CIRCUIT - addition
	}
	else
	{
		CurrentFloor.Clear();
		bCrouchMaintainsBaseLocation = false;

		UpdateComponentRotation(); // @CIRCUIT - addition

		if (MovementMode == MOVE_Falling)
		{
			if (GetNetMode() == ENetMode::NM_Client) {
				UE_LOG(LogTemp, Warning, TEXT("[%f] [CLIENT] MovementMode Falling"), GetWorld()->GetRealTimeSeconds());
			}
			Velocity += GetImpartedMovementBaseVelocity();
			CharacterOwner->Falling();
		}

		SetBase(NULL);

		if (MovementMode == MOVE_None)
		{
			// Kill velocity and clear queued up events
			StopMovementKeepPathing();
			CharacterOwner->ResetJumpState();
			//ClearAccumulatedForces(); @CIRCUIT - removed
		}
	}

	if (MovementMode == MOVE_Falling && PreviousMovementMode != MOVE_Falling)
	{
		IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
		if (PFAgent)
		{
			PFAgent->OnStartedFalling();
		}
	}

	CharacterOwner->OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	//ensureMsgf(GroundMovementMode == MOVE_Walking || GroundMovementMode == MOVE_NavWalking, TEXT("Invalid GroundMovementMode %d. MovementMode: %d, PreviousMovementMode: %d"), GroundMovementMode.GetValue(), MovementMode.GetValue(), PreviousMovementMode);
}

// KINDA DONE
void UCircuitCharacterMovement::PerformMovement(float DeltaSeconds)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementPerformMovement);

	const UWorld* MyWorld = GetWorld();
	if (!HasValidData() || MyWorld == nullptr)
	{
		return;
	}

	// no movement if we can't move, or if currently doing physical simulation on UpdatedComponent
	if (MovementMode == MOVE_None || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
	{
		if (!CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
		{
			// Consume root motion
			if (CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh())
			{
				TickCharacterPose(DeltaSeconds);
				RootMotionParams.Clear();
			}
			if (CurrentRootMotion.HasActiveRootMotionSources())
			{
				CurrentRootMotion.Clear();
			}
		}
		// Clear pending physics forces
		ClearAccumulatedForces();
		return;
	}

	UpdateComponentRotation(); // @CIRCUIT - addition

	// Force floor update if we've moved outside of CharacterMovement since last update.
	bForceNextFloorCheck |= (IsMovingOnGround() && UpdatedComponent->GetComponentLocation() != LastUpdateLocation);

	// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened since last update.
	// @CIRCUIT - unsure kept in
	if (CurrentRootMotion.HasAdditiveVelocity())
	{
		const FVector Adjustment = (Velocity - LastUpdateVelocity);
		CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		{
			if (!Adjustment.IsNearlyZero())
			{
				FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement HasAdditiveVelocity LastUpdateVelocityAdjustment LastPreAdditiveVelocity(%s) Adjustment(%s)"),
					*CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString(), *Adjustment.ToCompactString());
				RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
			}
		}
#endif
	}

	FVector OldVelocity;
	FVector OldLocation;

	// Scoped updates can improve performance of multiple MoveComponent calls.
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

		MaybeUpdateBasedMovement(DeltaSeconds);

		// Clean up invalid RootMotion Sources.
		// This includes RootMotion sources that ended naturally.
		// They might want to perform a clamp on velocity or an override, 
		// so we want this to happen before ApplyAccumulatedForces and HandlePendingLaunch as to not clobber these.
		const bool bHasRootMotionSources = HasRootMotionSources();
		if (bHasRootMotionSources && !CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
		{
			//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceCalculate);

			const FVector VelocityBeforeCleanup = Velocity;
			CurrentRootMotion.CleanUpInvalidRootMotion(DeltaSeconds, *CharacterOwner, *this);

#if ROOT_MOTION_DEBUG
			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
			{
				if (Velocity != VelocityBeforeCleanup)
				{
					const FVector Adjustment = Velocity - VelocityBeforeCleanup;
					FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement CleanUpInvalidRootMotion Velocity(%s) VelocityBeforeCleanup(%s) Adjustment(%s)"),
						*Velocity.ToCompactString(), *VelocityBeforeCleanup.ToCompactString(), *Adjustment.ToCompactString());
					RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
				}
			}
#endif
		}

		OldVelocity = Velocity;
		OldLocation = UpdatedComponent->GetComponentLocation();

		ApplyAccumulatedForces(DeltaSeconds);

		// Update the character state before we do our movement
		UpdateCharacterStateBeforeMovement(DeltaSeconds);

		if (MovementMode == MOVE_NavWalking && bWantsToLeaveNavWalking)
		{
			TryToLeaveNavWalking();
		}

		// Character::LaunchCharacter() has been deferred until now.
		HandlePendingLaunch();
		ClearAccumulatedForces();

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		{
			if (OldVelocity != Velocity)
			{
				const FVector Adjustment = Velocity - OldVelocity;
				FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement ApplyAccumulatedForces+HandlePendingLaunch Velocity(%s) OldVelocity(%s) Adjustment(%s)"),
					*Velocity.ToCompactString(), *OldVelocity.ToCompactString(), *Adjustment.ToCompactString());
				RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
			}
		}
#endif

		// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened due to ApplyAccumulatedForces/HandlePendingLaunch
		if (CurrentRootMotion.HasAdditiveVelocity())
		{
			const FVector Adjustment = (Velocity - OldVelocity);
			CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;

#if ROOT_MOTION_DEBUG
			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
			{
				if (!Adjustment.IsNearlyZero())
				{
					FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement HasAdditiveVelocity AccumulatedForces LastPreAdditiveVelocity(%s) Adjustment(%s)"),
						*CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString(), *Adjustment.ToCompactString());
					RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
				}
			}
#endif
		}

		// Prepare Root Motion (generate/accumulate from root motion sources to be used later)
		if (bHasRootMotionSources && !CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
		{
			// Animation root motion - If using animation RootMotion, tick animations before running physics.
			if (CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh())
			{
				TickCharacterPose(DeltaSeconds);

				// Make sure animation didn't trigger an event that destroyed us
				if (!HasValidData())
				{
					return;
				}

				// For local human clients, save off root motion data so it can be used by movement networking code.
				if (CharacterOwner->IsLocallyControlled() && (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy) && CharacterOwner->IsPlayingNetworkedRootMotionMontage())
				{
					CharacterOwner->ClientRootMotionParams = RootMotionParams;
				}
			}

			// Generates root motion to be used this frame from sources other than animation
			{
				//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceCalculate);
				CurrentRootMotion.PrepareRootMotion(DeltaSeconds, *CharacterOwner, *this, true);
			}

			// For local human clients, save off root motion data so it can be used by movement networking code.
			if (CharacterOwner->IsLocallyControlled() && (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy))
			{
				CharacterOwner->SavedRootMotion = CurrentRootMotion;
			}
		}

		// Apply Root Motion to Velocity
		if (CurrentRootMotion.HasOverrideVelocity() || HasAnimRootMotion())
		{
			// Animation root motion overrides Velocity and currently doesn't allow any other root motion sources
			if (HasAnimRootMotion())
			{
				// Convert to world space (animation root motion is always local)
				USkeletalMeshComponent* SkelMeshComp = CharacterOwner->GetMesh();
				if (SkelMeshComp)
				{
					// Convert Local Space Root Motion to world space. Do it right before used by physics to make sure we use up to date transforms, as translation is relative to rotation.
					RootMotionParams.Set(ConvertLocalRootMotionToWorld(RootMotionParams.GetRootMotionTransform(), DeltaSeconds));
				}

				// Then turn root motion to velocity to be used by various physics modes.
				if (DeltaSeconds > 0.f)
				{
					AnimRootMotionVelocity = CalcAnimRootMotionVelocity(RootMotionParams.GetRootMotionTransform().GetTranslation(), DeltaSeconds, Velocity);
					Velocity = ConstrainAnimRootMotionVelocity(AnimRootMotionVelocity, Velocity);
				}

				UE_LOG(LogRootMotion, Log, TEXT("PerformMovement WorldSpaceRootMotion Translation: %s, Rotation: %s, Actor Facing: %s, Velocity: %s")
					, *RootMotionParams.GetRootMotionTransform().GetTranslation().ToCompactString()
					, *RootMotionParams.GetRootMotionTransform().GetRotation().Rotator().ToCompactString()
					, *CharacterOwner->GetActorForwardVector().ToCompactString()
					, *Velocity.ToCompactString()
				);
			}
			else
			{
				// We don't have animation root motion so we apply other sources
				if (DeltaSeconds > 0.f)
				{
					//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceApply);

					const FVector VelocityBeforeOverride = Velocity;
					FVector NewVelocity = Velocity;
					CurrentRootMotion.AccumulateOverrideRootMotionVelocity(DeltaSeconds, *CharacterOwner, *this, NewVelocity);
					Velocity = NewVelocity;

#if ROOT_MOTION_DEBUG
					if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
					{
						if (VelocityBeforeOverride != Velocity)
						{
							FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement AccumulateOverrideRootMotionVelocity Velocity(%s) VelocityBeforeOverride(%s)"),
								*Velocity.ToCompactString(), *VelocityBeforeOverride.ToCompactString());
							RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
						}
					}
#endif
				}
			}
		}

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		{
			FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement Velocity(%s) OldVelocity(%s)"),
				*Velocity.ToCompactString(), *OldVelocity.ToCompactString());
			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		}
#endif

		// NaN tracking
		//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("UCharacterMovementComponent::PerformMovement: Velocity contains NaN (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

		// Clear jump input now, to allow movement events to trigger it for next update.
		CharacterOwner->ClearJumpInput(DeltaSeconds);
		NumJumpApexAttempts = 0;

		// change position
		StartNewPhysics(DeltaSeconds, 0);

		if (!HasValidData())
		{
			return;
		}

		// Update character state based on change from movement
		UpdateCharacterStateAfterMovement(DeltaSeconds);

		if ((bAllowPhysicsRotationDuringAnimRootMotion || !HasAnimRootMotion()) && !CharacterOwner->IsMatineeControlled())
		{
			PhysicsRotation(DeltaSeconds);
		}

		// Apply Root Motion rotation after movement is complete.
		if (HasAnimRootMotion())
		{
			const FQuat OldActorRotationQuat = UpdatedComponent->GetComponentQuat();
			const FQuat RootMotionRotationQuat = RootMotionParams.GetRootMotionTransform().GetRotation();
			if (!RootMotionRotationQuat.IsIdentity())
			{
				const FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
				MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
			}

#if !(UE_BUILD_SHIPPING)
			// debug
			if (false)
			{
				const FRotator OldActorRotation = OldActorRotationQuat.Rotator();
				const FVector ResultingLocation = UpdatedComponent->GetComponentLocation();
				const FRotator ResultingRotation = UpdatedComponent->GetComponentRotation();

				// Show current position
				DrawDebugCoordinateSystem(MyWorld, CharacterOwner->GetMesh()->GetComponentLocation() + FVector(0, 0, 1), ResultingRotation, 50.f, false);

				// Show resulting delta move.
				DrawDebugLine(MyWorld, OldLocation, ResultingLocation, FColor::Red, false, 10.f);

				// Log details.
				UE_LOG(LogRootMotion, Warning, TEXT("PerformMovement Resulting DeltaMove Translation: %s, Rotation: %s, MovementBase: %s"), //-V595
					*(ResultingLocation - OldLocation).ToCompactString(), *(ResultingRotation - OldActorRotation).GetNormalized().ToCompactString(), *GetNameSafe(CharacterOwner->GetMovementBase()));

				const FVector RMTranslation = RootMotionParams.GetRootMotionTransform().GetTranslation();
				const FRotator RMRotation = RootMotionParams.GetRootMotionTransform().GetRotation().Rotator();
				UE_LOG(LogRootMotion, Warning, TEXT("PerformMovement Resulting DeltaError Translation: %s, Rotation: %s"),
					*(ResultingLocation - OldLocation - RMTranslation).ToCompactString(), *(ResultingRotation - OldActorRotation - RMRotation).GetNormalized().ToCompactString());
			}
#endif // !(UE_BUILD_SHIPPING)

			// Root Motion has been used, clear
			RootMotionParams.Clear();
		}
		else if (CurrentRootMotion.HasActiveRootMotionSources())
		{
			FQuat RootMotionRotationQuat;
			if (CharacterOwner && UpdatedComponent && CurrentRootMotion.GetOverrideRootMotionRotation(DeltaSeconds, *CharacterOwner, *this, RootMotionRotationQuat))
			{
				const FQuat OldActorRotationQuat = UpdatedComponent->GetComponentQuat();
				const FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
				MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
			}
		}

		// consume path following requested velocity
		bHasRequestedVelocity = false;

		OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	} // End scoped movement update

	// Call external post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);

	SaveBaseLocation();
	UpdateComponentVelocity();

	const bool bHasAuthority = CharacterOwner && CharacterOwner->HasAuthority();

	// If we move we want to avoid a long delay before replication catches up to notice this change, especially if it's throttling our rate.
	if (bHasAuthority && UNetDriver::IsAdaptiveNetUpdateFrequencyEnabled() && UpdatedComponent)
	{
		UNetDriver* NetDriver = MyWorld->GetNetDriver();
		if (NetDriver && NetDriver->IsServer())
		{
			FNetworkObjectInfo* NetActor = NetDriver->FindOrAddNetworkObjectInfo(CharacterOwner);

			if (NetActor && MyWorld->GetTimeSeconds() <= NetActor->NextUpdateTime && NetDriver->IsNetworkActorUpdateFrequencyThrottled(*NetActor))
			{
				if (ShouldCancelAdaptiveReplication())
				{
					NetDriver->CancelAdaptiveReplication(*NetActor);
				}
			}
		}
	}

	const FVector NewLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	const FQuat NewRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;

	if (bHasAuthority && UpdatedComponent && !IsNetMode(NM_Client))
	{
		const bool bLocationChanged = (NewLocation != LastUpdateLocation);
		const bool bRotationChanged = (NewRotation != LastUpdateRotation);
		if (bLocationChanged || bRotationChanged)
		{
			// Update ServerLastTransformUpdateTimeStamp. This is used by Linear smoothing on clients to interpolate positions with the correct delta time,
			// so the timestamp should be based on the client's move delta (ServerAccumulatedClientTimeStamp), not the server time when receiving the RPC.
			const bool bIsRemotePlayer = (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);
			const FNetworkPredictionData_Server_Character* ServerData = bIsRemotePlayer ? GetPredictionData_Server_Character() : nullptr;
			if (bIsRemotePlayer && ServerData /* && CharacterMovementCVars::NetUseClientTimestampForReplicatedTransform*/)
			{
				ServerLastTransformUpdateTimeStamp = float(ServerData->ServerAccumulatedClientTimeStamp);
			}
			else
			{
				ServerLastTransformUpdateTimeStamp = MyWorld->GetTimeSeconds();
			}
		}
	}

	// @CIRCUIT - unsure added
	SaveBaseLocation();
	UpdateComponentVelocity();

	LastUpdateLocation = NewLocation;
	LastUpdateRotation = NewRotation; // @CIRCUIT - unsure kept in
	LastUpdateVelocity = Velocity; // @CIRCUIT - unsure kept in
}


// DONE
void UCircuitCharacterMovement::PhysFalling(float deltaTime, int32 Iterations)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysFalling);
	//CSV_SCOPED_TIMING_STAT_EXCLUSIVE(CharPhysFalling);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	// @CIRCUIT - addition
	const FVector GravityDir = GetGravityDirection();
	if (GravityDir.IsZero())
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement PhysFalling 1"), GetWorld()->GetRealTimeSeconds());

	FVector FallAcceleration = GetFallingLateralAcceleration(deltaTime);
	//FallAcceleration.Z = 0.f; // @CIRCUIT - removed
	const bool bHasLimitedAirControl = ShouldLimitAirControl(deltaTime, FallAcceleration);

	float remainingTime = deltaTime;
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		Iterations++;
		float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
		bJustTeleported = false;

		RestorePreAdditiveRootMotionVelocity(); // @CIRCUIT - unsure kept in

		const FVector OldVelocity = Velocity;

		// Apply input
		const float MaxDecel = GetMaxBrakingDeceleration();
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			// @CIRCUIT - addition
			const FVector OldVelocityZ = GravityDir * (Velocity | GravityDir);
			// Compute Velocity
			{
				// Acceleration = FallAcceleration for CalcVelocity(), but we restore it after using it.
				TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);
				Velocity = FVector::VectorPlaneProject(Velocity, GetGravityDirection());
				CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
				Velocity = FVector::VectorPlaneProject(Velocity, GetGravityDirection()) + OldVelocityZ;
			}
		}

		//UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement PhysFalling 2"), GetWorld()->GetRealTimeSeconds());

		// Compute current gravity
		const FVector Gravity = GetGravityScaled();
		float GravityTime = timeTick;

		// If jump is providing force, gravity may be affected.
		bool bEndingJumpForce = false;
		if (CharacterOwner->JumpForceTimeRemaining > 0.0f)
		{
			// Consume some of the force time. Only the remaining time (if any) is affected by gravity when bApplyGravityWhileJumping=false.
			const float JumpForceTime = FMath::Min(CharacterOwner->JumpForceTimeRemaining, timeTick);
			GravityTime = bApplyGravityWhileJumping ? timeTick : FMath::Max(0.0f, timeTick - JumpForceTime);

			// Update Character state
			CharacterOwner->JumpForceTimeRemaining -= JumpForceTime;
			if (CharacterOwner->JumpForceTimeRemaining <= 0.0f)
			{
				CharacterOwner->ResetJumpState();
				bEndingJumpForce = true;
			}
		}

		// Apply gravity
		Velocity = NewFallVelocity(Velocity, Gravity, GravityTime);

		/* @CIRCUIT - unsure removed
		// See if we need to sub-step to exactly reach the apex. This is important for avoiding "cutting off the top" of the trajectory as framerate varies.
		if (CharacterMovementCVars::ForceJumpPeakSubstep &&  OldVelocity.Z > 0.f && Velocity.Z <= 0.f && NumJumpApexAttempts < MaxJumpApexAttemptsPerSimulation)
		{
			const FVector DerivedAccel = (Velocity - OldVelocity) / timeTick;
			if (!FMath::IsNearlyZero(DerivedAccel.Z))
			{
				const float TimeToApex = -OldVelocity.Z / DerivedAccel.Z;

				// The time-to-apex calculation should be precise, and we want to avoid adding a substep when we are basically already at the apex from the previous iteration's work.
				const float ApexTimeMinimum = 0.0001f;
				if (TimeToApex >= ApexTimeMinimum && TimeToApex < timeTick)
				{
					const FVector ApexVelocity = OldVelocity + DerivedAccel * TimeToApex;
					Velocity = ApexVelocity;
					Velocity.Z = 0.f; // Should be nearly zero anyway, but this makes apex notifications consistent.

					// We only want to move the amount of time it takes to reach the apex, and refund the unused time for next iteration.
					remainingTime += (timeTick - TimeToApex);
					timeTick = TimeToApex;
					Iterations--;
					NumJumpApexAttempts++;
				}
			}
		}
		*/
		//UE_LOG(LogCharacterMovement, Log, TEXT("dt=(%.6f) OldLocation=(%s) OldVelocity=(%s) NewVelocity=(%s)"), timeTick, *(UpdatedComponent->GetComponentLocation()).ToString(), *OldVelocity.ToString(), *Velocity.ToString());
		ApplyRootMotionToVelocity(timeTick);

		if (bNotifyApex && (Velocity.Z < 0.f))
		{
			// Just passed jump apex since now going down
			bNotifyApex = false;
			NotifyJumpApex();
		}

		// Compute change in position (using midpoint integration method).
		FVector Adjusted = 0.5f * (OldVelocity + Velocity) * timeTick;

		// Special handling if ending the jump force where we didn't apply gravity during the jump.
		if (bEndingJumpForce && !bApplyGravityWhileJumping)
		{
			// We had a portion of the time at constant speed then a portion with acceleration due to gravity.
			// Account for that here with a more correct change in position.
			const float NonGravityTime = FMath::Max(0.f, timeTick - GravityTime);
			Adjusted = (OldVelocity * NonGravityTime) + (0.5f * (OldVelocity + Velocity) * GravityTime);
		}

		// Move
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Adjusted, PawnRotation, true, Hit);

		if (!HasValidData())
		{
			return;
		}

		float LastMoveTimeSlice = timeTick;
		float subTimeTickRemaining = timeTick * (1.f - Hit.Time);
		//UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement PhysFalling 3"), GetWorld()->GetRealTimeSeconds());
		if (IsSwimming()) //just entered water
		{
			//UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement PhysFalling 3 swimming?"), GetWorld()->GetRealTimeSeconds());

			remainingTime += subTimeTickRemaining;
			StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
			return;
		}
		else if (Hit.bBlockingHit)
		{
			if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
			{
				remainingTime += subTimeTickRemaining;
				ProcessLanded(Hit, remainingTime, Iterations);
				return;
			}
			else
			{
				// Compute impact deflection based on final velocity, not integration step.
				// This allows us to compute a new velocity from the deflected vector, and ensures the full gravity effect is included in the slide result.
				Adjusted = Velocity * timeTick;

				// See if we can convert a normally invalid landing spot (based on the hit result) to a usable one.
				if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(timeTick, Adjusted, Hit))
				{
					const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
					FFindFloorResult FloorResult;

					FindFloor(PawnLocation, FloorResult, false, nullptr);
					if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
					{
						remainingTime += subTimeTickRemaining;
						ProcessLanded(FloorResult.HitResult, remainingTime, Iterations);
						return;
					}
				}

				HandleImpact(Hit, LastMoveTimeSlice, Adjusted);

				// If we've changed physics mode, abort.
				if (!HasValidData() || !IsFalling())
				{
					return;
				}

				// Limit air control based on what we hit.
				// We moved to the impact point using air control, but may want to deflect from there based on a limited air control acceleration.
				FVector VelocityNoAirControl = OldVelocity;
				FVector AirControlAccel = Acceleration;
				if (bHasLimitedAirControl)
				{
					// Compute VelocityNoAirControl
					{
						/* @CIRCUIT - removed
						// Find velocity *without* acceleration.
						TGuardValue<FVector> RestoreAcceleration(Acceleration, FVector::ZeroVector);
						TGuardValue<FVector> RestoreVelocity(Velocity, OldVelocity);
						//Velocity.Z = 0.f; // @CIRCUIT - my removed
						CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
						VelocityNoAirControl = FVector(Velocity.X, Velocity.Y, OldVelocity.Z);
						VelocityNoAirControl = NewFallVelocity(VelocityNoAirControl, Gravity, GravityTime);
						*/

						const FVector AirControlDeltaV = LimitAirControl(LastMoveTimeSlice, AirControlAccel, Hit, false) * LastMoveTimeSlice;
						Adjusted = (VelocityNoAirControl + AirControlDeltaV) * LastMoveTimeSlice;
					}

					// @CIRCUIT - unsure kept in
					const bool bCheckLandingSpot = false; // we already checked above.
					AirControlAccel = (Velocity - VelocityNoAirControl) / timeTick;
					const FVector AirControlDeltaV = LimitAirControl(LastMoveTimeSlice, AirControlAccel, Hit, bCheckLandingSpot) * LastMoveTimeSlice;
					Adjusted = (VelocityNoAirControl + AirControlDeltaV) * LastMoveTimeSlice;
				}

				const FVector OldHitNormal = Hit.Normal;
				const FVector OldHitImpactNormal = Hit.ImpactNormal;
				FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, OldHitNormal, Hit);

				// Compute velocity after deflection (only gravity component for RootMotion)
				if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
				{
					/* @CIRCUIT - removed
					const FVector NewVelocity = (Delta / subTimeTickRemaining);
					Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
					*/
					const FVector NewVelocity = (Delta / subTimeTickRemaining);

					if (!HasAnimRootMotion())
					{
						Velocity = NewVelocity;
					}
					else
					{
						Velocity = FVector::VectorPlaneProject(Velocity, GravityDir) + GravityDir * (NewVelocity | GravityDir);
					}
				}

				if (subTimeTickRemaining > KINDA_SMALL_NUMBER && (Delta | Adjusted) > 0.f)
				{
					// Move in deflected direction.
					SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);

					if (Hit.bBlockingHit)
					{
						// hit second wall
						LastMoveTimeSlice = subTimeTickRemaining;
						subTimeTickRemaining = subTimeTickRemaining * (1.f - Hit.Time);

						if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
						{
							remainingTime += subTimeTickRemaining;
							ProcessLanded(Hit, remainingTime, Iterations);
							return;
						}

						HandleImpact(Hit, LastMoveTimeSlice, Delta);

						// If we've changed physics mode, abort.
						if (!HasValidData() || !IsFalling())
						{
							return;
						}

						// Act as if there was no air control on the last move when computing new deflection.
						if (bHasLimitedAirControl && (Hit.Normal | GravityDir) > VERTICAL_SLOPE_NORMAL_Z)
						{
							const FVector LastMoveNoAirControl = VelocityNoAirControl * LastMoveTimeSlice;
							Delta = ComputeSlideVector(LastMoveNoAirControl, 1.f, OldHitNormal, Hit);
						}

						FVector PreTwoWallDelta = Delta;
						TwoWallAdjust(Delta, Hit, OldHitNormal);

						// Limit air control, but allow a slide along the second wall.
						if (bHasLimitedAirControl)
						{
							const bool bCheckLandingSpot = false; // we already checked above.
							const FVector AirControlDeltaV = LimitAirControl(subTimeTickRemaining, AirControlAccel, Hit, bCheckLandingSpot) * subTimeTickRemaining;

							// Only allow if not back in to first wall
							if (FVector::DotProduct(AirControlDeltaV, OldHitNormal) > 0.f)
							{
								Delta += (AirControlDeltaV * subTimeTickRemaining);
							}
						}

						// Compute velocity after deflection (only gravity component for RootMotion)
						if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
						{
							/* @CIRCUIT - removed
							const FVector NewVelocity = (Delta / subTimeTickRemaining);
							Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
							*/
							const FVector NewVelocity = (Delta / subTimeTickRemaining);

							if (!HasAnimRootMotion())
							{
								Velocity = NewVelocity;
							}
							else
							{
								Velocity = FVector::VectorPlaneProject(Velocity, GravityDir) + GravityDir * (NewVelocity | GravityDir);
							}
						}

						// bDitch=true means that pawn is straddling two slopes, neither of which he can stand on
						//bool bDitch = ((OldHitImpactNormal.Z > 0.f) && (Hit.ImpactNormal.Z > 0.f) && (FMath::Abs(Delta.Z) <= KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));
												// bDitch=true means that pawn is straddling two slopes, neither of which he can stand on.
						// @CIRCUIT - removed and added
						bool bDitch = ((OldHitImpactNormal | GravityDir) < 0.0f && (Hit.ImpactNormal | GravityDir) < 0.0f &&
							FMath::Abs(Delta | GravityDir) <= KINDA_SMALL_NUMBER && (Hit.ImpactNormal | OldHitImpactNormal) < 0.0f);

						SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
						if (Hit.Time == 0.f)
						{
							// if we are stuck then try to side step
							// @CIRCUIT - changed
							FVector SideDelta = FVector::VectorPlaneProject(OldHitNormal + Hit.ImpactNormal, GravityDir).GetSafeNormal();
							if (SideDelta.IsNearlyZero())
							{
								// @CIRCUIT - changed
								SideDelta = GravityDir ^ (FVector::VectorPlaneProject(OldHitNormal, GravityDir).GetSafeNormal());
							}
							SafeMoveUpdatedComponent(SideDelta, PawnRotation, true, Hit);
						}

						if (bDitch || IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit) || Hit.Time == 0.f)
						{
							remainingTime = 0.f;
							ProcessLanded(Hit, remainingTime, Iterations);
							return;
						}
						else if (GetPerchRadiusThreshold() > 0.f && Hit.Time == 1.f && (OldHitImpactNormal | GravityDir) >= GetWalkableFloorZ()) // @CIRCUIT - changed
						{
							// We might be in a virtual 'ditch' within our perch radius. This is rare.
							// @CIRCUIT - removed
							//const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
							//const float ZMovedDist = FMath::Abs(PawnLocation.Z - OldLocation.Z);
							//const float MovedDist2DSq = (PawnLocation - OldLocation).SizeSquared2D();

							const FVector PawnLocation = CharacterOwner->GetActorLocation();
							const float ZMovedDist = FMath::Abs((PawnLocation - OldLocation) | GravityDir);
							const float MovedDist2DSq = (FVector::VectorPlaneProject(PawnLocation - OldLocation, GravityDir)).SizeSquared();

							if (ZMovedDist <= 0.2f * timeTick && MovedDist2DSq <= 4.f * timeTick)
							{
								Velocity.X += 0.25f * GetMaxSpeed() * (RandomStream.FRand() - 0.5f);
								Velocity.Y += 0.25f * GetMaxSpeed() * (RandomStream.FRand() - 0.5f);
								Velocity.Z += 0.25f * GetMaxSpeed() * (RandomStream.FRand() - 0.5f); // @CIRCUIT - alteration
								Velocity = FVector::VectorPlaneProject(Velocity, GetGravityDirection()) + GetGravityDirection() * (FMath::Max<float>(JumpZVelocity * 0.25f, 1.0f) * -1.0f);
								Delta = Velocity * timeTick;
								SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
							}
						}
					}
				}
			}
		}

		/* @CIRCUIT - Removal
		if (Velocity.SizeSquared2D() <= KINDA_SMALL_NUMBER * 10.f)
		{
			Velocity.X = 0.f;
			Velocity.Y = 0.f;
		}
		*/
		if ((FVector::VectorPlaneProject(Velocity, GetGravityDirection())).SizeSquared() <= KINDA_SMALL_NUMBER * 10.0f)
		{
			Velocity = GetGravityDirection() * (Velocity | GetGravityDirection());
		}
	}
}

void UCircuitCharacterMovement::PhysFlying(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	// Abort if no valid gravity can be obtained.
	const FVector GravityDir = GetGravityDirection();
	if (GravityDir.IsZero())
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		if (bCheatFlying && Acceleration.IsZero())
		{
			Velocity = FVector::ZeroVector;
		}
		const float Friction = 0.5f * GetPhysicsVolume()->FluidFriction;
		CalcVelocity(deltaTime, Friction, true, GetMaxBrakingDeceleration());
	}

	ApplyRootMotionToVelocity(deltaTime);

	Iterations++;
	bJustTeleported = false;

	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		const FVector GravDir = GetGravityDirection();
		const FVector VelDir = Velocity.GetSafeNormal();
		const float UpDown = GravDir | VelDir;

		bool bSteppedUp = false;
		if (UpDown < 0.5f && UpDown > -0.2f && FMath::Abs(Hit.ImpactNormal | GravityDir) < 0.2f && CanStepUp(Hit))
		{
			float stepZ = UpdatedComponent->GetComponentLocation().Z;
			bSteppedUp = StepUp(GravDir, Adjusted * (1.f - Hit.Time), Hit, nullptr);
			if (bSteppedUp)
			{
				const FVector StepLocation = CharacterOwner->GetActorLocation();
				OldLocation += GravityDir * ((CharacterOwner->GetActorLocation() - StepLocation) | GravityDir);
			}
		}

		if (!bSteppedUp)
		{
			//adjust and try again
			HandleImpact(Hit, deltaTime, Adjusted);
			SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
		}
	}

	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}
}

// DONE
void UCircuitCharacterMovement::PhysWalking(float deltaTime, int32 Iterations)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysWalking);
	//CSV_SCOPED_TIMING_STAT_EXCLUSIVE(CharPhysWalking);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		SetMovementMode(MOVE_Walking);
		return;
	}

	//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN before Iteration (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

	bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float remainingTime = deltaTime;

	// Perform the move
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity() || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)))
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Save current values
		UPrimitiveComponent* const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FFindFloorResult OldFloor = CurrentFloor;

		//RestorePreAdditiveRootMotionVelocity(); // @CIRCUIT - unsure kept in

		// Ensure velocity is horizontal.
		MaintainHorizontalGroundVelocity();
		const FVector OldVelocity = Velocity;
		//Acceleration.Z = 0.f; // @CIRCUIT - Removed

		// Apply acceleration
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			CalcVelocity(timeTick, GroundFriction, false, GetMaxBrakingDeceleration());
			//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
		}

		ApplyRootMotionToVelocity(timeTick); // @CIRCUIT - unsure kept in
		//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after Root Motion application (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

		// @CIRCUIT - unsure kept in
		if (IsFalling())
		{
			// Root motion could have put us into Falling.
			// No movement has taken place this movement tick so we pass on full time/past iteration count
			StartNewPhysics(remainingTime + timeTick, Iterations - 1);
			return;
		}

		// Compute move parameters
		const FVector MoveVelocity = Velocity;
		const FVector Delta = timeTick * MoveVelocity;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;

		if (bZeroDelta)
		{
			remainingTime = 0.f;
		}
		else
		{
			// try to move forward
			MoveAlongFloor(MoveVelocity, timeTick, &StepDownResult);

			if (IsFalling())
			{
				// pawn decided to jump up
				const float DesiredDist = Delta.Size();
				if (DesiredDist > KINDA_SMALL_NUMBER)
				{
					const float ActualDist = FVector::VectorPlaneProject(CharacterOwner->GetActorLocation() - OldLocation, GetCapsuleAxisZ()).Size(); // @CIRCUIT - changed
					remainingTime += timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));
				}
				StartNewPhysics(remainingTime, Iterations);
				return;
			}
			else if (IsSwimming()) //just entered water
			{
				StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		// Update floor.
		// StepUp might have already done it for us.
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, NULL);
		}

		// check for ledges here
		const bool bCheckLedges = !CanWalkOffLedges();
		if (bCheckLedges && !CurrentFloor.IsWalkableFloor())
		{
			// calculate possible alternate movement
			const FVector GravDir = GetGravityDirection();
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, GetCapsuleAxisZ() * -1.0f); // @CIRCUIT - changed
			if (!NewDelta.IsZero())
			{
				// first revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

				// avoid repeated ledge moves if the first one fails
				bTriedLedgeMove = true;

				// Try new movement direction
				Velocity = NewDelta / timeTick;
				remainingTime += timeTick;
				continue;
			}
			else
			{
				// see if it is OK to jump
				// @todo collision : only thing that can be problem is that oldbase has world collision on
				bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;

				// revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				remainingTime = 0.f;
				break;
			}
		}
		else
		{
			// Validate the floor check
			if (CurrentFloor.IsWalkableFloor())
			{
				if (ShouldCatchAir(OldFloor, CurrentFloor))
				{
					// @CIRCUIT - Removed
					//HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
					if (IsMovingOnGround())
					{
						// If still walking, then fall. If not, assume the user set a different mode they want to keep.
						StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation);
					}
					return;
				}

				AdjustFloorHeight();
				SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
			}
			else if (CurrentFloor.HitResult.bStartPenetrating && remainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration
				// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
				FHitResult Hit(CurrentFloor.HitResult);
				Hit.TraceEnd = Hit.TraceStart + GetCapsuleAxisZ() * MAX_FLOOR_DIST; // @CIRCUIT - changed
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				bForceNextFloorCheck = true;
			}

			// check if just entered water
			if (IsSwimming())
			{
				StartSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}

			// See if we need to start falling.
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				const bool bMustJump = bJustTeleported || bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;
			}
		}


		// Allow overlap events and such to change physics state and velocity
		if (IsMovingOnGround())
		{
			// Make velocity reflect actual move
			if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && timeTick >= MIN_TICK_TIME)
			{
				// TODO-RootMotionSource: Allow this to happen during partial override Velocity, but only set allowed axes?
				Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick;
				//MaintainHorizontalGroundVelocity(); // @CIRCUIT - removed
			}
		}

		// If we didn't move at all this iteration then abort (since future iterations will also be stuck).
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
	}

	if (IsMovingOnGround())
	{
		MaintainHorizontalGroundVelocity();
	}
}

// DIDNT CHANGE
void UCircuitCharacterMovement::SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode)
{
	if (NewMovementMode != MOVE_Custom)
	{
		NewCustomMode = 0;
	}

	// If trying to use NavWalking but there is no navmesh, use walking instead.
	if (NewMovementMode == MOVE_NavWalking)
	{
		if (GetNavData() == nullptr)
		{
			NewMovementMode = MOVE_Walking;
		}
	}

	// Do nothing if nothing is changing.
	if (MovementMode == NewMovementMode)
	{
		// Allow changes in custom sub-mode.
		if ((NewMovementMode != MOVE_Custom) || (NewCustomMode == CustomMovementMode))
		{
			return;
		}
	}

	const EMovementMode PrevMovementMode = MovementMode;
	const uint8 PrevCustomMode = CustomMovementMode;

	MovementMode = NewMovementMode;
	CustomMovementMode = NewCustomMode;

	// We allow setting movement mode before we have a component to update, in case this happens at startup.
	if (!HasValidData())
	{
		return;
	}

	// Handle change in movement mode
	OnMovementModeChanged(PrevMovementMode, PrevCustomMode);

	// @todo do we need to disable ragdoll physics here? Should this function do nothing if in ragdoll?
}

// DONE
void UCircuitCharacterMovement::SetPostLandedPhysics(const FHitResult& Hit)
{
	if (CharacterOwner)
	{
		if (CanEverSwim() && IsInWater())
		{
			SetMovementMode(MOVE_Swimming);
		}
		else
		{
			const FVector PreImpactAccel = Acceleration + (IsFalling() ? GetGravityScaled() : FVector::ZeroVector);
			const FVector PreImpactVelocity = Velocity;

			if (DefaultLandMovementMode == MOVE_Walking ||
				DefaultLandMovementMode == MOVE_NavWalking ||
				DefaultLandMovementMode == MOVE_Falling)
			{
				SetMovementMode(GetGroundMovementMode());
			}
			else
			{
				SetDefaultMovementMode();
			}

			ApplyImpactPhysicsForces(Hit, PreImpactAccel, PreImpactVelocity);
		}
	}
}

bool UCircuitCharacterMovement::ShouldCheckForValidLandingSpot(float DeltaTime, const FVector& Delta, const FHitResult& Hit) const
{
	/* @CIRCUIT - removed
	// See if we hit an edge of a surface on the lower portion of the capsule.
	// In this case the normal will not equal the impact normal, and a downward sweep may find a walkable surface on top of the edge.
	if (Hit.Normal.Z > KINDA_SMALL_NUMBER && !Hit.Normal.Equals(Hit.ImpactNormal))
	{
		const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
		if (IsWithinEdgeTolerance(PawnLocation, Hit.ImpactPoint, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius()))
		{
			return true;
		}
	}
	*/
	const FVector CapsuleUp = GetCapsuleAxisZ();

	// See if we hit an edge of a surface on the lower portion of the capsule.
	// In this case the normal will not equal the impact normal, and a downward sweep may find a walkable surface on top of the edge.
	if ((Hit.Normal | CapsuleUp) > KINDA_SMALL_NUMBER && !Hit.Normal.Equals(Hit.ImpactNormal) &&
		IsWithinEdgeTolerance(UpdatedComponent->GetComponentLocation(), Hit.ImpactPoint, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius()))
	{
		return true;
	}

	return false;
}

// DONE
bool UCircuitCharacterMovement::ShouldComputePerchResult(const FHitResult& InHit, bool bCheckRadius) const
{
	if (!InHit.IsValidBlockingHit())
	{
		return false;
	}

	// Don't try to perch if the edge radius is very small.
	if (GetPerchRadiusThreshold() <= SWEEP_EDGE_REJECT_DISTANCE)
	{
		return false;
	}

	if (bCheckRadius)
	{
		/* @CIRCUIT - removed
		const float DistFromCenterSq = (InHit.ImpactPoint - InHit.Location).SizeSquared2D();
		const float StandOnEdgeRadius = GetValidPerchRadius();
		if (DistFromCenterSq <= FMath::Square(StandOnEdgeRadius))
		{
			// Already within perch radius.
			return false;
		}
		*/
		const FVector CapsuleDown = GetCapsuleAxisZ() * -1.0f;
		const float DistFromCenterSq = (InHit.Location + CapsuleDown * ((InHit.ImpactPoint - InHit.Location) | CapsuleDown) - InHit.ImpactPoint).SizeSquared();
		const float StandOnEdgeRadiusSq = FMath::Square(GetValidPerchRadius());

		if (DistFromCenterSq <= StandOnEdgeRadiusSq)
		{
			// Already within perch radius.
			return false;
		}
	}

	return true;
}

float UCircuitCharacterMovement::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact)
{
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	FVector Normal(InNormal);
	FVector NewNormal = InNormal;
	if (IsMovingOnGround())
	{
		// @CIRCUIT - addition
		const FVector CapsuleUp = GetCapsuleAxisZ();
		const float Dot = NewNormal | CapsuleUp;
		// We don't want to be pushed up an unwalkable surface.
		if (Dot > 0.f)
		{
			if (!IsWalkable(Hit))
			{
				Normal = FVector::VectorPlaneProject(NewNormal, CapsuleUp).GetSafeNormal();
			}
		}
		else if (Dot < -KINDA_SMALL_NUMBER)
		{
			/* @CIRCUIT - removal
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				const FVector FloorNormal = CurrentFloor.HitResult.Normal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - DELTA);
				if (bFloorOpposedToMovement)
				{
					Normal = FloorNormal;
				}

				Normal = Normal.GetSafeNormal2D();
			}
			*/
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				const FVector FloorNormal = CurrentFloor.HitResult.Normal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.0f && (FloorNormal | CapsuleUp) < 1.0f - DELTA;
				if (bFloorOpposedToMovement)
				{
					NewNormal = FloorNormal;
				}

				NewNormal = FVector::VectorPlaneProject(NewNormal, CapsuleUp).GetSafeNormal();
			}
		}
	}

	return Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}

// DONE
void UCircuitCharacterMovement::SimulateMovement(float DeltaSeconds)
{
	if (!HasValidData() || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);

	const FRepMovement& ConstRepMovement = CharacterOwner->GetReplicatedMovement();

	// Workaround for replication not being updated initially
	if (bIsSimulatedProxy &&
		ConstRepMovement.Location.IsZero() &&
		ConstRepMovement.Rotation.IsZero() &&
		ConstRepMovement.LinearVelocity.IsZero())
	{
		return;
	}

	// If base is not resolved on the client, we should not try to simulate at all
	if (CharacterOwner->GetReplicatedBasedMovement().IsBaseUnresolved())
	{
		//UE_LOG(LogCharacterMovement, Verbose, TEXT("Base for simulated character '%s' is not resolved on client, skipping SimulateMovement"), *CharacterOwner->GetName());
		return;
	}

	UpdateComponentRotation(); // @CIRCUIT - addition

	FVector OldVelocity;
	FVector OldLocation;

	// Scoped updates can improve performance of multiple MoveComponent calls.
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

		bool bHandledNetUpdate = false;
		if (bIsSimulatedProxy)
		{
			// Handle network changes
			if (bNetworkUpdateReceived)
			{
				bNetworkUpdateReceived = false;
				bHandledNetUpdate = true;
				//UE_LOG(LogCharacterMovement, Verbose, TEXT("Proxy %s received net update"), *CharacterOwner->GetName());
				if (bNetworkMovementModeChanged)
				{
					ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());
					bNetworkMovementModeChanged = false;
				}
				else if (bJustTeleported || bForceNextFloorCheck)
				{
					// Make sure floor is current. We will continue using the replicated base, if there was one.
					bJustTeleported = false;
					UpdateFloorFromAdjustment();
				}
			}
			/* @CIRCUIT - unsure removed
			else if (bForceNextFloorCheck)
			{
				UpdateFloorFromAdjustment();
			}
			*/
		}

		// @CIRCUIT - unsure
		UpdateCharacterStateBeforeMovement(DeltaSeconds);

		if (MovementMode != MOVE_None)
		{
			//TODO: Also ApplyAccumulatedForces()?
			HandlePendingLaunch();
		}
		ClearAccumulatedForces();

		if (MovementMode == MOVE_None)
		{
			return;
		}

		/* @CIRCUIT - unsure removed
		const bool bSimGravityDisabled = (bIsSimulatedProxy && CharacterOwner->bSimGravityDisabled);
		const bool bZeroReplicatedGroundVelocity = (bIsSimulatedProxy && IsMovingOnGround() && ConstRepMovement.LinearVelocity.IsZero());

		// bSimGravityDisabled means velocity was zero when replicated and we were stuck in something. Avoid external changes in velocity as well.
		// Being in ground movement with zero velocity, we cannot simulate proxy velocities safely because we might not get any further updates from the server.
		if (bSimGravityDisabled || bZeroReplicatedGroundVelocity)
		{
			Velocity = FVector::ZeroVector;
		}
		*/
		MaybeUpdateBasedMovement(DeltaSeconds);

		// simulated pawns predict location
		OldVelocity = Velocity;
		OldLocation = UpdatedComponent->GetComponentLocation();

		UpdateProxyAcceleration();

		// Consume path following requested velocity.
		bHasRequestedVelocity = false;

		// May only need to simulate forward on frames where we haven't just received a new position update.
		if (!bHandledNetUpdate /* || !bNetworkSkipProxyPredictionOnNetUpdate || !CharacterMovementCVars::NetEnableSkipProxyPredictionOnNetUpdate*/ )
		{
			//UE_LOG(LogCharacterMovement, Verbose, TEXT("Proxy %s simulating movement"), *GetNameSafe(CharacterOwner));
			FStepDownResult StepDownResult;
			MoveSmooth(Velocity, DeltaSeconds, &StepDownResult);

			// find floor and check if falling
			if (IsMovingOnGround() || MovementMode == MOVE_Falling)
			{
				const FVector Gravity = GetGravityScaled();

				if (StepDownResult.bComputedFloor)
				{
					CurrentFloor = StepDownResult.FloorResult;
				}
				else if (!Gravity.IsZero() && (Velocity | Gravity) >= 0.0f)
				{
					FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, Velocity.IsZero(), NULL);
				}
				else
				{
					CurrentFloor.Clear();
				}

				if (!CurrentFloor.IsWalkableFloor())
				{
					/*
					if (!bSimGravityDisabled)
					{*/
						// No floor, must fall.
						/* @CIRCUIT - removed
						if (Velocity.Z <= 0.f || bApplyGravityWhileJumping || !CharacterOwner->IsJumpProvidingForce())
						{
							Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaSeconds);
						}
						*/
					/* }*/
					Velocity = NewFallVelocity(Velocity, Gravity, DeltaSeconds);
					SetMovementMode(MOVE_Falling);
				}
				else
				{
					// Walkable floor
					if (IsMovingOnGround())
					{
						AdjustFloorHeight();
						SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
					}
					else if (MovementMode == MOVE_Falling)
					{
						if (CurrentFloor.FloorDist <= MIN_FLOOR_DIST) // @CIRCUIT - alteration
						{
							// Landed
							//SetPostLandedPhysics(CurrentFloor.HitResult); // @CIRCUIT - removed
							SetMovementMode(MOVE_Walking);
						}
						else
						{
							/* @CIRCUIT - removed
							if (!bSimGravityDisabled)
							{
								// Continue falling.
								Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaSeconds);
							}
							*/
							Velocity = NewFallVelocity(Velocity, Gravity, DeltaSeconds);
							CurrentFloor.Clear();
						}
					}
				}
			}
		}
		else
		{
			//UE_LOG(LogCharacterMovement, Verbose, TEXT("Proxy %s SKIPPING simulate movement"), *GetNameSafe(CharacterOwner));
		}

		UpdateCharacterStateAfterMovement(DeltaSeconds);// @CIRCUIT - unsure kept in

		// consume path following requested velocity
		bHasRequestedVelocity = false;// @CIRCUIT - unsure kept in

		OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	} // End scoped movement update

	// Call custom post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);

	SaveBaseLocation();
	UpdateComponentVelocity();
	bJustTeleported = false;

	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity; // @CIRCUIT - unsure kept in
	LastUpdateVelocity = Velocity;// @CIRCUIT - unsure kept in
}

// KINDA DONE
void UCircuitCharacterMovement::StartFalling(int32 Iterations, float remainingTime, float timeTick, const FVector& Delta, const FVector& subLoc)
{
	// start falling 
	const float DesiredDist = Delta.Size();
	const float ActualDist = (UpdatedComponent->GetComponentLocation() - subLoc).Size2D();
	remainingTime = (DesiredDist < KINDA_SMALL_NUMBER)
		? 0.f
		: remainingTime + timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));

	// @CIRCUIT - addition
	const FVector GravityDir = GetGravityDirection();
	if (!GravityDir.IsZero())
	{
		Velocity = FVector::VectorPlaneProject(Velocity, GravityDir);
	}

	if (IsMovingOnGround())
	{
		// This is to catch cases where the first frame of PIE is executed, and the
		// level is not yet visible. In those cases, the player will fall out of the
		// world... So, don't set MOVE_Falling straight away.
		if (!GIsEditor || (GetWorld()->HasBegunPlay() && (GetWorld()->GetTimeSeconds() >= 1.f)))
		{
			SetMovementMode(MOVE_Falling); //default behavior if script didn't change physics
		}
		else
		{
			// Make sure that the floor check code continues processing during this delay.
			bForceNextFloorCheck = true;
		}
	}
	StartNewPhysics(remainingTime, Iterations);
}

// DONE
bool UCircuitCharacterMovement::StepUp(const FVector& GravDir, const FVector& Delta, const FHitResult& InHit, struct UCharacterMovementComponent::FStepDownResult* OutStepDownResult)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharStepUp);

	if (!CanStepUp(InHit) || MaxStepHeight <= 0.f)
	{
		return false;
	}

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	const FVector CapsuleDown = GetCapsuleAxisZ() * -1.0f;

	// Get the axis of the capsule bounded by the following two end points.
	const FVector BottomPoint = OldLocation + CapsuleDown * PawnHalfHeight;
	const FVector TopPoint = OldLocation - CapsuleDown * FMath::Max(0.0f, PawnHalfHeight - PawnRadius);
	const FVector Segment = TopPoint - BottomPoint;

	// Project the impact point on the segment.
	const float Alpha = ((InHit.ImpactPoint - BottomPoint) | Segment) / Segment.SizeSquared();

	// @CIRCUIT - alterations
	// Don't bother stepping up if top of capsule is hitting something or if the impact is below us.
	if (Alpha > 1.0f || Alpha <= 0.0f)
	{
		return false;
	}

	if (GravDir.IsZero())
	{
		return false;
	}

	// Gravity should be a normalized direction
	ensure(GravDir.IsNormalized());

	float StepTravelUpHeight = (InHit.ImpactNormal | GravDir) * -1.0f;
	float StepTravelDownHeight = StepTravelUpHeight;
	//const float StepSideZ = -1.f * FVector::DotProduct(InHit.ImpactNormal, GravDir); // @CIRCUIT - removed
	FVector PawnInitialFloorBase = OldLocation + CapsuleDown * PawnHalfHeight;
	FVector PawnFloorPoint = PawnInitialFloorBase; // @CIRCUIT - alterations

	if (IsMovingOnGround() && CurrentFloor.IsWalkableFloor())
	{
		// Since we float a variable amount off the floor, we need to enforce max step height off the actual point of impact with the floor.
		const float FloorDist = FMath::Max(0.f, CurrentFloor.GetDistanceToFloor());
		PawnInitialFloorBase += CapsuleDown * FloorDist;// @CIRCUIT - alterations
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (MaxStepHeight + MAX_FLOOR_DIST * 2.f);

		const bool bHitVerticalFace = !IsWithinEdgeTolerance(InHit.Location, InHit.ImpactPoint, PawnRadius);
		if (!CurrentFloor.bLineTrace && !bHitVerticalFace)
		{
			PawnFloorPoint = CurrentFloor.HitResult.ImpactPoint;// @CIRCUIT - alterations
		}
		else
		{
			// Base floor point is the base of the capsule moved down by how far we are hovering over the surface we are hitting.
			PawnFloorPoint += CapsuleDown * CurrentFloor.FloorDist;// @CIRCUIT - alterations
		}
	}

	// @CIRCUIT - removal

	// Scope our movement updates, and do not apply them until all intermediate moves are completed.
	FScopedMovementUpdate ScopedStepUpMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	// step up - treat as vertical wall
	FHitResult SweepUpHit(1.f);
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	MoveUpdatedComponent(GravDir * -StepTravelUpHeight, PawnRotation, true, &SweepUpHit); // @CIRCUIT - minor alteration

	/* @CIRCUIT - removed
	if (SweepUpHit.bStartPenetrating)
	{
		// Undo movement
		ScopedStepUpMovement.RevertMove();
		return false;
	}
	*/

	// step fwd
	FHitResult Hit(1.f);
	MoveUpdatedComponent(Delta, PawnRotation, true, &Hit);

	// Check result of forward movement
	if (Hit.bBlockingHit)
	{
		if (Hit.bStartPenetrating)
		{
			// Undo movement
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If we hit something above us and also something ahead of us, we should notify about the upward hit as well.
		// The forward hit will be handled later (in the bSteppedOver case below).
		// In the case of hitting something above but not forward, we are not blocked from moving so we don't need the notification.
		if (SweepUpHit.bBlockingHit && Hit.bBlockingHit)
		{
			HandleImpact(SweepUpHit);
		}

		// pawn ran into a wall
		HandleImpact(Hit);
		if (IsFalling())
		{
			return true;
		}

		// adjust and try again
		const float ForwardHitTime = Hit.Time;
		const float ForwardSlideAmount = SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);

		if (IsFalling())
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If both the forward hit and the deflection got us nowhere, there is no point in this step up.
		if (ForwardHitTime == 0.f && ForwardSlideAmount == 0.f)
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}
	}

	// Step down
	MoveUpdatedComponent(GravDir * StepTravelDownHeight, UpdatedComponent->GetComponentQuat(), true, &Hit);

	// If step down was initially penetrating abort the step up
	if (Hit.bStartPenetrating)
	{
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	FStepDownResult StepDownResult;
	if (Hit.IsValidBlockingHit())
	{
		// See if this step sequence would have allowed us to travel higher than our max step height allows.
		const float DeltaZ = (PawnFloorPoint - Hit.ImpactPoint) | CapsuleDown; // @CIRCUIT - alteration
		if (DeltaZ > MaxStepHeight)
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (too high Height %.3f) up from floor base %f to %f"), DeltaZ, PawnInitialFloorBaseZ, NewLocation.Z);
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Reject unwalkable surface normals here.
		if (!IsWalkable(Hit))
		{
			// Reject if normal opposes movement direction
			const bool bNormalTowardsMe = (Delta | Hit.ImpactNormal) < 0.f;
			if (bNormalTowardsMe)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s opposed to movement)"), *Hit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}

			// Also reject if we would end up being higher than our starting location by stepping down.
			// It's fine to step down onto an unwalkable normal below us, we will just slide off. Rejecting those moves would prevent us from being able to walk off the edge.
			if (((OldLocation - Hit.Location) | CapsuleDown) > 0.0f) // @CIRCUIT - alteration
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s above old position)"), *Hit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}
		}

		// Reject moves where the downward sweep hit something very close to the edge of the capsule. This maintains consistency with FindFloor as well.
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (outside edge tolerance)"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Don't step up onto invalid surfaces if traveling higher.
		if (DeltaZ > 0.f && !CanStepUp(Hit))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (up onto surface with !CanStepUp())"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// See if we can validate the floor as a result of this step down. In almost all cases this should succeed, and we can avoid computing the floor outside this method.
		if (OutStepDownResult != NULL)
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), StepDownResult.FloorResult, false, &Hit);

			// Reject unwalkable normals if we end up higher than our initial height.
			// It's fine to walk down onto an unwalkable surface, don't reject those moves.
			if (((OldLocation - Hit.Location) | CapsuleDown) > 0.0f) // @CIRCUIT - alteration
			{
				// We should reject the floor result if we are trying to step up an actual step where we are not able to perch (this is rare).
				// In those cases we should instead abort the step up and try to slide along the stair.
				/* @CIRCUIT - Removal
				if (!StepDownResult.FloorResult.bBlockingHit && StepSideZ < MAX_STEP_SIDE_Z)
				{
					ScopedStepUpMovement.RevertMove();
					return false;
				}
				*/
			}

			StepDownResult.bComputedFloor = true;
		}
	}

	// Copy step down result.
	if (OutStepDownResult != NULL)
	{
		*OutStepDownResult = StepDownResult;
	}

	// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
	bJustTeleported |= !bMaintainHorizontalGroundVelocity;

	return true;
}

// DONE
void UCircuitCharacterMovement::UpdateBasedMovement(float DeltaSeconds)
{
	if (!HasValidData())
	{
		return;
	}

	const UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
	if (!MovementBaseUtility::UseRelativeLocation(MovementBase))
	{
		return;
	}

	if (!IsValid(MovementBase) || !IsValid(MovementBase->GetOwner()))
	{
		SetBase(NULL);
		return;
	}

	// Ignore collision with bases during these movements.
	TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, MoveComponentFlags | MOVECOMP_IgnoreBases);

	FQuat DeltaQuat = FQuat::Identity;
	FVector DeltaPosition = FVector::ZeroVector;

	FQuat NewBaseQuat;
	FVector NewBaseLocation;
	if (!MovementBaseUtility::GetMovementBaseTransform(MovementBase, CharacterOwner->GetBasedMovement().BoneName, NewBaseLocation, NewBaseQuat))
	{
		return;
	}

	// Find change in rotation
	const bool bRotationChanged = !OldBaseQuat.Equals(NewBaseQuat, 1e-8f);
	if (bRotationChanged)
	{
		DeltaQuat = NewBaseQuat * OldBaseQuat.Inverse();
	}

	// only if base moved
	if (bRotationChanged || (OldBaseLocation != NewBaseLocation))
	{
		// Calculate new transform matrix of base actor (ignoring scale).
		const FQuatRotationTranslationMatrix OldLocalToWorld(OldBaseQuat, OldBaseLocation);
		const FQuatRotationTranslationMatrix NewLocalToWorld(NewBaseQuat, NewBaseLocation);

		if (CharacterOwner->IsMatineeControlled())
		{
			FRotationTranslationMatrix HardRelMatrix(CharacterOwner->GetBasedMovement().Rotation, CharacterOwner->GetBasedMovement().Location);
			const FMatrix NewWorldTM = HardRelMatrix * NewLocalToWorld;
			const FQuat NewWorldRot = bIgnoreBaseRotation ? UpdatedComponent->GetComponentQuat() : NewWorldTM.ToQuat();
			MoveUpdatedComponent(NewWorldTM.GetOrigin() - UpdatedComponent->GetComponentLocation(), NewWorldRot, true);
		}
		else
		{
			FQuat FinalQuat = UpdatedComponent->GetComponentQuat();

			if (bRotationChanged && !bIgnoreBaseRotation)
			{
				// Apply change in rotation and pipe through FaceRotation to maintain axis restrictions
				const FQuat PawnOldQuat = UpdatedComponent->GetComponentQuat();
				const FQuat TargetQuat = DeltaQuat * FinalQuat;
				FRotator TargetRotator(TargetQuat);
				CharacterOwner->FaceRotation(TargetRotator, 0.f);
				FinalQuat = UpdatedComponent->GetComponentQuat();

				/* @CIRCUIT - removed
				if (PawnOldQuat.Equals(FinalQuat, 1e-6f))
				{
					// Nothing changed. This means we probably are using another rotation mechanism (bOrientToMovement etc). We should still follow the base object.
					// @todo: This assumes only Yaw is used, currently a valid assumption. This is the only reason FaceRotation() is used above really, aside from being a virtual hook.
					if (bOrientRotationToMovement || (bUseControllerDesiredRotation && CharacterOwner->Controller))
					{
						TargetRotator.Pitch = 0.f;
						TargetRotator.Roll = 0.f;
						MoveUpdatedComponent(FVector::ZeroVector, TargetRotator, false);
						FinalQuat = UpdatedComponent->GetComponentQuat();
					}
				}
				*/
				// Pipe through ControlRotation, to affect camera.
				if (CharacterOwner->Controller)
				{
					const FQuat PawnDeltaRotation = FinalQuat * PawnOldQuat.Inverse();
					FRotator FinalRotation = FinalQuat.Rotator();
					UpdateBasedRotation(FinalRotation, PawnDeltaRotation.Rotator());
					FinalQuat = UpdatedComponent->GetComponentQuat();
				}
			}

			// We need to offset the base of the character here, not its origin, so offset by half height
			float HalfHeight, Radius;
			CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);

			FVector const BaseOffset = GetCapsuleAxisZ() * HalfHeight;
			FVector const LocalBasePos = OldLocalToWorld.InverseTransformPosition(UpdatedComponent->GetComponentLocation() - BaseOffset);
			FVector const NewWorldPos = ConstrainLocationToPlane(NewLocalToWorld.TransformPosition(LocalBasePos) + BaseOffset);
			DeltaPosition = ConstrainDirectionToPlane(NewWorldPos - UpdatedComponent->GetComponentLocation());

			// move attached actor
			if (bFastAttachedMove)
			{
				// we're trusting no other obstacle can prevent the move here
				UpdatedComponent->SetWorldLocationAndRotation(NewWorldPos, FinalQuat, false);
			}
			else
			{
				// @CIRCUIT - added
				FHitResult MoveOnBaseHit(1.0f);
				const FVector OldLocation = UpdatedComponent->GetComponentLocation();
				MoveUpdatedComponent(DeltaPosition, FinalQuat.Rotator(), true, &MoveOnBaseHit);
				if (!((UpdatedComponent->GetComponentLocation() - (OldLocation + DeltaPosition)).IsNearlyZero()))
				{
					OnUnableToFollowBaseMove(DeltaPosition, OldLocation, MoveOnBaseHit);
				}

				/* @CIRCUIT - removed
				// hack - transforms between local and world space introducing slight error FIXMESTEVE - discuss with engine team: just skip the transforms if no rotation?
				FVector BaseMoveDelta = NewBaseLocation - OldBaseLocation;
				if (!bRotationChanged && (BaseMoveDelta.X == 0.f) && (BaseMoveDelta.Y == 0.f))
				{
					DeltaPosition.X = 0.f;
					DeltaPosition.Y = 0.f;
				}

				FHitResult MoveOnBaseHit(1.f);
				const FVector OldLocation = UpdatedComponent->GetComponentLocation();
				MoveUpdatedComponent(DeltaPosition, FinalQuat, true, &MoveOnBaseHit);
				if ((UpdatedComponent->GetComponentLocation() - (OldLocation + DeltaPosition)).IsNearlyZero() == false)
				{
					OnUnableToFollowBaseMove(DeltaPosition, OldLocation, MoveOnBaseHit);
				}
				*/
			}
		}

		if (MovementBase->IsSimulatingPhysics() && CharacterOwner->GetMesh())
		{
			CharacterOwner->GetMesh()->ApplyDeltaToAllPhysicsTransforms(DeltaPosition, DeltaQuat);
		}
	}
}

// DONE
void UCircuitCharacterMovement::UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation)
{
	AController* Controller = CharacterOwner ? CharacterOwner->Controller : NULL;
	float ControllerRoll = 0.0f;

	if (Controller && !bIgnoreBaseRotation)
	{
		const FRotator ControllerRot = Controller->GetControlRotation();
		ControllerRoll = ControllerRot.Roll;
		UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement SetControlRotation Roll %f"), GetWorld()->GetRealTimeSeconds(), ControllerRot.Roll);
		Controller->SetControlRotation(ControllerRot + ReducedRotation);
	}

	if (true)
	{
		// Remove roll.
		//FinalRotation.Roll = 0.0f;
		if (Controller)
		{
			//FinalRotation.Roll = CharacterOwner->GetActorRotation().Roll;
			FRotator NewRotation = Controller->GetControlRotation();
			NewRotation.Roll = ControllerRoll;
			UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement SetControlRotation NewRotation Roll %f"), GetWorld()->GetRealTimeSeconds(), NewRotation.Roll);
			Controller->SetControlRotation(NewRotation);
		}
	}
}

// DONE
FQuat UCircuitCharacterMovement::GetCapsuleRotation() const
{
	return UpdatedComponent->GetComponentQuat();
}

// DONE
FVector UCircuitCharacterMovement::GetCapsuleAxisX() 
{
	// Fast simplification of FQuat::RotateVector() with FVector(1,0,0).
	const FQuat CapsuleRotation = GetCapsuleRotation();
	const FVector QuatVector(CapsuleRotation.X, CapsuleRotation.Y, CapsuleRotation.Z);

	return FVector(FMath::Square(CapsuleRotation.W) - QuatVector.SizeSquared(), CapsuleRotation.Z * CapsuleRotation.W * 2.0f,
		CapsuleRotation.Y * CapsuleRotation.W * -2.0f) + QuatVector * (CapsuleRotation.X * 2.0f);
}

// DONE
FVector UCircuitCharacterMovement::GetCapsuleAxisZ() const 
{
	// Fast simplification of FQuat::RotateVector() with FVector(0,0,1).
	const FQuat CapsuleRotation = GetCapsuleRotation();
	const FVector QuatVector(CapsuleRotation.X, CapsuleRotation.Y, CapsuleRotation.Z);

	return FVector(CapsuleRotation.Y * CapsuleRotation.W * 2.0f, CapsuleRotation.X * CapsuleRotation.W * -2.0f,
		FMath::Square(CapsuleRotation.W) - QuatVector.SizeSquared()) + QuatVector * (CapsuleRotation.Z * 2.0f);
}

// DONE
FVector UCircuitCharacterMovement::GetComponentDesiredAxisZ() 
{
	return GetGravityDirection() * -1.0f;
}

// DONE
FVector UCircuitCharacterMovement::GetGravityDirection() const 
{
	// Gravity direction can be influenced by the custom gravity scale value.
	if (GravityScale != 0.0f)
	{
		ACircuitCharacter* CircuitCharacter = Cast<ACircuitCharacter>(CharacterOwner);
		if (CircuitCharacter->GravityComponent && !CircuitCharacter->GravityComponent->GravitySettings.GravityDirection.IsZero()) {
			//UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement GetGravityDirection GravityComponent %f %f %f"), GetWorld()->GetRealTimeSeconds(), Cast<ACircuitCharacter>(CharacterOwner)->GravityComponent->GravitySettings.GravityDirection.X, Cast<ACircuitCharacter>(CharacterOwner)->GravityComponent->GravitySettings.GravityDirection.Y, Cast<ACircuitCharacter>(CharacterOwner)->GravityComponent->GravitySettings.GravityDirection.Z);
			return Cast<ACircuitCharacter>(CharacterOwner)->GravityComponent->GravitySettings.GravityDirection;
		}

		const float WorldGravityZ = Super::GetGravityZ();
		if (WorldGravityZ != 0.0f)
		{
			return FVector(0.0f, 0.0f, ((WorldGravityZ > 0.0f) ? 1.0f : -1.0f) * ((GravityScale > 0.0f) ? 1.0f : -1.0f));
		}
	}

	//UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement GetGravityDirection GravityComponent NULL"), GetWorld()->GetRealTimeSeconds());
	return FVector::ZeroVector;
}

// DONE
FVector UCircuitCharacterMovement::GetGravityScaled()
{
	if (Cast<ACircuitCharacter>(CharacterOwner)->GravityComponent && !Cast<ACircuitCharacter>(CharacterOwner)->GravityComponent->GravitySettings.GravityDirection.IsZero()) {
		return GetGravityDirection() * Cast<ACircuitCharacter>(CharacterOwner)->GravityComponent->GravitySettings.GravityPower; /* (FMath::Abs(Super::GetGravityZ()));*/
	}

	return FVector(0.0f, 0.0f, GetGravityZ());
}

// DONE
// Version that does not use inverse sqrt estimate, for higher precision.
FVector UCircuitCharacterMovement::GetSafeNormalPrecise(const FVector& V)
{
	const float VSq = V.SizeSquared();
	if (VSq < SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}
	else
	{
		return V * (1.f / FMath::Sqrt(VSq));
	}
}

// DONE
bool UCircuitCharacterMovement::IsWithinEdgeToleranceCircuit(const FVector& CapsuleLocation, const FVector& CapsuleDown, const FVector& TestImpactPoint, const float CapsuleRadius) const
{
	const float DistFromCenterSq = (CapsuleLocation + CapsuleDown * ((TestImpactPoint - CapsuleLocation) | CapsuleDown) - TestImpactPoint).SizeSquared();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(KINDA_SMALL_NUMBER, CapsuleRadius - SWEEP_EDGE_REJECT_DISTANCE));

	return DistFromCenterSq < ReducedRadiusSq;
}

// DONE
void UCircuitCharacterMovement::UpdateComponentRotation()
{
	if (!UpdatedComponent)
	{
		return;
	}

	const FVector DesiredCapsuleUp = GetComponentDesiredAxisZ();

	// Commented out due to issues with big plannet and knowing snapping issues

	// Abort if angle between new and old capsule 'up' axis almost equals to 0 degrees.
	//if ((DesiredCapsuleUp | GetCapsuleAxisZ()) >= THRESH_NORMALS_ARE_PARALLEL)
	//{
	//	return;
	//}

	// Take desired Z rotation axis of capsule, try to keep current X rotation axis of capsule.
	const FMatrix RotationMatrix = FRotationMatrix::MakeFromZX(DesiredCapsuleUp, GetCapsuleAxisX());

	// @TODO - add delta time, make it so that it scales with gravity strength
	FRotator InterpRotator = FMath::RInterpTo(UpdatedComponent->GetComponentRotation(), RotationMatrix.Rotator(), 0.075f, 1.0f);
	// Intentionally not using MoveUpdatedComponent to bypass constraints.
	UpdatedComponent->MoveComponent(FVector::ZeroVector, InterpRotator, true);

	/*
	AController* Controller = CharacterOwner ? CharacterOwner->Controller : NULL;
	float ControllerRoll = UpdatedComponent->GetComponentRotation().Roll;

	if (Controller)
	{
		//FinalRotation.Roll = CharacterOwner->GetActorRotation().Roll;
		FRotator NewRotation = Controller->GetControlRotation();
		NewRotation.Roll += ControllerRoll;
		//UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement SetControlRotation NewRotation Roll %f"), GetWorld()->GetRealTimeSeconds(), NewRotation.Roll);
		Controller->SetControlRotation(NewRotation);
	}
	*/
	//UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement UpdateComponentRotation %f %f %f"), GetWorld()->GetRealTimeSeconds(), UpdatedComponent->GetComponentRotation().Pitch, UpdatedComponent->GetComponentRotation().Yaw, UpdatedComponent->GetComponentRotation().Roll);
	//UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement SetControlRotation %f %f %f"), GetWorld()->GetRealTimeSeconds(), Controller->GetControlRotation().Pitch, Controller->GetControlRotation().Yaw, Controller->GetControlRotation().Roll);
}