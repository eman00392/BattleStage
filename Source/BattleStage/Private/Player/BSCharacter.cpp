// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BattleStage.h"
#include "BSCharacter.h"

#include "Animation/AnimInstance.h"
#include "GameFramework/InputSettings.h"

#include "BSProjectile.h"
#include "BSWeapon.h"
#include "BSCharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

ABSCharacter::ABSCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBSCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// First person camera
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->RelativeLocation = FVector(0, 2.56f, 74.f);
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());

	// Setup first person mesh.
	// Only owner can see and does not replicate.
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCamera);
	FirstPersonMesh->SetCastShadow(true);
	FirstPersonMesh->bSelfShadowOnly = true;
	FirstPersonMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	FirstPersonMesh->bOnlyOwnerSee = true;
	FirstPersonMesh->bReceivesDecals = false;
	FirstPersonMesh->RelativeRotation = FRotator{ 0.f, -90.f, -5.f };
	FirstPersonMesh->RelativeLocation = FVector{ 0, -5.f, -130.f };

	// Setup character mesh.
	// Owner can not see and does replicate.
	USkeletalMeshComponent* Mesh = GetMesh();
	Mesh->SetupAttachment(GetCapsuleComponent());
	Mesh->bOwnerNoSee = true;
	Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	Mesh->bReceivesDecals = false;
	Mesh->RelativeLocation = FVector{ 0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() };
	Mesh->RelativeRotation = FRotator{ 0.f, -90.f, 0.f };

	bIsDying = false;
	bIsRunning = false;
	Health = 100;
	RunningMovementModifier = 1.5f;
	CrouchCameraSpeed = 500.f;
	
	WeaponEquippedSocket = TEXT("GripPoint");
}

void ABSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABSCharacter, Weapons, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ABSCharacter, ActiveWeaponSlot, COND_SkipOwner);
	DOREPLIFETIME(ABSCharacter, bIsDying);
	DOREPLIFETIME_CONDITION(ABSCharacter, bIsRunning, COND_SkipOwner);
	DOREPLIFETIME(ABSCharacter, Health);
	DOREPLIFETIME(ABSCharacter, ReceiveHitInfo);
}

float ABSCharacter::GetAimSpread() const
{
	if (auto Weapon = GetEquippedWeapon())
	{
		return Weapon->GetCurrentSpread();
	}		

	return 0.f;
}

float ABSCharacter::GetMovementModifier() const
{
	float Modifier = 1.0f;

	if (bIsRunning)
	{
		Modifier = RunningMovementModifier;
	}

	return Modifier;
}

void ABSCharacter::SetDisableActions(bool bIsDisabled)
{
	bIsActionsDisabled = bIsDisabled;

	if (bIsDisabled)
	{
		if (auto Weapon = GetEquippedWeapon())
		{
			Weapon->StopFire();
		}

		bIsRunning = false;
	}
}

void ABSCharacter::StartFire()
{
	auto Weapon = GetEquippedWeapon();
	if (Weapon && !bIsActionsDisabled)
	{
		if (bIsRunning)
		{
			SetRunning(false);
		}

		Weapon->StartFire();
	}
}

void ABSCharacter::StopFire()
{
	if (auto Weapon = GetEquippedWeapon())
	{
		Weapon->StopFire();
	}
}

bool ABSCharacter::IsRunning() const
{
	return bIsRunning;
}

bool ABSCharacter::CanRun() const
{
	return !GetMovementComponent()->IsFalling() && !bIsActionsDisabled;
}

void ABSCharacter::SetRunning(bool bNewRunning)
{
	if (bIsRunning != bNewRunning &&
		(!bNewRunning || CanRun()))
	{
		bIsRunning = bNewRunning;

		if (bNewRunning)
		{
			// No firing while running
			if (auto Weapon = GetEquippedWeapon())
			{
				Weapon->StopFire();
			}

			// Stop crouching before running
			UnCrouch();
		}

		if (!HasAuthority())
		{
			ServerSetRunning(bNewRunning);
		}
	}
}

void ABSCharacter::ServerSetRunning_Implementation(bool bNewRunning)
{
	SetRunning(bNewRunning);
}

bool ABSCharacter::ServerSetRunning_Validate(bool bNewRunning)
{
	return true;
}

void ABSCharacter::ToggleRunning()
{
	SetRunning(!bIsRunning);
}

void ABSCharacter::ReloadWeapon()
{
	auto Weapon = GetEquippedWeapon();
	if (!bIsActionsDisabled && Weapon)
	{
		Weapon->Reload();
	}		
}

const FReceiveHitInfo& ABSCharacter::GetLastHitInfo() const
{
	return ReceiveHitInfo;
}

