// Fill out your copyright notice in the Description page of Project Settings.

#include "ShooterGame.h"
#include "Circuit/Components/WireComponent.h"

// Sets default values for this component's properties
UWireComponent::UWireComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UWireComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...

}


// Called every frame
void UWireComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

// CIRCUIT TODO 
// Fix delegate observers. Event has a built in system for notifying listeners.
// https://unreal.gg-labs.com/wiki-archives/macros-and-data-types/delegates-in-ue4-raw-c++-and-bp-exposed

/*
* Used in Blueprint/C++
* Adds an Observer/InputName pair to an event. Then updates observer with that event value.
* If it succeeds true is returned. Otherwise false.
*/
bool UWireComponent::AddObserverToActorEvent_Implementation(UWireComponent* OutputActor, UWireComponent* Observer, FName InputName, FName EventName)
{
	for (int i = Events.Num() - 1; i >= 0; i--)
	{
		if (Events[i].EventName.IsEqual(EventName))
		{
			//Make sure not already an observer
			for (int x = 0; x < Events[i].Observers.Num(); x++)
			{
				if (Events[i].Observers[x].InputComponent == Observer) {
					return false;
				}
			}

			FConnectedWireInputInfo NewObserver;
			NewObserver.InputComponent = Observer;
			NewObserver.InputName = InputName;

			for (int y = 0; y < Observer->Inputs.Num(); y++) {
				if (Observer->Inputs[y].EventName == InputName) {
					Observer->Inputs[y].OutputComponents.Push(OutputActor);
				}
			}

			switch (Events[i].EnumType)
			{
			case EDataType::Int32:
			{
				Events[i].Observers.Push(NewObserver);
				Observer->SendDataInt32(Observer, EventName, InputName, Observer->GetDataInt32(EventName));
				return true;
				break;
			}
			case EDataType::Float:
			{
				Events[i].Observers.Push(NewObserver);
				Observer->SendDataFloat(Observer, EventName, InputName, Observer->GetDataFloat(EventName));
				return true;
				break;
			}
			case EDataType::String:
			{
				Events[i].Observers.Push(NewObserver);
				Observer->SendDataString(Observer, EventName, InputName, Observer->GetDataString(EventName));
				return true;
				break;
			}
			case EDataType::Bool:
			{
				Events[i].Observers.Push(NewObserver);
				Observer->SendDataBool(Observer, EventName, InputName, Observer->GetDataBool(EventName));
				return true;
				break;
			}
			default:
				return false;
				break;
			}
			return true;

		}
	}
	return false;
}

bool UWireComponent::DisconnectObserverFromActorEvent_Implementation(UWireComponent* Observer, FName InputName, FName EventName)
{
	for (int y = Observer->Inputs.Num() - 1; y >= 0; y--) {
		if (Observer->Inputs[y].EventName == InputName) {
			for (int w = Observer->Inputs[y].OutputComponents.Num() - 1; w >= 0; w--) {
				if (Observer->Inputs[y].OutputComponents[w] == this) {
					Observer->Inputs[y].OutputComponents.RemoveAt(w);
					break;
				}
				if (Observer->Inputs[y].OutputComponents[w] == NULL) {
					Observer->Inputs[y].OutputComponents.RemoveAt(w);
				}
			}
		}
	}

	for (int i = Events.Num() - 1; i >= 0; i--)
	{
		if (Events[i].EventName.IsEqual(EventName))
		{
			for (int x = Events[i].Observers.Num() - 1; x >= 0; x--) {
				if (Events[i].Observers[x].InputComponent == NULL)
				{
					Events[i].Observers.RemoveAt(x);
					continue;
				}
				if (Events[i].Observers[x].InputComponent == Observer)
				{
					Events[i].Observers.RemoveAt(x);
					return true;
				}
			}
			return false;
		}
	}
	return false;
}

