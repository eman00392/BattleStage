// Fill out your copyright notice in the Description page of Project Settings.

#include "BattleStage.h"
#include "BSHUD.h"


void ABSHUD::NotifyWeaponHit()
{
	OnNotifyWeaponHit.Broadcast();
}
