// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Actors/CircuitActor.h"
#include "Circuit/Online/CircuitGameState.h" // Needed to get ServerSnapshotTime and ClientBufferTime
#include "Engine/NetworkObjectList.h"// Taken from CharacterMovementComponent.cpp - Needed for FNetworkObjectInfo in ClientPerformMovement
#include "Circuit/Player/CircuitCharacterMovement.h"

DECLARE_CYCLE_STAT(TEXT("Char Tick"), STAT_CharacterMovementTick, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char NonSimulated Time"), STAT_CharacterMovementNonSimulated, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char Physics Interation"), STAT_CharPhysicsInteraction, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char Update Acceleration"), STAT_CharUpdateAcceleration, STATGROUP_Character);

// MAGIC NUMBERS - Taken from CharacterMovementComponent.cpp
const float VERTICAL_SLOPE_NORMAL_Z = 0.001f; // Slope is vertical if Abs(Normal.Z) <= this threshold. Accounts for precision problems that sometimes angle normals slightly off horizontal for vertical surface.

// CVars
namespace CharacterMovementCVars2
{
	// Listen server smoothing
	static int32 NetEnableListenServerSmoothing2 = 1;
	FAutoConsoleVariableRef CVarNetEnableListenServerSmoothing2(
		TEXT("p.NetEnableListenServerSmoothing"),
		NetEnableListenServerSmoothing2,
		TEXT("Whether to enable mesh smoothing on listen servers for the local view of remote clients.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static int32 VisualizeMovement2 = 0;
	FAutoConsoleVariableRef CVarVisualizeMovement2(
		TEXT("p.VisualizeMovement"),
		VisualizeMovement2,
		TEXT("Whether to draw in-world debug information for character movement.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Cheat);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

/**
* BORROWED FROM CharacterMovementComponent.cpp
* Helper to change mesh bone updates within a scope.
* Example usage:
*	{
*		FScopedPreventMeshBoneUpdate ScopedNoMeshBoneUpdate(CharacterOwner->GetMesh(), EKinematicBonesUpdateToPhysics::SkipAllBones);
*		// Do something to move mesh, bones will not update
*	}
*	// Movement of mesh at this point will use previous setting.
*/
struct FScopedMeshBoneUpdateOverride
{
	FScopedMeshBoneUpdateOverride(USkeletalMeshComponent* Mesh, EKinematicBonesUpdateToPhysics::Type OverrideSetting)
		: MeshRef(Mesh)
	{
		if (MeshRef)
		{
			// Save current state.
			SavedUpdateSetting = MeshRef->KinematicBonesUpdateType;
			// Override bone update setting.
			MeshRef->KinematicBonesUpdateType = OverrideSetting;
		}
	}

	~FScopedMeshBoneUpdateOverride()
	{
		if (MeshRef)
		{
			// Restore bone update flag.
			MeshRef->KinematicBonesUpdateType = SavedUpdateSetting;
		}
	}

private:
	USkeletalMeshComponent* MeshRef;
	EKinematicBonesUpdateToPhysics::Type SavedUpdateSetting;
};

UCircuitCharacterMovement::UCircuitCharacterMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseFlatBaseForFloorChecks = true;
	bNetworkAlwaysReplicateTransformUpdateTimestamp = true; // Needed to that GetReplicatedServerTimeStamp() replicates value to simulated proxies
	NetworkSmoothingMode = ENetworkSmoothingMode::Disabled; // May fix jitter in non-controlled characters.
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PostPhysicsTickFunction.bTickEvenWhenPaused = true; // Needs to tick when paused too if being used.

	bWasFalling = false;

	CurrentMovementTime = 0.0f;
	InitialMovementTime = -20.0f;
}

// Called when the game starts or when spawned
void UCircuitCharacterMovement::BeginPlay()
{
	Super::BeginPlay();


	const ACircuitGameState* DefGame = Cast<ACircuitGameState>(GetWorld()->GetGameState());
	if (DefGame) {
		//UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement BeginPlay() GetGameState()"), GetWorld()->GetRealTimeSeconds());
		ClientBufferTime = DefGame->ClientBufferTime;
		ServerSnapshotTime = DefGame->ServerSnapshotTime;
		bUseCustomNetworking = DefGame->bUsesCustomNetworking;
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement BeginPlay() DefGame null"), GetWorld()->GetRealTimeSeconds());
	}
}

void UCircuitCharacterMovement::ClientAdjustPosition_Implementation
(
	float TimeStamp,
	FVector NewLocation,
	FVector NewVelocity,
	UPrimitiveComponent* NewBase,
	FName NewBaseBoneName,
	bool bHasBase,
	bool bBaseRelativePosition,
	uint8 ServerMovementMode
)
{
	if (!GetUsesCustomNetworking()) {
		return Super::ClientAdjustPosition_Implementation(TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);
	}
	
	if (!HasValidData() || !IsActive())
	{
		return;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	check(ClientData);

	// Make sure the base actor exists on this client.
	const bool bUnresolvedBase = bHasBase && (NewBase == NULL);
	if (bUnresolvedBase)
	{
		if (bBaseRelativePosition)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("ClientAdjustPosition_Implementation could not resolve the new relative movement base actor, ignoring server correction!"));
			return;
		}
		else
		{
			UE_LOG(LogNetPlayerMovement, Verbose, TEXT("ClientAdjustPosition_Implementation could not resolve the new absolute movement base actor, but WILL use the position!"));
		}
	}

	// Ack move if it has not expired.
	int32 MoveIndex = ClientData->GetSavedMoveIndex(TimeStamp);
	if (MoveIndex == INDEX_NONE)
	{
		if (ClientData->LastAckedMove.IsValid())
		{
			UE_LOG(LogNetPlayerMovement, Log, TEXT("ClientAdjustPosition_Implementation could not find Move for TimeStamp: %f, LastAckedTimeStamp: %f, CurrentTimeStamp: %f"), TimeStamp, ClientData->LastAckedMove->TimeStamp, ClientData->CurrentTimeStamp);
		}
		return;
	}
	ClientData->AckMove(MoveIndex, *this);

	//***************TODO - Add WorldShiftedNewLocation to buffer list. (Might have to consider if new position is relative to a base. May require adding "Base" as a variable in buffer.
	// Ideally when based on something new position will be relative. That way when based object moves it doesn't leave character behind. This also makes it so characters location doesn't need to be updated if merely standing on moving based object.
	AddToMovementBuffer(NewLocation, UpdatedComponent->GetComponentQuat(), NewVelocity, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode, TimeStamp, false);

	ClientData->bUpdatePosition = false;

	// @CIRCUIT - Everything after this in the function we are overriding is done later when client uses MovementBuffer data to move.

}
bool UCircuitCharacterMovement::ClientUpdatePositionAfterServerUpdate()
{
	if (!GetUsesCustomNetworking()) {
		return Super::ClientUpdatePositionAfterServerUpdate();
	}

	if (!HasValidData())
	{
		return false;
	}

	if (MovementBuffer.Num() == 0 || InitialMovementTime <= -10.0f) {
		return false;
	}
	
	/*
	// Still buffering
	if (CurrentMovementTime <= (InitialMovementTime + 0.015f)) {
		return false;
	}
	*/
	//FMath::VInterpConstantTo()

	float LerpPercent = 0.0f;
	const float LerpLimit = 1.15f;
	float ServerDelta = (MovementBuffer.Num() > 1) ? (MovementBuffer[1].TimeStamp - MovementBuffer[0].TimeStamp) : 0.0f;

	// Has at least 1 move to make
	if (ServerDelta > SMALL_NUMBER)
	{
		// Calculate lerp percent
		float RemainingTime = MovementBuffer[1].TimeStamp - (CurrentMovementTime); // We need to subtract ClientBufferTime otherwise CurrentMovementTime will be in real time, not in past buffered time.
		//UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement - ClientUpdatePositionAfterServerUpdate() MovementBuffer[1].TimeStamp: %f CurrentMovementTime %f GetClientBufferTime() %f"), GetWorld()->GetRealTimeSeconds(), MovementBuffer[1].TimeStamp, CurrentMovementTime, GetClientBufferTime());
		float CurrentSmoothTime = ServerDelta - RemainingTime;
		//UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement - ClientUpdatePositionAfterServerUpdate() CurrentSmoothTime: %f"), GetWorld()->GetRealTimeSeconds(), CurrentSmoothTime);
		LerpPercent = FMath::Clamp(CurrentSmoothTime / ServerDelta, 0.0f, LerpLimit);
		if (InitialMove) {
			LerpPercent *= 0.575f;
		}
	}
	else {

		if (ServerDelta < 0.0f) {
			UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement - ClientUpdatePositionAfterServerUpdate() ServerDelta too small: %f"), GetWorld()->GetRealTimeSeconds(), ServerDelta);
		}
		// No more moves to make
		LerpPercent = 1.0f;

		UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement - ClientUpdatePositionAfterServerUpdate() No moves to make: %f"), GetWorld()->GetRealTimeSeconds(), ServerDelta);
	}

	//UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement - ClientUpdatePositionAfterServerUpdate() BufferNum: %d"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num());

	// LerpPercent is too high, need to move buffer forward and recalculate lerppercent based on new movement point
	if (LerpPercent >= (1.0f - KINDA_SMALL_NUMBER)) {
		InitialMove = false;
		MovementBuffer.RemoveAt(0);
		if (MovementBuffer.Num() > 1) {
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
				//UE_LOG(LogTemp, Error, TEXT("[%f] PerdixActor - Tick() Second LerpPercent calculation is too large: %f"), GetWorld()->GetRealTimeSeconds(), LerpPercent);
			}

			//UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement - ClientUpdatePositionAfterServerUpdate() MoveClient 1 LerpPercent: %f"), GetWorld()->GetRealTimeSeconds(), LerpPercent);

			MoveClient(MovementBuffer[0], MovementBuffer[1], LerpPercent);
			return true;
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement - ClientUpdatePositionAfterServerUpdate() MovementBuffer %d"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num());
			if (MovementBuffer.Num() > 0) {
				InitialMovementTime = -20.0f; // Set to "still buffering" state
				MoveClient(MovementBuffer[0], MovementBuffer[0], LerpPercent);
				MovementBuffer.RemoveAt(0);

				// Needed otherwise on occasion character will retain velocity while stopped and animate forever.
				Velocity = FVector(0.0f, 0.0f, 0.0f);
				return true;
			}
		}
	}
	else {
		//UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement - ClientUpdatePositionAfterServerUpdate() MoveClient 2 LerpPercent: %f"), GetWorld()->GetRealTimeSeconds(), LerpPercent);

		MoveClient(MovementBuffer[0], MovementBuffer[1], LerpPercent);
		return true;
	}

	return false;
}

bool UCircuitCharacterMovement::DoJump(bool bReplayingMoves)
{
	if (!GetUsesCustomNetworking()) {
		return Super::DoJump(bReplayingMoves);
	}

	if (CharacterOwner && CharacterOwner->CanJump())
	{
		// Don't jump if we can't move up/down.
		if (!bConstrainToPlane || FMath::Abs(PlaneConstraintNormal.Z) != 1.f)
		{
			// @PERDIX - Don't try to jump if client. Client waits for server data.
			if (!GetIsClient()) {
				Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
				SetMovementMode(MOVE_Falling);
				return true;
			}
		}
	}

	return false;
}

void UCircuitCharacterMovement::MaybeUpdateBasedMovement(float DeltaSeconds)
{
	bDeferUpdateBasedMovement = false;

	//UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
	ACircuitActor* Base;
	UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
	if (MovementBase) {
		Base = Cast<ACircuitActor>(CharacterOwner->GetMovementBase()->GetOwner());
		if (Base)
		{
			// Fixes welded base jitter on clients
			PostPhysicsTickFunction.SetTickFunctionEnable(true);
			Base = Base->GetTopAttachedParent(Base);
			MovementBase = Cast<UPrimitiveComponent>(Base->GetRootComponent());
		}
	}

	if (MovementBaseUtility::UseRelativeLocation(MovementBase))
	{
		const bool bBaseIsSimulatingPhysics = MovementBase->IsSimulatingPhysics();

		// Temporarily disabling deferred tick on skeletal mesh components that sim physics.
		// We need to be consistent on when we read the bone locations for those, and while this reads
		// the wrong location, the relative changes (which is what we care about) will be accurate.
		const bool bAllowDefer = (bBaseIsSimulatingPhysics && !Cast<USkeletalMeshComponent>(MovementBase));

		if (!bBaseIsSimulatingPhysics || !bAllowDefer)
		{
			bDeferUpdateBasedMovement = false;
			UpdateBasedMovement(DeltaSeconds);

			// If previously simulated, go back to using normal tick dependencies.
			if (PostPhysicsTickFunction.IsTickFunctionEnabled())
			{
				PostPhysicsTickFunction.SetTickFunctionEnable(false);
				MovementBaseUtility::AddTickDependency(PrimaryComponentTick, MovementBase);
			}
		}
		else
		{
			// defer movement base update until after physics
			bDeferUpdateBasedMovement = true;
			// If previously not simulating, remove tick dependencies and use post physics tick function.
			if (!PostPhysicsTickFunction.IsTickFunctionEnabled())
			{
				PostPhysicsTickFunction.SetTickFunctionEnable(true);
				MovementBaseUtility::RemoveTickDependency(PrimaryComponentTick, MovementBase);
			}
		}
	}
}

void UCircuitCharacterMovement::PerformMovement(float DeltaSeconds)
{
	if (!GetUsesCustomNetworking()) {
		return Super::PerformMovement(DeltaSeconds);
	}
	else {
		// Only server performs movement, using what client sent.
		if (!GetIsClient()) {
			Super::PerformMovement(DeltaSeconds);
			return;
		}
		else {
			ClientPerformMovement(DeltaSeconds);
		}
	}
}

void UCircuitCharacterMovement::PhysFalling(float deltaTime, int32 Iterations)
{
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysFalling);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	FVector FallAcceleration = GetFallingLateralAcceleration(deltaTime);
	FallAcceleration.Z = 0.f;
	const bool bHasAirControl = (FallAcceleration.SizeSquared2D() > 0.f);

	float remainingTime = deltaTime;
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		Iterations++;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
		bJustTeleported = false;

		RestorePreAdditiveRootMotionVelocity();

		FVector OldVelocity = Velocity;
		FVector VelocityNoAirControl = Velocity;

		// Apply input
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			const float MaxDecel = GetMaxBrakingDeceleration();
			// Compute VelocityNoAirControl
			if (bHasAirControl)
			{
				// Find velocity *without* acceleration.
				TGuardValue<FVector> RestoreAcceleration(Acceleration, FVector::ZeroVector);
				TGuardValue<FVector> RestoreVelocity(Velocity, Velocity);
				Velocity.Z = 0.f;
				CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
				VelocityNoAirControl = FVector(Velocity.X, Velocity.Y, OldVelocity.Z);
			}

			// Compute Velocity
			{
				// Acceleration = FallAcceleration for CalcVelocity(), but we restore it after using it.
				TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);
				Velocity.Z = 0.f;
				CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
				Velocity.Z = OldVelocity.Z;
			}

