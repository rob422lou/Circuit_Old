// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Actors/CircuitActor.h"
#include "Circuit/Online/CircuitGameState.h"
#include "Circuit/Components/ConstraintWeldComponent.h"

// @TODO - Add the ability to attach to sockets. See CombineConstraintWeldComponents.

// Sets default values for this component's properties
UConstraintWeldComponent::UConstraintWeldComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UConstraintWeldComponent::BeginPlay()
{
	Super::BeginPlay();

	AttachedParentRoot = GetAttachmentRoot();
	// ...

	if (GetNetMode() != ENetMode::NM_Standalone && GetNetMode() != ENetMode::NM_ListenServer && GetNetMode() != ENetMode::NM_DedicatedServer) {
		UE_LOG(LogTemp, Warning, TEXT("[CLIENT] UConstraintWeldComponent - BeginPlay() ConstraintWeldComponent created"));
	}
}


// Called every frame
void UConstraintWeldComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	DebugDrawWelds();
}

//////////////////////////////////////////////////////////////////////////
// Constraints

bool UConstraintWeldComponent::AddConstraint(UPrimitiveComponent* Comp1, UPrimitiveComponent* Comp2, FName Bone1, FName Bone2) {
	if (Comp1 != nullptr && Comp2 != nullptr && Comp1 != Comp2) {
		if (!DoesConstraintExist(Comp1, Comp2, Bone1, Bone2)) {
			//UE_LOG(LogTemp, Warning, TEXT("[CLIENT] UConstraintWeldComponent - AddConstraint() !DoesConstraintExist"));
			UCircuitConstraintComponent* newComp = NewObject<UCircuitConstraintComponent>(GetOwner(), UCircuitConstraintComponent::StaticClass(), FName("Constraint-1"));
			if (newComp)
			{
				//UE_LOG(LogTemp, Warning, TEXT("[CLIENT] UConstraintWeldComponent - AddConstraint() newComp"));
				newComp->ConstraintInstance.SetDisableCollision(true);
				newComp->ConstraintInstance.SetAngularSwing1Motion(EAngularConstraintMotion::ACM_Locked);
				newComp->ConstraintInstance.SetAngularSwing2Motion(EAngularConstraintMotion::ACM_Locked);
				newComp->ConstraintInstance.SetAngularTwistMotion(EAngularConstraintMotion::ACM_Locked);
				if (GetSkeletalMeshComponent(Comp1)) {
					newComp->SetWorldLocation(Comp1->GetSocketLocation(Bone1));
					newComp->AttachToComponent(Comp2->GetAttachmentRoot(), FAttachmentTransformRules::KeepWorldTransform);
					newComp->SetConstrainedComponents(Comp1, Bone1, Comp2, Bone2);
				}
				else {
					newComp->SetWorldLocation(Comp2->GetSocketLocation(Bone1));
					newComp->AttachToComponent(Comp1->GetAttachmentRoot(), FAttachmentTransformRules::KeepWorldTransform);
					newComp->SetConstrainedComponents(Comp2, Bone2, Comp1, Bone1);
				}
				newComp->RegisterComponent();
				newComp->InitComponentConstraint();
				ConstraintGraph.Add(Comp1, newComp);
				ConstraintGraph.Add(Comp2, newComp);
			}
			return true;
		}
	}

	return false;
}

