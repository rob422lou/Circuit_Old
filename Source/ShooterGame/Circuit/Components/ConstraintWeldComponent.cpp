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

	AttachedParentRoot = GetOwner();
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
// Weld Functions

bool UConstraintWeldComponent::AddWeld(AActor* Actor1, AActor* Actor2)
{
	// If either don't exist
	if (!Actor1 || !Actor2 || Actor1 == Actor2) {
		return false;
	}

	// @TODO - Make it so you can weld to static objects and freeze other objects until unwelded.
	// Don't allow welding to immovable objects
	if (Actor1->GetRootComponent()->Mobility != EComponentMobility::Movable || Actor2->GetRootComponent()->Mobility != EComponentMobility::Movable) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - AddWeld() Can't weld to static object"));
		return false;
	}

	// If either don't have the component (This is done in the weld tool but just in case)
	if (!Actor1->FindComponentByClass<UConstraintWeldComponent>()) {
		Actor1->AddComponentByClass(UConstraintWeldComponent::StaticClass(), false, FTransform::Identity, false);
	}

	if (!Actor2->FindComponentByClass<UConstraintWeldComponent>()) {
		Actor2->AddComponentByClass(UConstraintWeldComponent::StaticClass(), false, FTransform::Identity, false);
	}

	// Make sure Actor1 isn't already (edge) welded to Actor2 and return true
	if (IsDirectlyWeldedTo(Actor1, Actor2)) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - AddWeld() Directly welded to"));
		return true;
	}

	// If actors are in same weld system just add to weldgraph and return true
	if (IsIndirectlyWeldedTo(Actor1, Actor2)) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - AddWeld() Indirectly welded to"));
		GetConstraintWeldComponentRoot(Actor1)->WeldGraph.AddUnique(Actor1, Actor2);
		GetConstraintWeldComponentRoot(Actor1)->WeldGraph.AddUnique(Actor2, Actor1);
		return true;
	}

	UConstraintWeldComponent* Actor1ConstraintWeldComponent = GetConstraintWeldComponentRoot(Actor1);
	UConstraintWeldComponent* Actor2ConstraintWeldComponent = GetConstraintWeldComponentRoot(Actor2);

	if (!Actor1ConstraintWeldComponent || !Actor2ConstraintWeldComponent) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - AddWeld() !Actor1ConstraintWeldComponent || !Actor2ConstraintWeldComponent"));
		return false;
	}

	if (Actor1ConstraintWeldComponent->WeldGraph.Num() < Actor2ConstraintWeldComponent->WeldGraph.Num()) {
		Actor2ConstraintWeldComponent->WeldGraph.AddUnique(Actor1, Actor2);
		Actor2ConstraintWeldComponent->WeldGraph.AddUnique(Actor2, Actor1);

		//UE_LOG(LogTemp, Error, TEXT("UConstraintWeldComponent - AddWeld() Here 1 %d"), Actor2ConstraintWeldComponent->WeldGraph.Num());

		// Move Actor1 weld manager into Actor2
		Actor2ConstraintWeldComponent->CombineConstraintWeldComponents(Actor1ConstraintWeldComponent);
	}
	else {
		Actor1ConstraintWeldComponent->WeldGraph.AddUnique(Actor1, Actor2);
		Actor1ConstraintWeldComponent->WeldGraph.AddUnique(Actor2, Actor1);

		//UE_LOG(LogTemp, Error, TEXT("UConstraintWeldComponent - AddWeld() Here 2 %d"), Actor1ConstraintWeldComponent->WeldGraph.Num());

		// Move Actor2 weld manager into this WeldManager
		Actor1ConstraintWeldComponent->CombineConstraintWeldComponents(Actor2ConstraintWeldComponent);
	}

	return true;
}