			// Just copy Velocity to VelocityNoAirControl if they are the same (ie no acceleration).
			if (!bHasAirControl)
			{
				VelocityNoAirControl = Velocity;
			}
		}

		// Apply gravity
		const FVector Gravity(0.f, 0.f, GetGravityZ());
		float GravityTime = timeTick;

		// If jump is providing force, gravity may be affected.
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
			}
		}

		Velocity = NewFallVelocity(Velocity, Gravity, GravityTime);
		VelocityNoAirControl = bHasAirControl ? NewFallVelocity(VelocityNoAirControl, Gravity, GravityTime) : Velocity;
		const FVector AirControlAccel = (Velocity - VelocityNoAirControl) / timeTick;

		ApplyRootMotionToVelocity(timeTick);

		if (bNotifyApex && (Velocity.Z <= 0.f))
		{
			// Just passed jump apex since now going down
			bNotifyApex = false;
			NotifyJumpApex();
		}


		// Move
		FHitResult Hit(1.f);
		FVector Adjusted = 0.5f * (OldVelocity + Velocity) * timeTick;
		SafeMoveUpdatedComponent(Adjusted, PawnRotation, true, Hit);

		if (!HasValidData())
		{
			return;
		}

		float LastMoveTimeSlice = timeTick;
		float subTimeTickRemaining = timeTick * (1.f - Hit.Time);

		if (IsSwimming()) //just entered water
		{
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

				// @PERDIX
				bWasFalling = true;
				if (Hit.Component != nullptr && Hit.GetComponent()->ComponentVelocity.Size() != 0.0f) {
					Velocity.X = Hit.GetComponent()->ComponentVelocity.X;
					Velocity.Y = Hit.GetComponent()->ComponentVelocity.Y;
					UpdateComponentVelocity();
				}

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
					FindFloor(PawnLocation, FloorResult, false);
					if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
					{
						remainingTime += subTimeTickRemaining;
						ProcessLanded(FloorResult.HitResult, remainingTime, Iterations);

						// @PERDIX
						bWasFalling = true;
						if (Hit.Component != nullptr && Hit.GetComponent()->ComponentVelocity.Size() != 0.0f) {
							Velocity.X = Hit.GetComponent()->ComponentVelocity.X;
							Velocity.Y = Hit.GetComponent()->ComponentVelocity.Y;
							UpdateComponentVelocity();
						}

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
				if (bHasAirControl)
				{
					const bool bCheckLandingSpot = false; // we already checked above.
					const FVector AirControlDeltaV = LimitAirControl(LastMoveTimeSlice, AirControlAccel, Hit, bCheckLandingSpot) * LastMoveTimeSlice;
					Adjusted = (VelocityNoAirControl + AirControlDeltaV) * LastMoveTimeSlice;
				}

				const FVector OldHitNormal = Hit.Normal;
				const FVector OldHitImpactNormal = Hit.ImpactNormal;
				FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, OldHitNormal, Hit);

				// Compute velocity after deflection (only gravity component for RootMotion)
				if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
				{
					const FVector NewVelocity = (Delta / subTimeTickRemaining);
					Velocity = HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
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
						if (bHasAirControl && Hit.Normal.Z > VERTICAL_SLOPE_NORMAL_Z)
						{
							const FVector LastMoveNoAirControl = VelocityNoAirControl * LastMoveTimeSlice;
							Delta = ComputeSlideVector(LastMoveNoAirControl, 1.f, OldHitNormal, Hit);
						}

						FVector PreTwoWallDelta = Delta;
						TwoWallAdjust(Delta, Hit, OldHitNormal);

						// Limit air control, but allow a slide along the second wall.
						if (bHasAirControl)
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
							const FVector NewVelocity = (Delta / subTimeTickRemaining);
							Velocity = HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
						}

						// bDitch=true means that pawn is straddling two slopes, neither of which he can stand on
						bool bDitch = ((OldHitImpactNormal.Z > 0.f) && (Hit.ImpactNormal.Z > 0.f) && (FMath::Abs(Delta.Z) <= KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));
						SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
						if (Hit.Time == 0.f)
						{
							// if we are stuck then try to side step
							FVector SideDelta = (OldHitNormal + Hit.ImpactNormal).GetSafeNormal2D();
							if (SideDelta.IsNearlyZero())
							{
								SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0).GetSafeNormal();
							}
							SafeMoveUpdatedComponent(SideDelta, PawnRotation, true, Hit);
						}

						if (bDitch || IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit) || Hit.Time == 0.f)
						{
							remainingTime = 0.f;
							ProcessLanded(Hit, remainingTime, Iterations);
							return;
						}
						else if (GetPerchRadiusThreshold() > 0.f && Hit.Time == 1.f && OldHitImpactNormal.Z >= GetWalkableFloorZ()) // PERDIX - Altered "WalkableFloorZ" to "GetWalkableFloorZ()"
						{
							// We might be in a virtual 'ditch' within our perch radius. This is rare.
							const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
							const float ZMovedDist = FMath::Abs(PawnLocation.Z - OldLocation.Z);
							const float MovedDist2DSq = (PawnLocation - OldLocation).SizeSquared2D();
							if (ZMovedDist <= 0.2f * timeTick && MovedDist2DSq <= 4.f * timeTick)
							{
								Velocity.X += 0.25f * GetMaxSpeed() * (FMath::FRand() - 0.5f);
								Velocity.Y += 0.25f * GetMaxSpeed() * (FMath::FRand() - 0.5f);
								Velocity.Z = FMath::Max<float>(JumpZVelocity * 0.25f, 1.f);
								Delta = Velocity * timeTick;
								SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
							}
						}
					}
				}
			}
		}

		if (Velocity.SizeSquared2D() <= KINDA_SMALL_NUMBER * 10.f)
		{
			Velocity.X = 0.f;
			Velocity.Y = 0.f;
		}
	}
}

