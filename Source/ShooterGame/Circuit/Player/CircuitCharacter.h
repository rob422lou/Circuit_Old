// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/ShooterCharacter.h"
#include "Circuit/CircuitHelper.h"
#include "Circuit/Components/CircuitSkeletalMeshComponent.h"
#include "Circuit/Components/CircuitCapsuleComponent.h"
#include "Circuit/Components/UsableComponent.h"
#include "CircuitCharacter.generated.h"

/**
 * 
 */
UCLASS()
class SHOOTERGAME_API ACircuitCharacter : public AShooterCharacter
{
	GENERATED_BODY()

public:
	/** Default UObject constructor. Needed to allow character to use PerdixCharacterMovementComponent by default. */
	ACircuitCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaSeconds) override;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	//////////////////////////////////////////////////////////////////////////
	// Use

	/* True only in first frame when focused on new usable actor. */
	bool bHasNewFocus;

	// returns actor we are currently holding "Use" on
	UUsableComponent* CurrentlyUsed;

	/* Actor derived from UsableActor currently in center-view. */
	AActor* FocusedUsableActor;
	UUsableComponent* FocusedUsableComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxUseDistance;

	AActor* GetActorInView();

	UUsableComponent* GetUsableComponentInView();

	/** player pressed use action */
	UFUNCTION(BlueprintNativeEvent, Category = PlayerAbility)
	void OnStartUse();

	UFUNCTION(WithValidation, Server, Reliable)
	void ServerOnStartUse();

	/** player released use action */
	UFUNCTION(BlueprintNativeEvent, Category = PlayerAbility)
	void OnStopUse();

	UFUNCTION(WithValidation, Server, Reliable)
	void ServerOnStopUse();

	//////////////////////////////////////////////////////////////////////////
	// Weapon usage

	UFUNCTION(BlueprintNativeEvent, Category = PlayerAbility)
	void OnStartReload();

	UFUNCTION(reliable, server, WithValidation)
	void ServerOnStartReload();

	UFUNCTION(BlueprintNativeEvent, Category = PlayerAbility)
	void OnStopReload();

	UFUNCTION(reliable, server, WithValidation)
	void ServerOnStopReload();

	/** player pressed targeting action (Changed ShooterCharacter.h OnStartTargeting to virtual) */
	UFUNCTION(BlueprintNativeEvent, Category = PlayerAbility)
	void OnStartTargeting() override;

	/* When targeting keypress first happens, before "SetTargeting()" */
	UFUNCTION(reliable, server, WithValidation)
	void ServerOnStartTargeting();

	/** player released targeting action */
	UFUNCTION(BlueprintNativeEvent, Category = PlayerAbility)
	void OnStopTargeting() override;

	/* When targeting keypress ends, before "SetTargeting()" */
	UFUNCTION(reliable, server, WithValidation)
	void ServerOnStopTargeting();

	//////////////////////////////////////////////////////////////////////////
	// Damage & death
 


	//////////////////////////////////////////////////////////////////////////
	// Others


	class UCustomGravityComponent* GravityComponent;

	/** player noclip action */
	UFUNCTION(BlueprintNativeEvent, Category = PlayerAbility)
	void OnNoclip();
	
	UFUNCTION(WithValidation, Server, Reliable)
	void ServerOnNoclip();

	//////////////////////////////////////////////////////////////////////////
// Renderer Functions

	UFUNCTION(BlueprintCallable, Category = "Circuit|Rendering")
	void SetOutlineColor(EStencilColor OutlineColor, AActor* highlightActor);
};
