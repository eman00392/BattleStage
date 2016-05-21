// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/PlayerController.h"
#include "BSPlayerController.generated.h"

class ABSCharacter;

/**
 * 
 */
UCLASS()
class BATTLESTAGE_API ABSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABSPlayerController();

	UFUNCTION(Client, Unreliable)
	void ClientHearSound(USoundBase* Sound, AActor* SourceActor, const FVector_NetQuantize SoundLocation) const;

	/** Handles moving forward/backward */
	void OnMoveForward(float Val);

	/** Handles stafing movement, left and right */
	void OnMoveRight(float Val);

	/**
	* Called via input to turn at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	void OnTurnAtRate(float Rate);

	/**
	* Called via input to look up at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired look rate
	*/
	void OnLookUpRate(float Rate);

	void OnEquip();

	void OnFireRight();

protected:
	virtual void ClientHearSound_Implementation(USoundBase* Sound, AActor* SourceActor, const FVector_NetQuantize SoundLocation) const;

protected:
	/* The base turn rate of the player **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Controller)
	float BaseTurnRate;

	/* The base look rate of the player **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Controller)
	float BaseLookRate;

private:
	ABSCharacter* BSCharacter; //< If valid, the owning BSCharacter
	
	//-----------------------------------------------------------------
	// Begin AController Interface
	//-----------------------------------------------------------------
public:
	virtual void SetPawn(APawn* InPawn) override;
	virtual void BeginPlay() override;
protected:
	virtual void SetupInputComponent() override;
	//-----------------------------------------------------------------
	// End AController Interface
	//-----------------------------------------------------------------
};
