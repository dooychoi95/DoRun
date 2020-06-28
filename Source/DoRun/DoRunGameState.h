#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "DoRunGameState.generated.h"

UCLASS()
class ADoRunGameState : public AGameState
{
	GENERATED_BODY()

public:

protected:
	virtual void HandleMatchHasStarted();
	virtual void HandleMatchHasEnded();
};