USkeletalMeshComponent* ABSCharacter::GetActiveMesh() const
{
	return IsFirstPerson() ? FirstPersonMesh : GetThirdPersonMesh();
}

void ABSCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// If we are running, make sure our pending movement is forward. If not, stop running.
	if (bIsRunning && IsLocallyControlled())
	{
		const FVector Movement = GetLastMovementInputVector().GetSafeNormal2D();
		const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
		
		// Tolerance is half 30 degrees in the forward direction		
		const float Tolerance = FMath::Cos(PI / 6.0f);

		if (FVector::DotProduct(Movement, Forward) < Tolerance)
		{
			SetRunning(false);
		}
	}

	UpdateViewTarget(DeltaSeconds);
}

void ABSCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocallyControlled())
	{
		EquipWeapon(ActiveWeaponSlot);
	}			
}

float ABSCharacter::PlayAnimMontage(class UAnimMontage* AnimMontage, float InPlayRate /*= 1.f*/, FName StartSectionName /*= NAME_None*/)
{
	float Duration = 0.f;

	if (AnimMontage)
	{
		USkeletalMeshComponent* Mesh = GetActiveMesh();
		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
		Duration = AnimInstance->Montage_Play(AnimMontage, InPlayRate);

		if (Duration > 0 && StartSectionName != NAME_None)
		{
			AnimInstance->Montage_JumpToSection(StartSectionName, AnimMontage);
		}
	}

	return Duration;
}

void ABSCharacter::StopAnimMontage(class UAnimMontage* AnimMontage /*= NULL*/)
{
	USkeletalMeshComponent* ActiveMesh = GetActiveMesh();
	UAnimInstance * AnimInstance = (ActiveMesh) ? ActiveMesh->GetAnimInstance() : nullptr;
	UAnimMontage * MontageToStop = (AnimMontage) ? AnimMontage : GetCurrentMontage();
	bool bShouldStopMontage = AnimInstance && MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop);

	if (bShouldStopMontage)
	{
		AnimInstance->Montage_Stop(MontageToStop->BlendOut.GetBlendTime(), MontageToStop);
	}
}

void ABSCharacter::TurnOff()
{
	StopFire();

	if (GetNetMode() != NM_DedicatedServer && FirstPersonMesh != NULL)
	{
		FirstPersonMesh->bPauseAnims = true;
		if (FirstPersonMesh->IsSimulatingPhysics())
		{
			FirstPersonMesh->bBlendPhysics = true;
			FirstPersonMesh->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipAllBones;
		}
	}

	Super::TurnOff();
}

void ABSCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (HasAuthority())
	{
		CreateDefaultLoadout();
	}

	UpdateMeshVisibility();
}

void ABSCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	UpdateMeshVisibility();
}

void ABSCharacter::Crouch(bool bClientSimulation /*= false*/)
{
	SetRunning(false);

	Super::Crouch(bClientSimulation);
}

void ABSCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// Save eye height so we can lerp to new location following start crouch.
	FVector NewCameraLocation = FirstPersonCamera->RelativeLocation;
	NewCameraLocation.Z += HalfHeightAdjust;

	LastEyeHeight = NewCameraLocation.Z;
	FirstPersonCamera->SetRelativeLocation(NewCameraLocation);
}

void ABSCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// Save eye height so we can lerp to new location following start crouch.
	FVector NewCameraLocation = FirstPersonCamera->RelativeLocation;
	NewCameraLocation.Z -= HalfHeightAdjust;

	LastEyeHeight = NewCameraLocation.Z;
	FirstPersonCamera->SetRelativeLocation(NewCameraLocation);
}

void ABSCharacter::Jump()
{
	SetRunning(false);
	
	Super::Jump();
}

void ABSCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	++JumpCounter;
}

bool ABSCharacter::CanJumpInternal_Implementation() const
{
	const bool bCanJump = Super::CanJumpInternal_Implementation();
	return bCanJump && JumpCounter < MAX_JUMPS;
}

void ABSCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	JumpCounter = 0;
}

bool ABSCharacter::ShouldTakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) const
{
	return CanDie() && Super::ShouldTakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
}

void ABSCharacter::PostInitProperties()
{
	Super::PostInitProperties();

	LastEyeHeight = BaseEyeHeight;
}

float ABSCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = 0;

	if (Health > 0)
	{
		ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);

		TakeHit(ActualDamage, DamageEvent, EventInstigator, DamageCauser);

		if (ActualDamage > 0)
		{
			Health -= ActualDamage;

			if (Health <= 0)
			{
				Die(DamageEvent, EventInstigator);
			}
		}
	}
	
	return ActualDamage;
}