bool UConstraintWeldComponent::DoesConstraintExist(UPrimitiveComponent* Comp1, UPrimitiveComponent* Comp2, FName Bone1, FName Bone2) {
	FName bTemp1;
	FName bTemp2;
	UPrimitiveComponent* compA;
	UPrimitiveComponent* compB;

	for (TPair<UPrimitiveComponent*, UCircuitConstraintComponent*> It : ConstraintGraph)
	{
		if (It.Key == Comp1) {
			It.Value->GetConstrainedComponents(compA, bTemp1, compB, bTemp2);
			if ((compA == Comp1 && compB == Comp2 && bTemp1 == Bone1 && bTemp2 == Bone2) ||
				(compA == Comp2 && compB == Comp1 && bTemp2 == Bone1 && bTemp1 == Bone2)) {

				// @TODO - Update existing constraint
				return true;
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Weld Functions

bool UConstraintWeldComponent::AddWeld(UPrimitiveComponent* Comp1, UPrimitiveComponent* Comp2, FName Bone1, FName Bone2)
{
	// If either don't exist
	if (!Comp1 || !Comp2 || Comp1 == Comp2) {
		return false;
	}

	// @TODO - Make it so you can weld to static objects and freeze other objects until unwelded.
	// Don't allow welding to immovable objects
	if (Comp1->Mobility != EComponentMobility::Movable || Comp2->Mobility != EComponentMobility::Movable) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - AddWeld() Can't weld to static object"));
		return false;
	}

	//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - AddWeld() %s %s"), *Comp1->GetName(), *Comp2->GetName());
	// Check to see if it's a skeletal mesh
	USkeletalMeshComponent* skComp1 = GetSkeletalMeshComponent(Comp1);
	USkeletalMeshComponent* skComp2 = GetSkeletalMeshComponent(Comp2);
	if (skComp1 != nullptr || skComp2 != nullptr) {
		//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - AddWeld() skComp1"));
		return AddConstraint(Comp1, Comp2, Bone1, Bone2);
	}
	
	// If either don't have the component (This is done in the weld tool but just in case)
	if (!Comp1->GetOwner()->FindComponentByClass<UConstraintWeldComponent>()) {
		Comp1->GetOwner()->AddComponentByClass(UConstraintWeldComponent::StaticClass(), false, FTransform::Identity, false);
	}

	if (!Comp2->GetOwner()->FindComponentByClass<UConstraintWeldComponent>()) {
		Comp2->GetOwner()->AddComponentByClass(UConstraintWeldComponent::StaticClass(), false, FTransform::Identity, false);
	}

	// Make sure Actor1 isn't already (edge) welded to Actor2 and return true
	if (IsDirectlyWeldedTo(Comp1, Comp2, Bone1, Bone2)) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - AddWeld() Directly welded to"));
		return true;
	}

	// If actors are in same weld system just add to weldgraph and return true
	if (IsIndirectlyWeldedTo(Comp1, Comp2, Bone1, Bone2)) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - AddWeld() Indirectly welded to"));
		GetConstraintWeldComponentRoot(Comp1)->WeldGraph.AddUnique(Comp1, Comp2);
		GetConstraintWeldComponentRoot(Comp1)->WeldGraph.AddUnique(Comp2, Comp1);
		return true;
	}

	UConstraintWeldComponent* Actor1ConstraintWeldComponent = GetConstraintWeldComponentRoot(Comp1);
	UConstraintWeldComponent* Actor2ConstraintWeldComponent = GetConstraintWeldComponentRoot(Comp2);

	if (!Actor1ConstraintWeldComponent || !Actor2ConstraintWeldComponent) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - AddWeld() !Actor1ConstraintWeldComponent || !Actor2ConstraintWeldComponent"));
		return false;
	}

	if (Actor1ConstraintWeldComponent->WeldGraph.Num() < Actor2ConstraintWeldComponent->WeldGraph.Num()) {
		Actor2ConstraintWeldComponent->WeldGraph.AddUnique(Comp1, Comp2);
		Actor2ConstraintWeldComponent->WeldGraph.AddUnique(Comp2, Comp1);

		//UE_LOG(LogTemp, Error, TEXT("UConstraintWeldComponent - AddWeld() Here 1 %d"), Actor2ConstraintWeldComponent->WeldGraph.Num());

		// Move Actor1 weld manager into Actor2
		Actor2ConstraintWeldComponent->CombineConstraintWeldComponents(Actor1ConstraintWeldComponent);
	}
	else {
		Actor1ConstraintWeldComponent->WeldGraph.AddUnique(Comp1, Comp2);
		Actor1ConstraintWeldComponent->WeldGraph.AddUnique(Comp2, Comp1);

		//UE_LOG(LogTemp, Error, TEXT("UConstraintWeldComponent - AddWeld() Here 2 %d"), Actor1ConstraintWeldComponent->WeldGraph.Num());

		// Move Actor2 weld manager into this WeldManager
		Actor1ConstraintWeldComponent->CombineConstraintWeldComponents(Actor2ConstraintWeldComponent);
	}

	return true;
}

