// Fill out your copyright notice in the Description page of Project Settings.

#include "BattleStage.h"
#include "BSPlayerController.h"

#include "Camera/PlayerCameraManager.h"

#include "BSCharacter.h"
#include "BSWeapon.h"
#include "BSHUD.h"
#include "BSUserWidget.h"
#include "BSGameInstance.h"
#include "BSGameState.h"

ABSPlayerController::ABSPlayerController()
	: Super()
	, BSCharacter(nullptr)
	, BaseTurnRate(45.0f)
	, BaseLookRate(45.0f)
{

}

void ABSPlayerController::UnFreeze()
{
	ServerRestartPlayer();
}

void ABSPlayerController::PawnPendingDestroy(APawn* inPawn)
{
	APawn* const Pawn = GetPawn();
	FVector DeathLocation = Pawn->GetActorLocation() + FVector{ 0, 0, 300 };
	FRotator CameraRotation{ -90.f, 0, 0 };

	// #bstodo Get better camera view and location

	Super::PawnPendingDestroy(inPawn);

	ClientSetSpectatorCamera(DeathLocation, CameraRotation);
}

void ABSPlayerController::NotifyWeaponHit()
{
	ClientNotifyWeaponHit();
}

void ABSPlayerController::ClientNotifyWeaponHit_Implementation()
{
	if (ABSHUD* HUD = Cast<ABSHUD>(GetHUD()))
	{
		HUD->NotifyWeaponHit();
	}
}

void ABSPlayerController::NotifyReceivedDamage(const FVector& SourcePosition)
{
	ClientNotifyReceivedDamage(SourcePosition);
}

void ABSPlayerController::ToggleInGameMenu()
{
	const bool bShowMenu = (InGameMenuWidget && InGameMenuWidget->IsInViewport()) ? false : true;
	ShowInGameMenu(bShowMenu);
}

void ABSPlayerController::ShowInGameMenu(const bool bShowMenu)
{
	if (bShowMenu && InGameMenuClass)
	{
		if (InGameMenuWidget == nullptr)
		{
			InGameMenuWidget = CreateWidget<UBSUserWidget>(this, InGameMenuClass);
		}
		
		if (!InGameMenuWidget->IsInViewport())
		{
			InGameMenuWidget->AddToViewport();
		}

		SetCinematicMode(true, true, true);
		
		// Setup input for in-game menu
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewport(true);
		InputMode.SetWidgetToFocus(InGameMenuWidget->TakeWidget());
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
	else if (InGameMenuWidget && InGameMenuWidget->IsInViewport())
	{
		InGameMenuWidget->RemoveFromParent();		
		InGameMenuWidget = nullptr;

		SetCinematicMode(false, true, true);

		// Set input back to game only
		SetInputMode(FInputModeGameOnly{});
		bShowMouseCursor = false;
	}
}

void ABSPlayerController::ToggleGameScoreboard()
{
	if (ABSHUD* HUD = Cast<ABSHUD>(GetHUD()))
	{
		HUD->ShowGameScoreboard(!HUD->IsGameScoreboardUp());
	}
}

void ABSPlayerController::HandleReturnToMainMenu()
{
	if (ABSGameState* GameState = Cast<ABSGameState>(GetWorld()->GetGameState()))
	{
		GameState->QuitGameAndReturnToMainMenu();
	}
	else
	{
		ClientReturnToMainMenu_Implementation(FString{});
	}
}

void ABSPlayerController::ClientReturnToMainMenu_Implementation(const FString& ReturnReason)
{
	Super::ClientReturnToMainMenu_Implementation(ReturnReason);

	UBSGameInstance* GameInstance = Cast<UBSGameInstance>(GetGameInstance());
	if (GameInstance)
	{
		GameInstance->GracefullyDestroyOnlineSession();
	}
}

void ABSPlayerController::ClientNotifyReceivedDamage_Implementation(const FVector& SourcePosition)
{
	if (ABSHUD* HUD = Cast<ABSHUD>(GetHUD()))
	{
		HUD->NotifyReceivedDamage(SourcePosition);
	}

	if (HitCameraShake)
	{
		PlayerCameraManager->PlayCameraShake(HitCameraShake);
	}
}

void ABSPlayerController::ClientSetSpectatorCamera_Implementation(const FVector CameraLocation, const FRotator CameraRotation)
{
	SetInitialLocationAndRotation(CameraLocation, CameraRotation);
	SetViewTarget(this);
}

void ABSPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	BSCharacter = Cast<ABSCharacter>(InPawn);
}

void ABSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	check(InputComponent);

	InputComponent->BindAction("GameMenu", IE_Pressed, this, &ABSPlayerController::ToggleInGameMenu);
	InputComponent->BindAction("Scoreboard", IE_Pressed, this, &ABSPlayerController::ToggleGameScoreboard);

	InputComponent->BindAxis("MoveForward", this, &ABSPlayerController::OnMoveForward);
	InputComponent->BindAxis("MoveRight", this, &ABSPlayerController::OnMoveRight);

	InputComponent->BindAction("Jump", IE_Pressed, this, &ABSPlayerController::OnJump);
	InputComponent->BindAction("Sprint", IE_Pressed, this, &ABSPlayerController::OnStartSprint);
	InputComponent->BindAction("Sprint", IE_Released, this, &ABSPlayerController::OnStopSprint);
	InputComponent->BindAction("ToggleSprint", IE_Pressed, this, &ABSPlayerController::OnToggleSprint);
	InputComponent->BindAction("Crouch", IE_Pressed, this, &ABSPlayerController::OnCrouch);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	InputComponent->BindAxis("Turn", this, &ABSPlayerController::AddYawInput);
	InputComponent->BindAxis("TurnRate", this, &ABSPlayerController::OnTurnAtRate);

	InputComponent->BindAxis("LookUp", this, &ABSPlayerController::AddPitchInput);
	InputComponent->BindAxis("LookUpRate", this, &ABSPlayerController::OnLookUpRate);

	InputComponent->BindAction("Fire", IE_Pressed, this, &ABSPlayerController::OnStartFire);
	InputComponent->BindAction("Fire", IE_Released, this, &ABSPlayerController::OnStopFire);
	InputComponent->BindAction("Reload", IE_Pressed, this, &ABSPlayerController::OnReload);
}

void ABSPlayerController::ClientHearSound_Implementation(USoundBase* Sound, AActor* SourceActor, FVector_NetQuantize SoundLocation) const
{
	if (!Sound)
	{
		UE_LOG(BattleStage, Warning, TEXT("ABSPlayerController::ClientHearSound() is being sent a null sound."))
	}
	else
	{
		if (SourceActor && SoundLocation.IsZero())
		{
			UGameplayStatics::SpawnSoundAttached(Sound, SourceActor->GetRootComponent());
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, SoundLocation);
		}
	}
}

void ABSPlayerController::OnMoveForward(float Value)
{
	APawn* Pawn = GetPawn();
	if (Pawn && Value != 0.0f && !IsMoveInputIgnored())
	{
		Pawn->AddMovementInput(GetActorForwardVector().GetSafeNormal2D() , Value);
	}
}

void ABSPlayerController::OnMoveRight(float Value)
{
	APawn* Pawn = GetPawn();
	if (Pawn && Value != 0.0f && !IsMoveInputIgnored())
	{
		Pawn->AddMovementInput(GetActorRightVector().GetSafeNormal2D(), Value);
	}
}

void ABSPlayerController::OnJump()
{
	ACharacter* Character = GetCharacter();
	if (Character && !IsMoveInputIgnored())
	{
		Character->Jump();
	}
}

void ABSPlayerController::OnTurnAtRate(float Rate)
{
	if(!IsMoveInputIgnored())
		AddYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ABSPlayerController::OnLookUpRate(float Rate)
{
	if(!IsMoveInputIgnored())
		AddPitchInput(Rate * BaseLookRate * GetWorld()->GetDeltaSeconds());
}

void ABSPlayerController::OnStartFire()
{
	if (BSCharacter && !IsMoveInputIgnored())
	{
		BSCharacter->StartFire();
	}
}

void ABSPlayerController::OnStopFire()
{
	if (BSCharacter && !IsMoveInputIgnored())
	{
		BSCharacter->StopFire();
	}
}

void ABSPlayerController::OnReload()
{
	if (BSCharacter && !IsMoveInputIgnored())
	{
		BSCharacter->ReloadWeapon();
	}
}

void ABSPlayerController::OnStartSprint()
{
	if (BSCharacter && !IsMoveInputIgnored())
	{
		BSCharacter->SetRunning(true);
	}
}

void ABSPlayerController::OnToggleSprint()
{
	if (BSCharacter && !IsMoveInputIgnored())
	{
		BSCharacter->ToggleRunning();
	}
}

void ABSPlayerController::OnStopSprint()
{
	if (BSCharacter && !IsMoveInputIgnored())
	{
		BSCharacter->SetRunning(false);
	}
}

void ABSPlayerController::OnCrouch()
{
	if (BSCharacter && !IsMoveInputIgnored())
	{
		if (BSCharacter->bIsCrouched)
		{
			BSCharacter->UnCrouch();
		}
		else
		{
			BSCharacter->Crouch();
		}
	}
}