// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DoRunGameMode.h"
#include "DoRunCharacter.h"

ADoRunGameMode::ADoRunGameMode()
{
	// Set default pawn class to our character
	DefaultPawnClass = ADoRunCharacter::StaticClass();	
}