bool ABSCharacter::CanDie() const
{
	return Health > 0 && !IsPendingKill();
}

void ABSCharacter::SwapWeapon()
{
	if (ActiveWeaponSlot == EWeaponSlot::Primary)
	{
		EquipWeapon(EWeaponSlot::Secondary);
	}
	else
	{
		EquipWeapon(EWeaponSlot::Primary);
	}
}

void ABSCharacter::SetupPlayerInputComponent(class UInputComponent* InInputComponent)
{
	Super::SetupPlayerInputComponent(InInputComponent);

	InInputComponent->BindAction("SwapWeapon", IE_Pressed, this, &ABSCharacter::SwapWeapon);
}

void ABSCharacter::Die(FDamageEvent const& DamageEvent, AController* Killer)
{
	bIsDying = true;
	bReplicateMovement = false;
	TearOff();

	ABSGameMode* GameMode = Cast<ABSGameMode>(GetWorld()->GetAuthGameMode());
	GameMode->ScoreKill(Killer, GetController());

	// Detach controller, character will be destroyed soon
	DetachFromControllerPendingDestroy();

	for (int32 i = 0; i < (int32)EWeaponSlot::Max; ++i)
	{
		if (Weapons[i])
		{
			Weapons[i]->TearOff();			
		}
	}

	OnRep_IsDying();
}

void ABSCharacter::OnRep_IsDying()
{
	UpdateMeshVisibility();

	OnDeath();
}

void ABSCharacter::OnRep_Weapons()
{
	// Can't guarantee that the owner gets replicated with the weapon, so set it here.
	// (At least as far as I know)
	for (int32 i = 0; i < (int32)EWeaponSlot::Max; ++i)
	{
		if (Weapons[i])
		{
			Weapons[i]->SetOwner(this);
		}		
	}

	if (IsLocallyControlled())
	{
		EquipWeapon(ActiveWeaponSlot);
	}

	if (auto Weapon = GetEquippedWeapon())
	{
		Weapon->AttachToOwner(); // Need to make sure we are properly attach regardless of ownership.
	}
}

void ABSCharacter::OnRep_WeaponSlot()
{
	
}

void ABSCharacter::OnRep_Owner()
{
	Super::OnRep_Owner();
}

void ABSCharacter::UpdateViewTarget(const float DeltaSeconds)
{
	if (FMath::Abs(LastEyeHeight - BaseEyeHeight) > 0.01f)
	{
		// Get lerp alpha for camera from LastEyHeight to BaseEyeHeight
		const float Distance = BaseEyeHeight - LastEyeHeight;
		const float DeltaAlpha = CrouchCameraSpeed / FMath::Abs(Distance) * DeltaSeconds;

		// Get new camera height
		FVector Location = FirstPersonCamera->RelativeLocation;
		Location.Z = FMath::Lerp(LastEyeHeight, BaseEyeHeight, FMath::Min(DeltaAlpha, 1.0f));

		FirstPersonCamera->SetRelativeLocation(Location);
		LastEyeHeight = Location.Z;
	}
}

void ABSCharacter::TakeHit(const float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	SetReceiveHitInfo(Damage, DamageEvent, DamageCauser);
	
	if(GetNetMode() != NM_DedicatedServer)
		OnReceiveHit();

	ApplyDamageMomentum(Damage, DamageEvent, EventInstigator ? EventInstigator->GetPawn() : nullptr, DamageCauser);

	if (DamageCauser)
	{
		// Notify received damage on controller
		if (ABSPlayerController* DamagedController = Cast<ABSPlayerController>(GetController()))
		{
			DamagedController->NotifyReceivedDamage(DamageCauser->GetActorLocation());
		}

		// Notify hit if for controller that caused damage		
		if (ABSPlayerController* InstigatorController = Cast<ABSPlayerController>(EventInstigator))
		{
			InstigatorController->NotifyWeaponHit();
		}
	}
}

void ABSCharacter::SetReceiveHitInfo(const float Damage, const FDamageEvent& DamageEvent, AActor* DamageCauser)
{
	// Set hit bone based on damage event type
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamageEvent = static_cast<const FPointDamageEvent&>(DamageEvent);

		FHitResult Hit;
		FVector UnusedImpulseDirection;
		PointDamageEvent.GetBestHitInfo(this, DamageCauser, Hit, UnusedImpulseDirection);

		ReceiveHitInfo.HitLocation = Hit.ImpactPoint;
		ReceiveHitInfo.HitBone = Hit.BoneName;
		ReceiveHitInfo.HitDirection = PointDamageEvent.ShotDirection;
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		// For radial, always set spine as bone
		ReceiveHitInfo.HitBone = RadialDamageImpactBone;

		const FRadialDamageEvent& RadialDamageEvent = static_cast<const FRadialDamageEvent&>(DamageEvent);
		ReceiveHitInfo.HitLocation = RadialDamageEvent.Origin;
		ReceiveHitInfo.HitDirection = (GetActorLocation() - RadialDamageEvent.Origin).GetSafeNormal();
	}

	ReceiveHitInfo.DamageCauser = DamageCauser;
	ReceiveHitInfo.Damage = ReceiveHitInfo.Damage;

	ReceiveHitInfo.ForceReplication();
}