bool UConstraintWeldComponent::RemoveWeld(UPrimitiveComponent* KeyComp, UPrimitiveComponent* ValueComp, FName Bone1, FName Bone2)
{
	// Value actor containts rootComp as it's calling this function
	UConstraintWeldComponent* rootComp = GetConstraintWeldComponent(AttachedParentRoot);
	if (!KeyComp || !ValueComp || KeyComp == ValueComp) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() Equal or missing"));
		return false;
	}

	// No connection in graph
	if (!(rootComp->WeldGraph.FindPair(KeyComp, ValueComp) || rootComp->WeldGraph.FindPair(ValueComp, KeyComp))) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() !(WeldGraph.FindPair(KeyActor, ValueActor) || WeldGraph.FindPair(ValueActor, KeyActor)"));
		return false;
	}

	rootComp->WeldGraph.RemoveSingle(KeyComp, ValueComp);
	rootComp->WeldGraph.RemoveSingle(ValueComp, KeyComp);

	TArray<UPrimitiveComponent*> Visited;

	bool found = DFS(KeyComp, ValueComp, Visited);

	if (found) {
		// Do nothing.

		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() Another connection found, still connected"));

		return true;
	}
	else {
		// Reroot both sides

		// If this component contains the root
		if (Visited.Contains(rootComp->AttachedParentRoot)) {
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() Contains"));
			// Migrate ValueActor weld/constraints graphs to new manager. Remove that data from this WeldGraph.
			UConstraintWeldComponent* otherComp = GetConstraintWeldComponent(ValueComp);
			for (TPair<UPrimitiveComponent*, UPrimitiveComponent*> It : rootComp->WeldGraph)
			{
				if (!Visited.Contains(It.Key)) {
					otherComp->WeldGraph.AddUnique(It.Key, It.Value);
					otherComp->WeldGraph.AddUnique(It.Value, It.Key);
				}
			}
			for (TPair<UPrimitiveComponent*, UPrimitiveComponent*> It : otherComp->WeldGraph)
			{
				rootComp->WeldGraph.RemoveSingle(It.Key, It.Value);
				rootComp->WeldGraph.RemoveSingle(It.Value, It.Key);
			}

			// Have new graph reroot everything to AttachedParentRoot.
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() ValueActor: %s"), *ValueActor->GetName());
			otherComp->AttachedParentRoot = KeyComp;
			otherComp->RerootGraph();
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() otherComp->APR: %s"), *otherComp->AttachedParentRoot->GetName());
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() rootComp->APR: %s"), *rootComp->AttachedParentRoot->GetName());
			// TODO - add logic for constraints
		}
		else {
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() Does not contain"));
			// If this component does not contain the root

			UConstraintWeldComponent* otherComp = GetConstraintWeldComponent(KeyComp);
			// Migrate KeyActor weld/constraints graphs to new manager. Remove that data from this WeldGraph.
			for (TPair<UPrimitiveComponent*, UPrimitiveComponent*> It : rootComp->WeldGraph)
			{
				if (Visited.Contains(It.Key)) {
					otherComp->WeldGraph.AddUnique(It.Key, It.Value);
					otherComp->WeldGraph.AddUnique(It.Value, It.Key);
				}
			}
			for (TPair<UPrimitiveComponent*, UPrimitiveComponent*> It : otherComp->WeldGraph)
			{
				rootComp->WeldGraph.RemoveSingle(It.Key, It.Value);
				rootComp->WeldGraph.RemoveSingle(It.Value, It.Key);
			}

			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() KeyActor: %s"), *KeyActor->GetName());
			// Have new graph reroot everything to AttachedParentRoot.
			otherComp->AttachedParentRoot = KeyComp;
			otherComp->RerootGraph();
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() otherComp->APR: %s"), *otherComp->AttachedParentRoot->GetName());
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() rootComp->APR: %s"), *rootComp->AttachedParentRoot->GetName());
			// TODO - add logic for constraints
		}
		return true;
	}

	return false;
}

