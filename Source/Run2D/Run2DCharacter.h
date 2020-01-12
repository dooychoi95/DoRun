// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperCharacter.h"
#include "Run2DCharacter.generated.h"

UENUM()
enum class ECharacterState : uint8
{
	Stop,
	Run,
	Jump,
	Fly,
	Falling,
};

class UTextRenderComponent;

/**
 * This class is the default character for Run2D, and it is responsible for all
 * physical interaction between the player and the world.
 *
 * The capsule component (inherited from ACharacter) handles collision with the world
 * The CharacterMovementComponent (inherited from ACharacter) handles movement of the collision capsule
 * The Sprite component (inherited from APaperCharacter) handles the visuals
 */
UCLASS(config=Game)
class ARun2DCharacter : public APaperCharacter
{
	GENERATED_BODY()

	/** Side view camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera, meta=(AllowPrivateAccess="true"))
	class UCameraComponent* SideViewCameraComponent;

	/** Camera boom positioning the camera beside the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	UTextRenderComponent* TextComponent;
	virtual void Tick(float DeltaSeconds) override;

protected:
	// The animation to play while running around
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Animations)
	class UPaperFlipbook* RunningAnimation;

	// The animation to play while idle (standing still)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Animations)
	class UPaperFlipbook* IdleAnimation;

	/** Called for side to side input */
	void MoveRight(float Value);

	void UpdateCharacter();

	/** Handle touch inputs. */
	void TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location);

	/** Handle touch stop event. */
	void TouchStopped(const ETouchIndex::Type FingerIndex, const FVector Location);

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	ARun2DCharacter();

	/** Returns SideViewCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetSideViewCameraComponent() const { return SideViewCameraComponent; }
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	//~~ Begin Character Interface
	virtual void Jump();
	virtual void StopJumping();
	//~~ End Character Interface

	UPROPERTY(EditAnywhere, Category = "DoRun | CharacterMove")
	bool bForceRightMove;

	UPROPERTY(EditAnywhere, Category = "DoRun | CharacterMove")
	float ForceRightMoveValue;

private:
	/** ĳ���� ���� String ��ȯ */
	const FString GetCharacterStateToString(ECharacterState InState) const;

	/** ĳ���� ���� ������Ʈ */
	void SetCharacterState(const ECharacterState NewState);

	/** ĳ���� ���� ���� ���� �� ȣ��� */
	void OnChangeCharacterState(const ECharacterState NewState);

	/** ĳ���� ���� ���� ���� �� �ִϸ��̼� ���� */
	void UpdateAnimation(const ECharacterState NewState);

	/** ĳ���� ���� ���� */
	bool CanFly() const;

	/** ĳ���� ���� */
	ECharacterState CurrntState;

	/** ĳ���� ���� �Է� (�ٴڿ� ��� �� �ʱ�ȭ) */
	int32 JumpInputCount;
};