bool UConstraintWeldComponent::RemoveWeld(AActor* KeyActor, AActor* ValueActor)
{
	// Value actor containts rootComp as it's calling this function
	UConstraintWeldComponent* rootComp = GetConstraintWeldComponent(AttachedParentRoot);
	if (!KeyActor || !ValueActor || KeyActor == ValueActor) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() Equal or missing"));
		return false;
	}

	// No connection in graph
	if (!(rootComp->WeldGraph.FindPair(KeyActor, ValueActor) || rootComp->WeldGraph.FindPair(ValueActor, KeyActor))) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() !(WeldGraph.FindPair(KeyActor, ValueActor) || WeldGraph.FindPair(ValueActor, KeyActor)"));
		return false;
	}

	rootComp->WeldGraph.RemoveSingle(KeyActor, ValueActor);
	rootComp->WeldGraph.RemoveSingle(ValueActor, KeyActor);

	TArray<AActor*> Visited;

	bool found = DFS(KeyActor, ValueActor, Visited);

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
			UConstraintWeldComponent* otherComp = GetConstraintWeldComponent(ValueActor);
			for (TPair<AActor*, AActor*> It : rootComp->WeldGraph)
			{
				if (!Visited.Contains(It.Key)) {
					otherComp->WeldGraph.AddUnique(It.Key, It.Value);
					otherComp->WeldGraph.AddUnique(It.Value, It.Key);
				}
			}
			for (TPair<AActor*, AActor*> It : otherComp->WeldGraph)
			{
				rootComp->WeldGraph.RemoveSingle(It.Key, It.Value);
				rootComp->WeldGraph.RemoveSingle(It.Value, It.Key);
			}

			// Have new graph reroot everything to AttachedParentRoot.
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() ValueActor: %s"), *ValueActor->GetName());
			otherComp->AttachedParentRoot = ValueActor;
			otherComp->RerootGraph();
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() otherComp->APR: %s"), *otherComp->AttachedParentRoot->GetName());
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() rootComp->APR: %s"), *rootComp->AttachedParentRoot->GetName());
			// TODO - add logic for constraints
		}
		else {
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() Does not contain"));
			// If this component does not contain the root

			UConstraintWeldComponent* otherComp = GetConstraintWeldComponent(KeyActor);
			// Migrate KeyActor weld/constraints graphs to new manager. Remove that data from this WeldGraph.
			for (TPair<AActor*, AActor*> It : rootComp->WeldGraph)
			{
				if (Visited.Contains(It.Key)) {
					otherComp->WeldGraph.AddUnique(It.Key, It.Value);
					otherComp->WeldGraph.AddUnique(It.Value, It.Key);
				}
			}
			for (TPair<AActor*, AActor*> It : otherComp->WeldGraph)
			{
				rootComp->WeldGraph.RemoveSingle(It.Key, It.Value);
				rootComp->WeldGraph.RemoveSingle(It.Value, It.Key);
			}

			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() KeyActor: %s"), *KeyActor->GetName());
			// Have new graph reroot everything to AttachedParentRoot.
			otherComp->AttachedParentRoot = KeyActor;
			otherComp->RerootGraph();
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() otherComp->APR: %s"), *otherComp->AttachedParentRoot->GetName());
			//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - RemoveWeld() rootComp->APR: %s"), *rootComp->AttachedParentRoot->GetName());
			// TODO - add logic for constraints
		}
		return true;
	}

	return false;
}

void UConstraintWeldComponent::RemoveAllWeldsFrom(AActor* Actor)
{
	if (!Actor) {
		return;
	}

	// @todo - get all children of actor. disconnect from all children. reroot each child if needed.

	TArray<AActor*> OutChildren;
	GetConstraintWeldComponentRoot(Actor)->WeldGraph.MultiFind(Actor, OutChildren);

	for (int i = 0; i < OutChildren.Num(); i++) {
		RemoveWeld(Actor, OutChildren[i]);
	}

	// Look and see if anything was constrained to this. Move constraints.

}

bool UConstraintWeldComponent::IsDirectlyWeldedTo(AActor* Actor1, AActor* Actor2)
{
	if (GetConstraintWeldComponentRoot(Actor1)->WeldGraph.FindPair(Actor1, Actor2) || GetConstraintWeldComponentRoot(Actor1)->WeldGraph.FindPair(Actor2, Actor1)) {
		return true;
	}
	return false;
}

bool UConstraintWeldComponent::IsIndirectlyWeldedTo(AActor* Actor1, AActor* Actor2)
{
	// Trace path from Actor1 to all actors its welded to in graph
	TArray<AActor*> Visited;
	return DFS(Actor1, Actor2, Visited);
}

//////////////////////////////////////////////////////////////////////////
// Helper Functions