void UConstraintWeldComponent::RemoveAllWeldsFrom(UPrimitiveComponent* Comp)
{
	if (!Comp) {
		return;
	}

	// @todo - get all children of actor. disconnect from all children. reroot each child if needed.

	TArray<UPrimitiveComponent*> OutChildren;
	GetConstraintWeldComponentRoot(Comp)->WeldGraph.MultiFind(Comp, OutChildren);

	for (int i = 0; i < OutChildren.Num(); i++) {
		RemoveWeld(Comp, OutChildren[i], FName(""), FName(""));
	}

	// Look and see if anything was constrained to this. Move constraints.

}

bool UConstraintWeldComponent::IsDirectlyWeldedTo(UPrimitiveComponent* Comp1, UPrimitiveComponent* Comp2, FName Bone1, FName Bone2)
{
	if (GetConstraintWeldComponentRoot(Comp1)->WeldGraph.FindPair(Comp1, Comp2) || GetConstraintWeldComponentRoot(Comp1)->WeldGraph.FindPair(Comp2, Comp1)) {
		return true;
	}
	return false;
}

bool UConstraintWeldComponent::IsIndirectlyWeldedTo(UPrimitiveComponent* Comp1, UPrimitiveComponent* Comp2, FName Bone1, FName Bone2)
{
	// Trace path from Actor1 to all actors its welded to in graph
	TArray<UPrimitiveComponent*> Visited;
	return DFS(Comp1, Comp2, Visited);
}

//////////////////////////////////////////////////////////////////////////
// Helper Functions

