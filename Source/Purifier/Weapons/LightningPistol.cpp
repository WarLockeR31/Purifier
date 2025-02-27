// Fill out your copyright notice in the Description page of Project Settings.


#include "LightningPistol.h"
#include "Projectiles/ProjectileBase.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Components/SphereComponent.h"

ALightningPistol::ALightningPistol()
{
    PrimaryFireMode = EFireMode::Raycast;  
    SecondaryFireMode = EFireMode::Projectile;  
}