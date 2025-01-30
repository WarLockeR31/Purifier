// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Dashable.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UDashable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class PURIFIER_API IDashable
{
	GENERATED_BODY()

public:
	// Метод для обработки событий рывка
	virtual void OnDashStart() = 0;
	virtual void OnDashEnd() = 0;

	virtual FVector GetMoveDirection() const = 0;  // Метод для получения направления рывка.
	virtual FVector2D GetInputDirection() const = 0;
};
