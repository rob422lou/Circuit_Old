// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/ConstraintWeldComponent.h"

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
	if (!Actor1 || !Actor2 || Actor1 == Actor2) {
		return false;
	}

	// Make sure Actor1 isn't already (edge) welded to Actor2 and return true
	if (IsDirectlyWeldedTo(Actor1, Actor2)) {
		return true;
	}

	// If actors are in same weld system just add to weldgraph and return true
	if (IsIndirectlyWeldedTo(Actor1, Actor2)) {
		WeldGraph.AddUnique(Actor1, Actor2);
		WeldGraph.AddUnique(Actor2, Actor1);
		return true;
	}

	UConstraintWeldComponent* Actor1ConstraintWeldComponent = GetConstraintWeldComponent(Actor1);
	UConstraintWeldComponent* Actor2ConstraintWeldComponent = GetConstraintWeldComponent(Actor2);

	if (!Actor1ConstraintWeldComponent || !Actor2ConstraintWeldComponent) {
		return false;
	}

	if (Actor1ConstraintWeldComponent->WeldGraph.Num() < Actor2ConstraintWeldComponent->WeldGraph.Num()) {
		Actor2ConstraintWeldComponent->WeldGraph.AddUnique(Actor1, Actor2);
		Actor2ConstraintWeldComponent->WeldGraph.AddUnique(Actor2, Actor1);

		// Move Actor1 weld manager into Actor2
		Actor2ConstraintWeldComponent->CombineConstraintWeldComponents(Actor1ConstraintWeldComponent);
	}
	else {
		Actor1ConstraintWeldComponent->WeldGraph.AddUnique(Actor1, Actor2);
		Actor1ConstraintWeldComponent->WeldGraph.AddUnique(Actor2, Actor1);

		// Move Actor2 weld manager into this WeldManager
		Actor1ConstraintWeldComponent->CombineConstraintWeldComponents(Actor2ConstraintWeldComponent);
	}

	return true;
}

bool UConstraintWeldComponent::IsDirectlyWeldedTo(AActor* Actor1, AActor* Actor2)
{
	if (WeldGraph.FindPair(Actor1, Actor2) || WeldGraph.FindPair(Actor2, Actor1)) {
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


	// Move old parent over first
	if (AddingFrom->AttachedParentRoot) {
		UE_LOG(LogTemp, Error, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() Move old parent over first"));
		AddingFrom->AttachedParentRoot->AttachToComponent(AttachedParentRoot->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true), "");

		//AddingFrom->AttachedParentRoot->WeldAttachmentReplication.AttachParent = AttachedParentRoot;
		//AddingFrom->AttachedParentRoot->WeldAttachmentReplication.AttachComponent = AttachedParentRoot->GetRootComponent();
		//AddingFrom->AttachedParentRoot->WeldAttachmentReplication.AttachSocket = AddingFrom->AttachedParentRoot->GetAttachParentSocketName();
		//AddingFrom->AttachedParentRoot->WeldAttachmentReplication.LocationOffset = AddingFrom->AttachedParentRoot->GetRootComponent()->RelativeLocation;
		//AddingFrom->AttachedParentRoot->WeldAttachmentReplication.RotationOffset = AddingFrom->AttachedParentRoot->GetRootComponent()->RelativeRotation;
		//AddingFrom->AttachedParentRoot->WeldAttachmentReplication.RelativeScale3D = AddingFrom->AttachedParentRoot->GetRootComponent()->RelativeScale3D;
		//AddingFrom->AttachedParentRoot->SetWeldManager(this);
	}

	// @todo - have client do this automatically without all the replication
	for (TPair<AActor*, AActor*> pair : AddingFrom->WeldGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("UConstraintWeldComponent - CombineConstraintWeldComponents() Move"));
		//pair.Key->SetWeldManager(this);
		if (pair.Key != AttachedParentRoot) {
			pair.Key->AttachToComponent(AttachedParentRoot->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true), "");
			//pair.Key->WeldAttachmentReplication.AttachParent = AttachedParentRoot;
			//pair.Key->WeldAttachmentReplication.AttachComponent = AttachedParentRoot->GetRootComponent();
			//pair.Key->WeldAttachmentReplication.AttachSocket = pair.Key->GetAttachParentSocketName();
			//pair.Key->WeldAttachmentReplication.LocationOffset = pair.Key->GetRootComponent()->RelativeLocation;
			//pair.Key->WeldAttachmentReplication.RotationOffset = pair.Key->GetRootComponent()->RelativeRotation;
			//pair.Key->WeldAttachmentReplication.RelativeScale3D = pair.Key->GetRootComponent()->RelativeScale3D;
		}
	}

	WeldGraph.Append(AddingFrom->WeldGraph);

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
	return Actor->FindComponentByClass<UConstraintWeldComponent>();
}

bool UConstraintWeldComponent::DFS(AActor* V, AActor* SearchingFor, TArray<AActor*>& Visited)
{
	Visited.Add(V);

	bool bReturn = false;

	TArray<AActor*> Out;
	WeldGraph.MultiFind(V, Out);

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

void UConstraintWeldComponent::DebugDrawWelds() {
	if (WeldGraph.Num() < 1) {
		return;
	}
	DrawDebugBox(GetWorld(), AttachedParentRoot->GetActorLocation(), FVector(55.0f, 55.0f, 55.0f), FColor::Blue, false, -1.0f, 0, 2.0f);
	TArray<AActor*> Out;
	WeldGraph.MultiFind(AttachedParentRoot, Out);

	for (int i = 0; i < Out.Num(); i++) {
		//DrawDebugLine(GetWorld(), GetActorLocation(), Out[i]->GetActorLocation(), FColor::Cyan, false, -1.0f, 0, 1);
		if (Out[i] && Out[i]->GetAttachParentActor()) {
			DrawDebugDirectionalArrow(GetWorld(), Out[i]->GetAttachParentActor()->GetActorLocation(), Out[i]->GetActorLocation(), 15.0f, FColor::Cyan, false, -1.0f, 0, 1);
		}
	}
}