void ABSCharacter::CreateDefaultLoadout()
{
	for (int32 i = 0; i < (int32)EWeaponSlot::Max; ++i)
	{
		if (auto WeaponClass = DefaultWeapons[i])
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Instigator = this;
			SpawnParams.Owner = this;
			Weapons[i] = GetWorld()->SpawnActor<ABSWeapon>(WeaponClass, SpawnParams);
		}
	}
}

void ABSCharacter::OnReceiveHit_Implementation()
{

}

void ABSCharacter::OnDeath_Implementation()
{
	for (int32 i = 0; i < (int32)EWeaponSlot::Max; ++i)
	{
		if (Weapons[i])
		{
			Weapons[i]->StopFire();
			Weapons[i]->SetLifeSpan(5.f);
		}
	}

	// Stop any existing montages
	StopAnimMontage();

	// Set collision properties
	static FName RagdollProfile{ TEXT("Ragdoll") };
	GetMesh()->SetCollisionProfileName(RagdollProfile);

	SetActorEnableCollision(true);

	// Play optional death anim and then active ragdoll
	if (DeathAnim)
	{
		const float DeathAnimLength = PlayAnimMontage(DeathAnim);
		
		// Activate ragdoll after death anim starts. Give it a small amount of time to get into the death anim pose.
		FTimerHandle RagdollTimer;
		GetWorldTimerManager().SetTimer(RagdollTimer, this, &ABSCharacter::EnableRagdollPhysics, FMath::Min(.2f, DeathAnimLength));
	}
	else
	{
		EnableRagdollPhysics();
	}	

	// Disable capsule
	static FName NoCollisionProfile{ TEXT("NoCollision") };
	GetCapsuleComponent()->SetCollisionProfileName(NoCollisionProfile);
}

void ABSCharacter::EnableRagdollPhysics()
{
	if (GetMesh()->GetPhysicsAsset())
	{
		// Set all bodies of the mesh component to simulate
		GetMesh()->SetAllBodiesSimulatePhysics(true);
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->WakeAllRigidBodies();

		GetMesh()->bBlendPhysics = true;
	}

	// Stop and disable any character movement
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);

	SetLifeSpan(10.0f);
}

bool ABSCharacter::IsFirstPerson() const
{
	return !bIsDying && IsLocallyControlled();
}

void ABSCharacter::UpdateMeshVisibility()
{
	const bool bIsFirstPerson = IsFirstPerson();

	GetMesh()->SetOwnerNoSee(bIsFirstPerson);
	GetMesh()->MeshComponentUpdateFlag = bIsFirstPerson ? EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered : EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;

	FirstPersonMesh->SetOwnerNoSee(!bIsFirstPerson);
	FirstPersonMesh->MeshComponentUpdateFlag = bIsFirstPerson ? EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones : EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
}

void ABSCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	SetOwner(NewController);

	if (auto Weapon = GetEquippedWeapon())
	{
		Weapon->AttachToOwner();
	}

	UpdateMeshVisibility();
}

void ABSCharacter::UnPossessed()
{
	Super::UnPossessed();

	UpdateMeshVisibility();
}

void ABSCharacter::EquipWeapon(const EWeaponSlot InWeaponSlot)
{
	if (auto CurrentWeapon = GetEquippedWeapon())
	{
		CurrentWeapon->Unequip();
	}
	
	ActiveWeaponSlot = InWeaponSlot;

	if (auto NewWeapon = Weapons[(int32)InWeaponSlot])
	{
		NewWeapon->Equip();
	}

	if (!HasAuthority())
	{
		ServerEquipWeapon(InWeaponSlot);
	}
}

void ABSCharacter::ServerEquipWeapon_Implementation(const EWeaponSlot InWeaponSlot)
{
	EquipWeapon(InWeaponSlot);
}

bool ABSCharacter::ServerEquipWeapon_Validate(const EWeaponSlot InWeaponSlot)
{
	return InWeaponSlot == EWeaponSlot::Primary || InWeaponSlot == EWeaponSlot::Secondary;
}