void UCircuitCharacterMovement::PostPhysicsTickComponent(float DeltaTime, FCharacterMovementComponentPostPhysicsTickFunction& ThisTickFunction)
{
	if (!GetUsesCustomNetworking()) {
		return Super::PostPhysicsTickComponent(DeltaTime, ThisTickFunction);
	}
	
	if (bWasFalling) {
		bWasFalling = false;
		bDeferUpdateBasedMovement = false; //PERDIX - This allows us to skip UpdateBasedMovement, which was causing jitter when jumping and landing on a high speed object.

		//UpdateBasedMovement(DeltaTime); // PERDIX - Doesn't appear to do anything
		SaveBaseLocation(); // PERDIX - This fixes movementmode->falling to movementmode->walking jump jitter that will occur on moving objects.
	}

	Super::PostPhysicsTickComponent(DeltaTime, ThisTickFunction);
}

void UCircuitCharacterMovement::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (!GetUsesCustomNetworking()) {
		return Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}

	/** @CIRCUIT - All the following code is taken directly from TickComponent() with a couple modifications **/
	SCOPED_NAMED_EVENT(UCharacterMovementComponent_TickComponent, FColor::Yellow);
	//SCOPE_CYCLE_COUNTER(STAT_CharacterMovement);
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementTick);
	//CSV_SCOPED_TIMING_STAT(CharacterMovement, CharacterMovementTick);

	const FVector InputVector = ConsumeInputVector();
	if (!HasValidData() || ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	// @CIRCUIT - We don't want to call CharacterMovementComponent::TickComponent as it'd be redundant, but parents of it are fine.
	Super::Super::Super::TickComponent(DeltaTime, TickType, ThisTickFunction); // Super 1 = ShooterCharacterMovement, Super 2 = CharacterMovementComponent, Super 3 = MovementComponent

	// Super tick may destroy/invalidate CharacterOwner or UpdatedComponent, so we need to re-check.
	if (!HasValidData())
	{
		return;
	}

	// See if we fell out of the world.
	const bool bIsSimulatingPhysics = UpdatedComponent->IsSimulatingPhysics();
	if (CharacterOwner->GetLocalRole() == ROLE_Authority && (!bCheatFlying || bIsSimulatingPhysics) && !CharacterOwner->CheckStillInWorld())
	{
		return;
	}

	// We don't update if simulating physics (eg ragdolls).
	if (bIsSimulatingPhysics)
	{
		// Update camera to ensure client gets updates even when physics move him far away from point where simulation started
		if (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client))
		{
			APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
			APlayerCameraManager* PlayerCameraManager = (PC ? PC->PlayerCameraManager : NULL);
			if (PlayerCameraManager != NULL && PlayerCameraManager->bUseClientSideCameraUpdates)
			{
				PlayerCameraManager->bShouldSendClientSideCameraUpdate = true;
			}
		}

		ClearAccumulatedForces();
		return;
	}

	AvoidanceLockTimer -= DeltaTime;

	if (CharacterOwner->GetLocalRole() > ROLE_SimulatedProxy)
	{
		SCOPE_CYCLE_COUNTER(STAT_CharacterMovementNonSimulated);

		// If we are a client we might have received an update from the server.
		const bool bIsClient = (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client));
		// @CIRCUIT - Moved several lines below
		/*if (bIsClient)
		{
			ClientUpdatePositionAfterServerUpdate();
		}*/

		// Allow root motion to move characters that have no controller.
		if (CharacterOwner->IsLocallyControlled() || (!CharacterOwner->Controller && bRunPhysicsWithNoController) || (!CharacterOwner->Controller && CharacterOwner->IsPlayingRootMotion()))
		{
			{
				SCOPE_CYCLE_COUNTER(STAT_CharUpdateAcceleration);

				// We need to check the jump state before adjusting input acceleration, to minimize latency
				// and to make sure acceleration respects our potentially new falling state.
				CharacterOwner->CheckJumpInput(DeltaTime);

				// apply input to acceleration
				Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector));
				AnalogInputModifier = ComputeAnalogInputModifier();
			}

			if (CharacterOwner->GetLocalRole() == ROLE_Authority)
			{
				PerformMovement(DeltaTime);
			}
			else if (bIsClient)
			{
				ReplicateMoveToServer(DeltaTime, Acceleration);

				// @PERDIX - added from above. Don't want ClientUpdatePositionAfterServerUpdate() messing with sent replicated values.
				CurrentMovementTime += DeltaTime;
				ClientUpdatePositionAfterServerUpdate();
			}
		}
		else if (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy)
		{
			// Server ticking for remote client.
			// Between net updates from the client we need to update position if based on another object,
			// otherwise the object will move on intermediate frames and we won't follow it.
			MaybeUpdateBasedMovement(DeltaTime);
			MaybeSaveBaseLocation();

			// Smooth on listen server for local view of remote clients. We may receive updates at a rate different than our own tick rate.
			if (CharacterMovementCVars2::NetEnableListenServerSmoothing2 && !bNetworkSmoothingComplete && IsNetMode(NM_ListenServer))
			{
				SmoothClientPosition(DeltaTime);
			}
		}
	}
	else if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		if (bShrinkProxyCapsule)
		{
			AdjustProxyCapsuleSize();
		}
		// @CIRCUIT - Remove as we want to move simulatedproxys ourselves
		//SimulatedTick(DeltaTime);

		// @CIRCUIT Taken from SimulatedTick()
		if (CharacterOwner->IsReplicatingMovement())
		{
			//Taken from SimulateMovement()
			HandlePendingLaunch();
			ClearAccumulatedForces();

			Acceleration = Velocity.GetSafeNormal();	// Not currently used for simulated movement
			AnalogInputModifier = 1.0f;				// Not currently used for simulated movement

			MaybeUpdateBasedMovement(DeltaTime);

			MaybeSaveBaseLocation();
			UpdateComponentVelocity();
			bJustTeleported = false;

			LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
			LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
			LastUpdateVelocity = Velocity;

			// @PERDIX - Not taken from SimulateMovement()
			CurrentMovementTime += DeltaTime;
			ClientUpdatePositionAfterServerUpdate();
		}
	}

	if (bUseRVOAvoidance)
	{
		UpdateDefaultAvoidance();
	}

	if (bEnablePhysicsInteraction)
	{
		SCOPE_CYCLE_COUNTER(STAT_CharPhysicsInteraction);
		ApplyDownwardForce(DeltaTime);
		ApplyRepulsionForce(DeltaTime);
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bVisualizeMovement = CharacterMovementCVars2::VisualizeMovement2 > 0;
	if (bVisualizeMovement)
	{
		VisualizeMovement();
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UCircuitCharacterMovement::ReplicateMoveToServer(float DeltaTime, const FVector& NewAcceleration)
{
	if (!GetUsesCustomNetworking()) {
		return Super::ReplicateMoveToServer(DeltaTime, NewAcceleration);
	}

	//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementReplicateMoveToServer);
	check(CharacterOwner != NULL);

	// Can only start sending moves if our controllers are synced up over the network, otherwise we flood the reliable buffer.
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (PC && PC->AcknowledgedPawn != CharacterOwner)
	{
		return;
	}

	// Bail out if our character's controller doesn't have a Player. This may be the case when the local player
	// has switched to another controller, such as a debug camera controller.
	if (PC && PC->Player == nullptr)
	{
		return;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (!ClientData)
	{
		return;
	}

	// Update our delta time for physics simulation.
	DeltaTime = ClientData->UpdateTimeStampAndDeltaTime(DeltaTime, *CharacterOwner, *this);

	// Find the oldest (unacknowledged) important move (OldMove).
	// Don't include the last move because it may be combined with the next new move.
	// A saved move is interesting if it differs significantly from the last acknowledged move
	FSavedMovePtr OldMove = NULL;
	if (ClientData->LastAckedMove.IsValid())
	{
		const int32 NumSavedMoves = ClientData->SavedMoves.Num();
		for (int32 i = 0; i < NumSavedMoves - 1; i++)
		{
			const FSavedMovePtr& CurrentMove = ClientData->SavedMoves[i];
			if (CurrentMove->IsImportantMove(ClientData->LastAckedMove))
			{
				OldMove = CurrentMove;
				break;
			}
		}
	}

	// Get a SavedMove object to store the movement in.
	FSavedMovePtr NewMovePtr = ClientData->CreateSavedMove();
	FSavedMove_Character* const NewMove = NewMovePtr.Get();
	if (NewMove == nullptr)
	{
		return;
	}

	NewMove->SetMoveFor(CharacterOwner, DeltaTime, NewAcceleration, *ClientData);

	// see if the two moves could be combined
	// do not combine moves which have different TimeStamps (before and after reset).
	if (const FSavedMove_Character* PendingMove = ClientData->PendingMove.Get())
	{
		if (!PendingMove->bOldTimeStampBeforeReset && PendingMove->CanCombineWith(NewMovePtr, CharacterOwner, ClientData->MaxMoveDeltaTime * CharacterOwner->GetActorTimeDilation()))
		{
			//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementCombineNetMove);

			// Only combine and move back to the start location if we don't move back in to a spot that would make us collide with something new.
			const FVector OldStartLocation = PendingMove->GetRevertedLocation();
			if (!OverlapTest(OldStartLocation, PendingMove->StartRotation.Quaternion(), UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), CharacterOwner))
			{
				// Avoid updating Mesh bones to physics during the teleport back, since PerformMovement() will update it right away anyway below.
				// Note: this must be before the FScopedMovementUpdate below, since that scope is what actually moves the character and mesh.
				FScopedMeshBoneUpdateOverride ScopedNoMeshBoneUpdate(CharacterOwner->GetMesh(), EKinematicBonesUpdateToPhysics::SkipAllBones);

				// Accumulate multiple transform updates until scope ends.
				FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, EScopedUpdate::DeferredUpdates);
				UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("CombineMove: add delta %f + %f and revert from %f %f to %f %f"), DeltaTime, PendingMove->DeltaTime, UpdatedComponent->GetComponentLocation().X, UpdatedComponent->GetComponentLocation().Y, OldStartLocation.X, OldStartLocation.Y);

				NewMove->CombineWith(PendingMove, CharacterOwner, PC, OldStartLocation);

				if (PC)
				{
					// We reverted position to that at the start of the pending move (above), however some code paths expect rotation to be set correctly
					// before character movement occurs (via FaceRotation), so try that now. The bOrientRotationToMovement path happens later as part of PerformMovement() and PhysicsRotation().
					CharacterOwner->FaceRotation(PC->GetControlRotation(), NewMove->DeltaTime);
				}

				SaveBaseLocation();
				NewMove->SetInitialPosition(CharacterOwner);

				// Remove pending move from move list. It would have to be the last move on the list.
				if (ClientData->SavedMoves.Num() > 0 && ClientData->SavedMoves.Last() == ClientData->PendingMove)
				{
					const bool bAllowShrinking = false;
					ClientData->SavedMoves.Pop(bAllowShrinking);
				}
				ClientData->FreeMove(ClientData->PendingMove);
				ClientData->PendingMove = nullptr;
				PendingMove = nullptr; // Avoid dangling reference, it's deleted above.
			}
			else
			{
				UE_LOG(LogNetPlayerMovement, Log, TEXT("Not combining move [would collide at start location]"));
			}
		}
		else
		{
			UE_LOG(LogNetPlayerMovement, Log, TEXT("Not combining move [not allowed by CanCombineWith()]"));
		}
	}

	// Acceleration should match what we send to the server, plus any other restrictions the server also enforces (see MoveAutonomous).
	Acceleration = NewMove->Acceleration.GetClampedToMaxSize(GetMaxAcceleration());
	AnalogInputModifier = ComputeAnalogInputModifier(); // recompute since acceleration may have changed.

														// Perform the move locally
	CharacterOwner->ClientRootMotionParams.Clear();
	CharacterOwner->SavedRootMotion.Clear();
	PerformMovement(NewMove->DeltaTime);

	NewMove->PostUpdate(CharacterOwner, FSavedMove_Character::PostUpdate_Record);

	// Add NewMove to the list
	if (CharacterOwner->IsReplicatingMovement())
	{
		check(NewMove == NewMovePtr.Get());
		ClientData->SavedMoves.Push(NewMovePtr);
		const UWorld* MyWorld = GetWorld();

		const bool bCanDelayMove = (/*CharacterMovementCVars::NetEnableMoveCombining*/ 1 != 0) && CanDelaySendingMove(NewMovePtr);

		// **TEST** if other clients see jitter.
		bool bPerdixDebugJitter = false; //When set to true, jitter happens on the server while watching clients. This is partially fixed by enabling network movement smoothing, which is off by default.

		if (bPerdixDebugJitter && bCanDelayMove && ClientData->PendingMove.IsValid() == false)
		{
			// Decide whether to hold off on move
			const float NetMoveDelta = FMath::Clamp(GetClientNetSendDeltaTime(PC, ClientData, NewMovePtr), 1.f / 120.f, 1.f / 5.f);

			if ((MyWorld->TimeSeconds - ClientData->ClientUpdateTime) * MyWorld->GetWorldSettings()->GetEffectiveTimeDilation() < NetMoveDelta)
			{
				// Delay sending this move.
				ClientData->PendingMove = NewMovePtr;
				return;
			}
		}

		ClientData->ClientUpdateTime = MyWorld->TimeSeconds;

		UE_CLOG(CharacterOwner && UpdatedComponent, LogNetPlayerMovement, Verbose, TEXT("ClientMove Time %f Acceleration %s Velocity %s Position %s DeltaTime %f Mode %s MovementBase %s.%s (Dynamic:%d) DualMove? %d"),
			NewMove->TimeStamp, *NewMove->Acceleration.ToString(), *Velocity.ToString(), *UpdatedComponent->GetComponentLocation().ToString(), NewMove->DeltaTime, *GetMovementName(),
			*GetNameSafe(NewMove->EndBase.Get()), *NewMove->EndBoneName.ToString(), MovementBaseUtility::IsDynamicBase(NewMove->EndBase.Get()) ? 1 : 0, ClientData->PendingMove.IsValid() ? 1 : 0);

		// Send move to server if this character is replicating movement
		bool bSendServerMove = true;
		//#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		//		bSendServerMove = (CharacterMovementCVars::NetForceClientServerMoveLossPercent == 0.f) || (FMath::SRand() >= CharacterMovementCVars::NetForceClientServerMoveLossPercent);
		//#endif
		if (bSendServerMove)
		{
			const FSavedMove_Character* PendingMove = ClientData->PendingMove.Get();
			//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementCallServerMove);
			CallServerMovePacked(NewMove, PendingMove, OldMove.Get());
		}
	}

	ClientData->PendingMove = NULL;
}