void UConstraintWeldComponent::CombineConstraintWeldComponents(UConstraintWeldComponent* AddingFrom)
{
	if (!AddingFrom->AttachedParentRoot) {
		UE_LOG(LogTemp, Error, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() !AddingFrom->AttachedParentRoot"));
		return;
	}

	if (!AddingFrom || AddingFrom == this || !AddingFrom->AttachedParentRoot->GetRootComponent()) {
		UE_LOG(LogTemp, Error, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() AddingFrom == this || AddingFrom == nullptr || !AddingFrom->AttachedParentRoot->GetRootComponent()"));
		return;
	}

	bool bWasSimulating = AddingFrom->AttachedParentRoot->GetRootComponent()->IsSimulatingPhysics();

	ACircuitActor* CircuitActorRoot = Cast<ACircuitActor>(AddingFrom->AttachedParentRoot);

	// Move old parent over first
	if (CircuitActorRoot) {
		UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() Move old parent over first"));
		CircuitActorRoot->AttachToComponent(AttachedParentRoot->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true), "");
		
		CircuitActorRoot->CustomAttachmentReplication.AttachParent = AttachedParentRoot;
		CircuitActorRoot->CustomAttachmentReplication.AttachComponent = AttachedParentRoot->GetRootComponent();
		CircuitActorRoot->CustomAttachmentReplication.AttachSocket = CircuitActorRoot->GetAttachParentSocketName();
		CircuitActorRoot->CustomAttachmentReplication.LocationOffset = CircuitActorRoot->GetRootComponent()->GetRelativeLocation();
		CircuitActorRoot->CustomAttachmentReplication.RotationOffset = CircuitActorRoot->GetRootComponent()->GetRelativeRotation();
		CircuitActorRoot->CustomAttachmentReplication.RelativeScale3D = CircuitActorRoot->GetRootComponent()->GetRelativeScale3D();

		//AddingFrom->AttachedParentRoot->SetWeldManager(this);
	}

	//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() Pre-move"));

	// @todo - have client do this automatically without all the replication
	// Update actors that were attached to other WeldGraph to this component
	for (TPair<AActor*, AActor*> pair : AddingFrom->WeldGraph)
	{
		//pair.Key->SetWeldManager(this);
		if (pair.Key != AttachedParentRoot) {
			UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() Move"));

			ACircuitActor* PairCircuitActor = Cast<ACircuitActor>(pair.Key);
			pair.Key->AttachToComponent(AttachedParentRoot->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true), "");

			PairCircuitActor->CustomAttachmentReplication.AttachParent = AttachedParentRoot;
			PairCircuitActor->CustomAttachmentReplication.AttachComponent = AttachedParentRoot->GetRootComponent();
			PairCircuitActor->CustomAttachmentReplication.AttachSocket = pair.Key->GetAttachParentSocketName();
			PairCircuitActor->CustomAttachmentReplication.LocationOffset = pair.Key->GetRootComponent()->GetRelativeLocation();
			PairCircuitActor->CustomAttachmentReplication.RotationOffset = pair.Key->GetRootComponent()->GetRelativeRotation();
			PairCircuitActor->CustomAttachmentReplication.RelativeScale3D = pair.Key->GetRootComponent()->GetRelativeScale3D();
		}
	}

	AddingFrom->AttachedParentRoot = AttachedParentRoot;

	AttachedParentRoot->FindComponentByClass<UConstraintWeldComponent>()->WeldGraph.Append(AddingFrom->WeldGraph);

	// Clear AddingFrom WeldGraph
	AddingFrom->WeldGraph.Empty();

	// @todo - there's no cast protection
	//Cast<UPrimitiveComponent>(AttachedParentRoot->GetRootComponent())->SetSimulatePhysics(AttachedParentRoot->GetRootComponent()->IsSimulatingPhysics() && bWasSimulating);
	/*
	for (TPair<AActor*, UPerdixPhysicsConstraintComponent*> It : AddingFrom->ConstraintMap)
	{
		ConstraintMap.AddUnique(It.Key, It.Value);
		It.Value->ReattachToParents();
	}
	*/
}

UConstraintWeldComponent* UConstraintWeldComponent::GetConstraintWeldComponent(AActor* Actor) {
	if (Actor->FindComponentByClass<UConstraintWeldComponent>() != nullptr && Actor->FindComponentByClass<UConstraintWeldComponent>()->AttachedParentRoot != nullptr) {
		//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - GetConstraintWeldComponent() Here"));
		return Actor->FindComponentByClass<UConstraintWeldComponent>();
	}
	return Actor->FindComponentByClass<UConstraintWeldComponent>();
}

UConstraintWeldComponent* UConstraintWeldComponent::GetConstraintWeldComponentRoot(AActor* Actor) {
	if (Actor->FindComponentByClass<UConstraintWeldComponent>() != nullptr && Actor->FindComponentByClass<UConstraintWeldComponent>()->AttachedParentRoot != nullptr) {
		//UE_LOG(LogTemp, Warning, TEXT("UConstraintWeldComponent - GetConstraintWeldComponent() Here"));
		return Actor->FindComponentByClass<UConstraintWeldComponent>()->AttachedParentRoot->FindComponentByClass<UConstraintWeldComponent>();
	}
	return Actor->FindComponentByClass<UConstraintWeldComponent>();
}

bool UConstraintWeldComponent::DFS(AActor* V, AActor* SearchingFor, TArray<AActor*>& Visited)
{
	Visited.Add(V);

	bool bReturn = false;

	TArray<AActor*> Out;
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
	rootComp->AttachedParentRoot->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	//AttachedParentRoot->SetWeldManager(this);
	for (TPair<AActor*, AActor*> It : rootComp->WeldGraph)
	{
		if (It.Key != rootComp->AttachedParentRoot) {
			It.Key->AttachToComponent(rootComp->AttachedParentRoot->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true), "");
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
	DrawDebugBox(GetWorld(), AttachedParentRoot->GetActorLocation(), FVector(55.0f, 55.0f, 55.0f), FColor::Blue, false, -1.0f, 0, 2.0f);

	for (const TPair<AActor*, AActor*>& pair : WeldGraph)
	{
		if (pair.Value && pair.Key) {
			DrawDebugDirectionalArrow(GetWorld(), pair.Key->GetActorLocation(), pair.Value->GetActorLocation(), 15.0f, FColor::Cyan, false, -1.0f, 0, 1);
			DrawDebugDirectionalArrow(GetWorld(), AttachedParentRoot->GetActorLocation(), pair.Value->GetActorLocation(), 15.0f, FColor::Orange, false, -1.0f, 0, 1);
		}
	}
}

int UConstraintWeldComponent::DebugGetWeldGraphNum() {
	return WeldGraph.Num();
}

FString UConstraintWeldComponent::DebugGetAttachedParentRootName() {
	return AttachedParentRoot->GetName();
}