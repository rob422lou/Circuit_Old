// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "WireComponent.generated.h"

UENUM(BlueprintType)
enum class EWireDataType : uint8
{
	Bool = 0,
	Int32,
	Float,
	String
};

/* Contains the actor and the message identifier. */
USTRUCT(BlueprintType)
struct FWireConnectedInputInfo
{
	GENERATED_USTRUCT_BODY()

	/* This is the input actor we are sending data to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circuit|Wire")
	UWireComponent* InputComponent;

	/* Input actor uses this as a message identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circuit|Wire")
	FName InputName;
};

/* Is an event container. Contains the event name, the message type, and all the actors which want to be notified of changes */
USTRUCT(BlueprintType)
struct FWireEvent
{
	GENERATED_USTRUCT_BODY()

	/* In game friendly name to allow searching event names. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circuit|Wire")
	FName EventName;

	/* In game friendly data type to allow easy connection of correct input/output types. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circuit|Wire")
	EWireDataType EnumType;

	/* Each observer is sent a message with a data type from an output actor. */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circuit|Wire")
	TArray<FWireConnectedInputInfo> Observers;
};

USTRUCT(BlueprintType)
struct FWireListen
{
	GENERATED_USTRUCT_BODY()

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire|Data")
	TArray<UWireComponent*> OutputComponents;

	/* In game friendly name to allow searching event names. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circuit|Wire")
	FName EventName;

	/* In game friendly data type to allow easy connection of correct input/output types. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circuit|Wire")
	EWireDataType EnumType;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SHOOTERGAME_API UWireComponent : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWireComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire")
	TArray<FWireListen> Inputs;

	/* Maintains a list of events that can be used to notify other OdysseyPoweredActors of changes in this actor. */
	/* Used in messaging system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wire", meta = (DisplayName = "Outputs"))
	TArray<FWireEvent> Events;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/* Used to retrieve the variable (in blueprint) paired with events. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Get Event Data (bool)"))
	bool GetDataBool(FName EventName);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Get Event Data (int32)"))
	int32 GetDataInt32(FName EventName);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Get Event Data (float)"))
	float GetDataFloat(FName EventName);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Get Event Data (string)"))
	FName GetDataString(FName EventName);

	/* Adds observers to any event. Updates observer with current event variable value. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Add Observer to Event"))
		bool AddObserverToActorEvent(UWireComponent* OutputActor, UWireComponent* Observer, FName InputName, FName EventName);

	/* Removes observers from a specific event. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Disconnect Observer from Event"))
		bool DisconnectObserverFromActorEvent(UWireComponent* Observer, FName InputName, FName EventName);

	/* Removes observers from a specific event. */
	UFUNCTION(BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Disconnect All Wires"))
		void DisconnectFromAllWires();

	/* Messaging System */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FReceiveDataBool, UWireComponent*, Sender, FName, InputName, bool, Data);
	UPROPERTY(BlueprintAssignable, Category = "Wire|Data")
		FReceiveDataBool ReceiveDataBool;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Send Message (bool)"))
		void SendDataBool(UWireComponent* Sender, FName EventName, FName InputName, bool Data);
	//UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Receive Message (bool)"))
	//void ReceiveDataBool(UWireComponent* Sender, FName InputName, bool Data);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Send Message (int32)"))
		void SendDataInt32(UWireComponent* Sender, FName EventName, FName InputName, int32 Data);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Receive Message (int32)"))
		void ReceiveDataInt32(UWireComponent* Sender, FName InputName, int32 Data);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Send Message (float)"))
		void SendDataFloat(UWireComponent* Sender, FName EventName, FName InputName, float Data);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Receive Message (float)"))
		void ReceiveDataFloat(UWireComponent* Sender, FName InputName, float Data);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Send Message (string)"))
		void SendDataString(UWireComponent* Sender, FName EventName, FName InputName, FName Data);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wire|Data", meta = (DisplayName = "Receive Message (string)"))
		void ReceiveDataString(UWireComponent* Sender, FName InputName, FName Data);
		
};