//////////////////////////////////////////////////////////////////////////
// Position/Attachment Replication

void UCircuitCharacterMovement::AddToMovementBuffer(FVector NewLocation, FQuat NewQuat, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, float TimeStamp, bool bRelativeRotation)
{
	if (!CharacterOwner) {
		UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement AddToMovementBuffer() !CharacterOwner"), GetWorld()->GetRealTimeSeconds());
		return;
	}

	//@BUGFREE
	//UE_LOG(LogTemp, Warning, TEXT("[%f] AddToMovementBuffer: %i %s"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num(), *NewLocation.ToCompactString());

	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy && (NewLocation - GetActorLocation()).Size() < 0.5f) {
		UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement AddToMovementBuffer() New position same as current position. Actor: %s"), GetWorld()->GetRealTimeSeconds(), *GetName());
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement AddToMovementBuffer() MovementBuffer.Num() = %d"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num());
	// Add initial location first so we have 2 points to interpolate between
	if (MovementBuffer.Num() == 0) {
		UE_LOG(LogTemp, Error, TEXT("[%f] UCircuitCharacterMovement AddToMovementBuffer() MovementBuffer.Num() = %d"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num());
		InitialMovementTime = TimeStamp - GetNetworkSendInterval();
		CurrentMovementTime = TimeStamp;

		FVector CurrentLocation = UpdatedComponent->GetComponentLocation();
		if (GetMovementBase() != nullptr) {
			//Get relative location
			CurrentLocation = UpdatedComponent->GetComponentLocation() - GetMovementBase()->GetComponentLocation();
		}

		MovementBuffer.Add(
			FInterpolationBuffer(FVector(0.0f, 0.0f, 0.0f),
				CurrentLocation,
				UpdatedComponent->GetComponentQuat(),
				TimeStamp - GetNetworkSendInterval(), GetMovementBase(), NewBaseBoneName, (GetMovementBase() == nullptr) ? false : true, (GetMovementBase() == nullptr) ? false : true, MovementMode) //Time is now + time for when it should be rendered
		);

		InitialMove = true;
	}

	if (TimeStamp > MovementBuffer[MovementBuffer.Num() - 1].TimeStamp) {
		MovementBuffer.Add(
			FInterpolationBuffer(NewVelocity,
				NewLocation,
				NewQuat,
				TimeStamp, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode, bRelativeRotation)
		);
	}

	//UE_LOG(LogTemp, Warning, TEXT("[%f] UPerdixCharacterMovement AddToMovementBuffer() Num:	%i TimeStamp:	%f	CMT:	%f"), GetWorld()->GetRealTimeSeconds(), MovementBuffer.Num(), TimeStamp, CurrentMovementTime);

	// @todo - This purges relative locations. Convert to world location.
	if (MovementBuffer.Num() > 4) {
		MovementBuffer.RemoveAt(0, 1);
		//UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement AddToMovementBuffer() Purging buffer. > 4 Actor: %s"), GetWorld()->GetRealTimeSeconds(), *GetName());
	}

}

void UCircuitCharacterMovement::ClientPerformMovement(float DeltaSeconds)
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
		if (!CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion && CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh())
		{
			// Consume root motion
			TickCharacterPose(DeltaSeconds);
			RootMotionParams.Clear();
			CurrentRootMotion.Clear();
		}
		// Clear pending physics forces
		ClearAccumulatedForces();
		return;
	}

	// Force floor update if we've moved outside of CharacterMovement since last update.
	bForceNextFloorCheck |= (IsMovingOnGround() && UpdatedComponent->GetComponentLocation() != LastUpdateLocation);

	// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened since last update.
	if (CurrentRootMotion.HasAdditiveVelocity())
	{
		const FVector Adjustment = (Velocity - LastUpdateVelocity);
		CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
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
			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
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

		// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened due to ApplyAccumulatedForces/HandlePendingLaunch
		if (CurrentRootMotion.HasAdditiveVelocity())
		{
			const FVector Adjustment = (Velocity - OldVelocity);
			CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;

#if ROOT_MOTION_DEBUG
			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
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
					if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
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
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
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

		// change position
		//StartNewPhysics(DeltaSeconds, 0); //PERDIX - Commented out to prevent character from moving in real time.

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
				DrawDebugLine(MyWorld, OldLocation, ResultingLocation, FColor::Red, true, 10.f);

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

		// consume path following requested velocity
		bHasRequestedVelocity = false;

		OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	} // End scoped movement update

	  // Call external post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);

	MaybeSaveBaseLocation();
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
			ServerLastTransformUpdateTimeStamp = MyWorld->GetTimeSeconds();
		}
	}

	LastUpdateLocation = NewLocation;
	LastUpdateRotation = NewRotation;
	LastUpdateVelocity = Velocity;
}

