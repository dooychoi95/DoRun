// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Run2DGameMode.h"
#include "Run2DCharacter.h"

ARun2DGameMode::ARun2DGameMode()
{
	// Set default pawn class to our character
	DefaultPawnClass = ARun2DCharacter::StaticClass();	
}