void UConstraintWeldComponent::CombineConstraintWeldComponents(UConstraintWeldComponent* AddingFrom)
{
	if (!AddingFrom->AttachedParentRoot) {
		UE_LOG(LogTemp, Error, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() !AddingFrom->AttachedParentRoot"));
		return;
	}

	if (!AddingFrom || AddingFrom == this || !AddingFrom->AttachedParentRoot) {
		UE_LOG(LogTemp, Error, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() AddingFrom == this || AddingFrom == nullptr || !AddingFrom->AttachedParentRoot->GetRootComponent()"));
		return;
	}

	bool bWasSimulating = AddingFrom->AttachedParentRoot->IsSimulatingPhysics();

	ACircuitActor* CircuitActorRoot = Cast<ACircuitActor>(AddingFrom->AttachedParentRoot->GetOwner());

	// Move old parent over first
	if (CircuitActorRoot) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() Move old parent over first"));
		CircuitActorRoot->AttachToComponent(AttachedParentRoot, FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true), "");
		
		//CircuitActorRoot->CustomAttachmentReplication.AttachParent = AttachedParentRoot;
		CircuitActorRoot->CustomAttachmentReplication.AttachComponent = AttachedParentRoot;
		CircuitActorRoot->CustomAttachmentReplication.AttachSocket = CircuitActorRoot->GetAttachParentSocketName();
		CircuitActorRoot->CustomAttachmentReplication.LocationOffset = CircuitActorRoot->GetRootComponent()->GetRelativeLocation();
		CircuitActorRoot->CustomAttachmentReplication.RotationOffset = CircuitActorRoot->GetRootComponent()->GetRelativeRotation();
		CircuitActorRoot->CustomAttachmentReplication.RelativeScale3D = CircuitActorRoot->GetRootComponent()->GetRelativeScale3D();

		//AddingFrom->AttachedParentRoot->SetWeldManager(this);
	}

	//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() Pre-move"));

	// @todo - have client do this automatically without all the replication
	// Update actors that were attached to other WeldGraph to this component
	for (TPair<UPrimitiveComponent*, UPrimitiveComponent*> pair : AddingFrom->WeldGraph)
	{
		//pair.Key->SetWeldManager(this);
		if (pair.Key != AttachedParentRoot) {
			UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() Move"));

			ACircuitActor* PairCircuitActor = Cast<ACircuitActor>(pair.Key);
			pair.Key->AttachToComponent(AttachedParentRoot, FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true), "");

			//PairCircuitActor->CustomAttachmentReplication.AttachParent = AttachedParentRoot;
			PairCircuitActor->CustomAttachmentReplication.AttachComponent = AttachedParentRoot;
			PairCircuitActor->CustomAttachmentReplication.AttachSocket = pair.Key->GetAttachSocketName();
			PairCircuitActor->CustomAttachmentReplication.LocationOffset = pair.Key->GetRelativeLocation();
			PairCircuitActor->CustomAttachmentReplication.RotationOffset = pair.Key->GetRelativeRotation();
			PairCircuitActor->CustomAttachmentReplication.RelativeScale3D = pair.Key->GetRelativeScale3D();
		}
	}

	AddingFrom->AttachedParentRoot = AttachedParentRoot;

	AttachedParentRoot->GetOwner()->FindComponentByClass<UConstraintWeldComponent>()->WeldGraph.Append(AddingFrom->WeldGraph);

	// Clear AddingFrom WeldGraph
	AddingFrom->WeldGraph.Empty();

	// @todo - there's no cast protection
	//Cast<UPrimitiveComponent>(AttachedParentRoot->GetRootComponent())->SetSimulatePhysics(AttachedParentRoot->GetRootComponent()->IsSimulatingPhysics() && bWasSimulating);

	/*
	for (TPair<UPrimitiveComponent*, UCircuitConstraintComponent*> It : AddingFrom->ConstraintGraph)
	{
		ConstraintGraph.Add(It.Key, It.Value);
		if (GetSkeletalMeshComponent(Comp1)) {
			It.Value->SetWorldLocation(Comp1->GetSocketLocation(Bone1));
			It.Value->AttachToComponent(Comp2->GetAttachmentRoot(), FAttachmentTransformRules::KeepWorldTransform);
			It.Value->SetConstrainedComponents(Comp1, Bone1, Comp2, Bone2);
		}
		else {
			It.Value->SetWorldLocation(Comp2->GetSocketLocation(Bone1));
			It.Value->AttachToComponent(Comp1->GetAttachmentRoot(), FAttachmentTransformRules::KeepWorldTransform);
			It.Value->SetConstrainedComponents(Comp2, Bone2, Comp1, Bone1);
		}
		It.Value->RegisterComponent();
		It.Value->InitComponentConstraint();
	}
	*/
}

UConstraintWeldComponent* UConstraintWeldComponent::GetConstraintWeldComponent(USceneComponent* Comp) {
	if (Comp->GetOwner()->FindComponentByClass<UConstraintWeldComponent>() != nullptr && Comp->GetOwner()->FindComponentByClass<UConstraintWeldComponent>()->AttachedParentRoot != nullptr) {
		//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - GetConstraintWeldComponent() Here"));
		return Comp->GetOwner()->FindComponentByClass<UConstraintWeldComponent>();
	}
	return Comp->GetOwner()->FindComponentByClass<UConstraintWeldComponent>();
}

UConstraintWeldComponent* UConstraintWeldComponent::GetConstraintWeldComponentRoot(USceneComponent* Comp) {
	if (Comp->GetOwner()->FindComponentByClass<UConstraintWeldComponent>() != nullptr && Comp->GetOwner()->FindComponentByClass<UConstraintWeldComponent>()->AttachedParentRoot != nullptr) {
		//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - GetConstraintWeldComponent() Here"));
		return Comp->GetOwner()->FindComponentByClass<UConstraintWeldComponent>()->AttachedParentRoot->GetOwner()->FindComponentByClass<UConstraintWeldComponent>();
	}
	return Comp->GetOwner()->FindComponentByClass<UConstraintWeldComponent>();
}