void UCircuitCharacterMovement::MoveClient(FInterpolationBuffer StartPosition, FInterpolationBuffer EndPosition, float LerpPercent)
{
	if (!UpdatedComponent || !CharacterOwner)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%f] UCircuitCharacterMovement MoveClient() ERROR: no UpdatedComponent or CharacterOwner"), GetWorld()->GetRealTimeSeconds());
		return;
	}

	// Make sure the base actor exists on this client.
	bool bUnresolvedBaseStart = StartPosition.bHasBase && (StartPosition.NewBase == NULL);
	if (bUnresolvedBaseStart)
	{
		if (StartPosition.bBaseRelativePosition)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("UCircuitCharacterMovement MoveClient() StartPosition - could not resolve the new relative movement base actor, ignoring server correction!"));
			return;
		}
		else
		{
			UE_LOG(LogNetPlayerMovement, Verbose, TEXT("UCircuitCharacterMovement MoveClient() StartPosition - could not resolve the new absolute movement base actor, but WILL use the position!"));
		}
	}

	// Make sure the base actor exists on this client.
	bool bUnresolvedBaseEnd = EndPosition.bHasBase && (EndPosition.NewBase == NULL);
	if (bUnresolvedBaseEnd)
	{
		if (EndPosition.bBaseRelativePosition)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("UCircuitCharacterMovement MoveClient() EndPosition could not resolve the new relative movement base actor, ignoring server correction!"));
			return;
		}
		else
		{
			UE_LOG(LogNetPlayerMovement, Verbose, TEXT("UCircuitCharacterMovement MoveClient() EndPosition could not resolve the new absolute movement base actor, but WILL use the position!"));
		}
	}

	FVector WorldShiftedStart;
	if (StartPosition.bBaseRelativePosition)
	{
		FVector BaseLocation;
		FQuat BaseRotation;
		MovementBaseUtility::GetMovementBaseTransform(StartPosition.NewBase, StartPosition.NewBaseBoneName, BaseLocation, BaseRotation); // TODO: error handling if returns false		
		WorldShiftedStart = StartPosition.Location + BaseLocation;

		//FIX - Not technically correct. Doesn't account for time.
		// Convert relative position to world position
		if (StartPosition.NewBase != nullptr && EndPosition.NewBase == nullptr) {
			StartPosition.bBaseRelativePosition = false;
			StartPosition.Location = WorldShiftedStart;
		}

		FRotator NewRotation;
		if (StartPosition.bRelativeRotation) {
			NewRotation = (FRotationMatrix(StartPosition.Quat.Rotator()) * FQuatRotationMatrix(BaseRotation)).Rotator();
			if (ShouldRemainVertical())
			{
				NewRotation.Pitch = 0.f;
				NewRotation.Roll = 0.f;
			}
			StartPosition.bRelativeRotation = false;
			StartPosition.Quat = NewRotation.Quaternion();
		}
	}
	else
	{
		WorldShiftedStart = FRepMovement::RebaseOntoLocalOrigin(StartPosition.Location, this);
	}

	FVector WorldShiftedEnd;
	if (EndPosition.bBaseRelativePosition)
	{
		FVector BaseLocation;
		FQuat BaseRotation;
		MovementBaseUtility::GetMovementBaseTransform(EndPosition.NewBase, EndPosition.NewBaseBoneName, BaseLocation, BaseRotation); // TODO: error handling if returns false		
		WorldShiftedEnd = EndPosition.Location + BaseLocation;

		// @todo - recalculate this

			//FIX - Still stutters on landing periodically.
		if (!StartPosition.bBaseRelativePosition) {
			WorldShiftedEnd = EndPosition.Location + BaseLocation + (EndPosition.NewBase->ComponentVelocity * (1.0 - LerpPercent) * (MovementBuffer[1].TimeStamp - MovementBuffer[0].TimeStamp));
			/*if (Cast<APerdixActor>(EndPosition.NewBase->GetAttachmentRootActor()) != nullptr) {
				//UE_LOG(LogTemp, Warning, TEXT("[%f] ClientAdjustPositio2  New: Y: %f FuturePos: %s t*(i[0]-i[1]): %f"), GetWorld()->GetRealTimeSeconds(), WorldShiftedNew.Y, *Cast<APerdixActor>(NewPosition.NewBase->GetAttachmentRootActor())->GetFutureLocation((1.0 - t)* (InterpolationBuffer[1].ReceivedTime - InterpolationBuffer[0].ReceivedTime)).ToString(), (1.0 - t) * (InterpolationBuffer[1].ReceivedTime - InterpolationBuffer[0].ReceivedTime));
				WorldShiftedEnd = EndPosition.Location + Cast<APerdixActor>(EndPosition.NewBase->GetAttachmentRootActor())->GetFutureLocation((1.0 - LerpPercent)* (MovementBuffer[1].TimeStamp - MovementBuffer[0].TimeStamp));
				EndPosition.bBaseRelativePosition = false;
				EndPosition.Location = EndPosition.Location + Cast<APerdixActor>(EndPosition.NewBase->GetAttachmentRootActor())->GetFutureLocation((1.0 - LerpPercent)* (MovementBuffer[1].TimeStamp - MovementBuffer[0].TimeStamp));
			}*/
		}

		FRotator NewRotation;
		if (EndPosition.bRelativeRotation) {
			NewRotation = (FRotationMatrix(EndPosition.Quat.Rotator()) * FQuatRotationMatrix(BaseRotation)).Rotator();
			if (ShouldRemainVertical())
			{
				NewRotation.Pitch = 0.f;
				NewRotation.Roll = 0.f;
			}
			EndPosition.bRelativeRotation = false;
			EndPosition.Quat = NewRotation.Quaternion();
		}
	}
	else
	{
		WorldShiftedEnd = FRepMovement::RebaseOntoLocalOrigin(EndPosition.Location, this);
		//UE_LOG(LogTemp, Warning, TEXT("[%f] ClientUpdateInterpolationMovement 2.5 World position New: Y: %f CurrentVelocity: %f"), GetWorld()->GetRealTimeSeconds(), WorldShiftedNew.Y, UpdatedComponent->GetComponentVelocity().Y);
	}

	FVector	NewLocation = FMath::LerpStable(WorldShiftedStart, WorldShiftedEnd, LerpPercent);
	FQuat NewRotation = FQuat::FastLerp(StartPosition.Quat, EndPosition.Quat, LerpPercent);
	FVector NewVelocity = FMath::LerpStable((EndPosition.NewBase == nullptr) ? EndPosition.LinearVelocity : StartPosition.LinearVelocity, EndPosition.LinearVelocity, LerpPercent);

	if (StartPosition.NewBase == nullptr && EndPosition.NewBase != nullptr) {
		NewVelocity = StartPosition.LinearVelocity;
	}
	Velocity = NewVelocity;

	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy) {
		UpdatedComponent->SetWorldLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FQuat OldRotation = UpdatedComponent->GetComponentQuat();
		bJustTeleported |= ((StartPosition.NewBase != EndPosition.NewBase) || NewLocation != OldLocation);
		UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Trust the server's movement mode
	UPrimitiveComponent* PreviousBase = GetMovementBase();
	ApplyNetworkMovementMode(((StartPosition.NewBase == nullptr) ? StartPosition.ServerMovementMode : EndPosition.ServerMovementMode));

	// Set base component
	UPrimitiveComponent* FinalBase = ((StartPosition.NewBase == nullptr) ? StartPosition.NewBase : EndPosition.NewBase);
	FName FinalBaseBoneName = ((StartPosition.NewBase == nullptr) ? StartPosition.NewBaseBoneName : EndPosition.NewBaseBoneName);
	if (bUnresolvedBaseEnd)
	{
		check(EndPosition.NewBase == NULL);
		check(!EndPosition.bBaseRelativePosition);

		// We had an unresolved base from the server
		// If walking, we'd like to continue walking if possible, to avoid falling for a frame, so try to find a base where we moved to.
		if (PreviousBase)
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false); //-V595
			if (CurrentFloor.IsWalkableFloor())
			{
				FinalBase = CurrentFloor.HitResult.Component.Get();
				FinalBaseBoneName = CurrentFloor.HitResult.BoneName;
			}
			else
			{
				FinalBase = nullptr;
				FinalBaseBoneName = NAME_None;
			}
		}
	}

	SetBase(FinalBase, FinalBaseBoneName);

	UpdateFloorFromAdjustment();
	bJustTeleported = true;

	SaveBaseLocation();
	UpdateComponentVelocity();

	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastUpdateVelocity = Velocity;
}

//////////////////////////////////////////////////////////////////////////
// Helpers

float UCircuitCharacterMovement::GetClientBufferTime()
{
	return ClientBufferTime;
}

float UCircuitCharacterMovement::GetNetworkSendInterval()
{
	return ServerSnapshotTime;
}

bool UCircuitCharacterMovement::GetUsesCustomNetworking() {
	return bUseCustomNetworking;
}