void UWireComponent::DisconnectFromAllWires()
{
	for (int i = Events.Num() - 1; i >= 0; i--) {
		for (int y = Events[i].Observers.Num() - 1; y >= 0; y--) {
			DisconnectObserverFromActorEvent(Events[i].Observers[y].InputComponent, Events[i].Observers[y].InputName, Events[i].EventName);
		}
	}
}

// No implementation, needs to be implemented in blueprints.
bool UWireComponent::GetDataBool_Implementation(FName EventName)
{
	return false;
}

void UWireComponent::SendDataBool_Implementation(UWireComponent* Sender, FName EventName, FName InputName, bool Data)
{
	for (int i = 0; i < Events.Num(); i++)
	{
		if (Events[i].EventName.IsEqual(EventName))
		{
			for (int x = Events[i].Observers.Num() - 1; x >= 0; x--) {
				if (Events[i].Observers[x].InputComponent == NULL)
				{
					Events[i].Observers.RemoveAt(x);
					continue;
				}
				Events[i].Observers[x].InputComponent->ReceiveDataBool.Broadcast(this, Events[i].Observers[x].InputName, Data);
			}
			return;
		}
	}
}

// No implementation, needs to be implemented in blueprints.
int32 UWireComponent::GetDataInt32_Implementation(FName EventName)
{
	return -1;
}

void UWireComponent::SendDataInt32_Implementation(UWireComponent* Sender, FName EventName, FName InputName, int32 Data)
{
	for (int i = 0; i < Events.Num(); i++)
	{
		if (Events[i].EventName.IsEqual(EventName))
		{
			for (int x = Events[i].Observers.Num() - 1; x >= 0; x--) {
				if (Events[i].Observers[x].InputComponent == NULL)
				{
					Events[i].Observers.RemoveAt(x);
					continue;
				}
				Events[i].Observers[x].InputComponent->ReceiveDataInt32(this, Events[i].Observers[x].InputName, Data);
			}
			return;
		}
	}
}

void UWireComponent::ReceiveDataInt32_Implementation(UWireComponent* Sender, FName InputName, int32 Data)
{

}

// No implementation, needs to be implemented in blueprints.
float UWireComponent::GetDataFloat_Implementation(FName EventName)
{
	return 0.0f;
}

void UWireComponent::SendDataFloat_Implementation(UWireComponent* Sender, FName EventName, FName InputName, float Data)
{
	for (int i = 0; i < Events.Num(); i++)
	{
		if (Events[i].EventName.IsEqual(EventName))
		{
			for (int x = Events[i].Observers.Num() - 1; x >= 0; x--) {
				if (Events[i].Observers[x].InputComponent == NULL)
				{
					Events[i].Observers.RemoveAt(x);
					continue;
				}
				Events[i].Observers[x].InputComponent->ReceiveDataFloat(this, Events[i].Observers[x].InputName, Data);
			}
			return;
		}
	}
}

void UWireComponent::ReceiveDataFloat_Implementation(UWireComponent* Sender, FName InputName, float Data)
{

}

// No implementation, needs to be implemented in blueprints.
FName UWireComponent::GetDataString_Implementation(FName EventName)
{
	return "";
}

void UWireComponent::SendDataString_Implementation(UWireComponent* Sender, FName EventName, FName InputName, FName Data)
{
	for (int i = 0; i < Events.Num(); i++)
	{
		if (Events[i].EventName.IsEqual(EventName))
		{
			for (int x = Events[i].Observers.Num() - 1; x >= 0; x--) {
				if (Events[i].Observers[x].InputComponent == NULL)
				{
					Events[i].Observers.RemoveAt(x);
					continue;
				}
				Events[i].Observers[x].InputComponent->ReceiveDataString(this, Events[i].Observers[x].InputName, Data);
			}
			return;
		}
	}
}

void UWireComponent::ReceiveDataString_Implementation(UWireComponent* Sender, FName InputName, FName Data)
{

}



