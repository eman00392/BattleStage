// Fill out your copyright notice in the Description page of Project Settings.

#include "BattleStage.h"
#include "Weapons/BSInstantShot.h"
#include "Weapons/BSWeapon.h"
#include "Effects/BSImpactEffect.h"
#include "UnrealNetwork.h"

static const float MAX_SHOT_RANGE = 10000.f;

void UBSInstantShot::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UBSInstantShot, ShotRep, COND_SkipOwner);
}

bool UBSInstantShot::GetShotData(FShotData& OutShotData) const
{
	const ABSWeapon* const Weapon = GetWeapon();
	OutShotData.Start = Weapon->GetCameraAimLocation();

	// Get a random weapon spread for the shot
	const int32 RandomSeed = FMath::Rand();
	FRandomStream SpreadStream(RandomSeed);
	const float BaseSpread = FMath::DegreesToRadians(Weapon->GetCurrentSpread());

	// Use spread to offset shot
	const FVector TrueAimDirection = Weapon->GetFireRotation().Vector();
	OutShotData.Direction = SpreadStream.VRandCone(TrueAimDirection, BaseSpread);

	const FVector FireEnd = OutShotData.Start + OutShotData.Direction * MAX_SHOT_RANGE;

	OutShotData.Impact = WeaponTrace(OutShotData.Start, FireEnd);
	OutShotData.bImpactNeeded = OutShotData.Impact.bBlockingHit ? true : false;

	return true;
}

void UBSInstantShot::PreInvokeShot(const FShotData& ShotData)
{
	// Play hit locally
	const FVector Target = (ShotData.bImpactNeeded) ? ShotData.Impact.ImpactPoint : ShotData.Start + ShotData.Direction * MAX_SHOT_RANGE;
	SimulateFire(Target);
}

void UBSInstantShot::InvokeShot(const FShotData& ShotData)
{
	if (ShotData.bImpactNeeded)
	{
		ProcessHit(ShotData);
	}
	else
	{		
		ProcessMiss(ShotData);
	}	
}

void UBSInstantShot::PlayTrailEffects(const FVector& Start, const FVector& End) const
{
	if (TrailFX)
	{		
		const FVector DirectionVector = (End - Start);

		UParticleSystemComponent* TrailComponent = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TrailFX, Start, DirectionVector.GetUnsafeNormal().Rotation());
		if (TrailEndParam != NAME_None)
		{
			TrailComponent->SetFloatParameter(TrailEndParam, DirectionVector.Size());
		}
	}
}

void UBSInstantShot::PlayImpactEffects(const FHitResult& Hit) const
{
	if (ImpactEffect)
	{
		const UBSImpactEffect* const EffectObject = ImpactEffect->GetDefaultObject<UBSImpactEffect>();
		EffectObject->SpawnEffect(GetWorld(), Hit);
	}
}

void UBSInstantShot::RespondValidHit(const FShotData& ShotData)
{
	ABSWeapon* const Weapon = GetWeapon();
	const FVector End = (ShotData.bImpactNeeded) ? ShotData.Impact.ImpactPoint : ShotData.Start + ShotData.Direction * MAX_SHOT_RANGE;

	// #bstodo Do damage on server
	if (Weapon->HasAuthority())
	{
		if (ShotData.bImpactNeeded && ShotData.Impact.Actor.IsValid())
		{
			AActor& HitActor = *ShotData.Impact.Actor;

			const float BaseDamage = Weapon->GetWeaponStats().BaseDamage;
			const FPointDamageEvent DamageEvent(BaseDamage, ShotData.Impact, -ShotData.Direction, DamageType);

			HitActor.TakeDamage(BaseDamage, DamageEvent, Weapon->GetInstigatorController(), Weapon->GetCharacter());
		}

		// Simulate on remotes
		ShotRep.Target = End;
		ShotRep.FireToggle = !ShotRep.FireToggle;
	}

	// Play local effects
	if (Weapon->GetNetMode() != NM_DedicatedServer)
	{
		SimulateFire(End);
	}
}

void UBSInstantShot::SimulateFire(const FVector& Target) const
{
	ABSWeapon* const Weapon = GetWeapon();

	const FVector Start = Weapon->GetFireLocation();
	FHitResult Impact = WeaponTrace(Start, Target);

	if (Impact.bBlockingHit)
		PlayImpactEffects(Impact);

	PlayTrailEffects(Start, Target);
}

FHitResult UBSInstantShot::WeaponTrace(const FVector& Start, const FVector& End) const
{
	ABSWeapon* const Weapon = GetWeapon();

	FHitResult Impact;
	const FCollisionQueryParams QueryParams(NAME_None, false, Weapon);
	GetWorld()->LineTraceSingleByChannel(Impact, Start, End, WEAPON_CHANNEL, QueryParams);

	return Impact;
	
}

void UBSInstantShot::OnRep_ShotRep()
{
	SimulateFire(ShotRep.Target);
}

void UBSInstantShot::ProcessHit(const FShotData& ShotData)
{
	// #bstodo Will need to verify a valid hit on the server
	RespondValidHit(ShotData);
}

void UBSInstantShot::ProcessMiss(const FShotData& ShotData)
{
	RespondValidHit(ShotData);
}