USkeletalMeshComponent* UConstraintWeldComponent::GetSkeletalMeshComponent(UPrimitiveComponent* Comp) {
	return Cast<USkeletalMeshComponent>(Comp);
}

bool UConstraintWeldComponent::DFS(UPrimitiveComponent* V, UPrimitiveComponent* SearchingFor, TArray<UPrimitiveComponent*>& Visited)
{
	Visited.Add(V);

	bool bReturn = false;

	TArray<UPrimitiveComponent*> Out;
	GetConstraintWeldComponentRoot(AttachedParentRoot)->WeldGraph.MultiFind(V, Out);

	for (int i = 0; i < Out.Num(); i++) {
		if (Out[i] == SearchingFor) {
			DFS(Out[i], SearchingFor, Visited);
			return true;
		}

		if (!Visited.Contains(Out[i])) {
			bReturn = bReturn || DFS(Out[i], SearchingFor, Visited);
		}
	}

	return bReturn;
}

void UConstraintWeldComponent::RerootGraph()
{
	UConstraintWeldComponent* rootComp = GetConstraintWeldComponent(AttachedParentRoot);

	UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RerootGraph() rootComp->AttachedParentRoot: %s"), *rootComp->AttachedParentRoot->GetName());
	//AttachedParentRoot->WeldAttachmentReplication.AttachParent = NULL;
	rootComp->AttachedParentRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	//AttachedParentRoot->SetWeldManager(this);
	for (TPair<UPrimitiveComponent*, UPrimitiveComponent*> It : rootComp->WeldGraph)
	{
		if (It.Key != rootComp->AttachedParentRoot) {
			It.Key->AttachToComponent(rootComp->AttachedParentRoot, FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true), "");
			GetConstraintWeldComponent(It.Key)->AttachedParentRoot = rootComp->AttachedParentRoot;
			/*
			if (It.Key->WeldAttachmentReplication.AttachParent != AttachedParentRoot) {
				It.Key->AttachToComponent(AttachedParentRoot->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true), "");
				It.Key->WeldAttachmentReplication.AttachParent = AttachedParentRoot;
				It.Key->WeldAttachmentReplication.AttachComponent = AttachedParentRoot->GetRootComponent();
				It.Key->WeldAttachmentReplication.LocationOffset = It.Key->GetRootComponent()->RelativeLocation;
				It.Key->WeldAttachmentReplication.RotationOffset = It.Key->GetRootComponent()->RelativeRotation;
				It.Key->WeldAttachmentReplication.RelativeScale3D = It.Key->GetRootComponent()->RelativeScale3D;
				It.Key->SetWeldManager(this);
			}
			*/
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Debug

void UConstraintWeldComponent::DebugDrawWelds() {
	if (WeldGraph.Num() < 1) {
		return;
	}
	DrawDebugBox(GetWorld(), AttachedParentRoot->GetComponentLocation(), FVector(55.0f, 55.0f, 55.0f), FColor::Blue, false, -1.0f, 0, 2.0f);

	for (const TPair<UPrimitiveComponent*, UPrimitiveComponent*>& pair : WeldGraph)
	{
		if (pair.Value && pair.Key) {
			DrawDebugDirectionalArrow(GetWorld(), pair.Key->GetComponentLocation(), pair.Value->GetComponentLocation(), 15.0f, FColor::Cyan, false, -1.0f, 0, 1);
			DrawDebugDirectionalArrow(GetWorld(), AttachedParentRoot->GetComponentLocation(), pair.Value->GetComponentLocation(), 15.0f, FColor::Orange, false, -1.0f, 0, 1);
		}
	}
}

int UConstraintWeldComponent::DebugGetWeldGraphNum() {
	return WeldGraph.Num();
}

FString UConstraintWeldComponent::DebugGetAttachedParentRootName() {
	return AttachedParentRoot->GetName();
}