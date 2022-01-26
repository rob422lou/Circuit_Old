// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "BaseGravityComponent.generated.h"

UENUM(BlueprintType)
namespace EGravityFieldType
{
	enum  Type
	{
		EGT_Point 			UMETA(DisplayName = "Point Gravity"),
		EGT_Directional		UMETA(DisplayName = "Directional Gravity")
	};

}

/**
 * 
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SHOOTERGAME_API UBaseGravityComponent : public UStaticMeshComponent
{
	GENERATED_BODY()
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Sets default values for this component's properties
	UBaseGravityComponent();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	virtual FVector CalculateGravity(FVector WorldPosition);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsAdditive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasFalloff;

	// Only used with point gravity
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsInverted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float GravityStrength;

	// USAGES: 0 is highest priority
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int Priority;

	// Not used with point gravity
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector DirectionalGravityDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EGravityFieldType::Type> GravityFieldType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"), Category = "Gravity")
	float Range;
};
