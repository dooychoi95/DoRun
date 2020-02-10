// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PaperCharacter.h"
#include "DoRunCharacter.generated.h"

UENUM()
enum class ECharacterState : uint8
{
	Stop,
	Run,
	Jump,
	Fly,
	Falling,
	Sliding,
	Max,
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
class ADoRunCharacter : public APaperCharacter
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Animations)
	class UPaperFlipbook* IdleAnimation;

	// The animation to play while running around
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Animations)
	class UPaperFlipbook* RunningAnimation;

	// The animation to play while idle (standing still)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Animations)
	class UPaperFlipbook* JumpAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Animations)
	class UPaperFlipbook* SlidingAnimation;

	/** Called for side to side input */
	void MoveRight(float Value);

	void UpdateCharacterMove(float DeltaSeconds);

	void UpdateCameraDetach(float DeltaSeconds);

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	ADoRunCharacter();

	/** Returns SideViewCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetSideViewCameraComponent() const { return SideViewCameraComponent; }
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	//~~ Begin ACharacter Interface
	virtual void BeginPlay() override;
	virtual void Jump();
	virtual void Falling();
	virtual void Landed(const FHitResult& Hit);
	//~~ End ACharacter Interface

	/** �̲������� */
	void PressedSliding();
	void ReleasedSliding();
	void ProssceSliding(bool bPressed);

	UPROPERTY(EditAnywhere, Category = "DoRun | CharacterMove")
	float ForceRightMoveValue;

	UPROPERTY(EditAnywhere, Category = "DoRun | CharacterMove")
	float RightMoveDuringTime;

	/** ī�޶� ���� ���� �ð� �ش� �ð� �� ĳ���� ���� �̵��� �ݴ�� �Է� �˴ϴ�.*/
	UPROPERTY(EditAnywhere, Category = "DoRun | CameraMove")
	float CameraDetachDuringTime;

private:
	/** ĳ���� ���� String ��ȯ */
	const FString GetCharacterStateToString(ECharacterState InState) const;

	/** ĳ���� ���� ������Ʈ */
	void SetCharacterState(const ECharacterState NewState);

	/** ĳ���� ���� ���� �������� */
	const ECharacterState GetCharacterState() const;

	/** ĳ���� ���� ���� ���� �� ȣ��� */
	void OnChangeCharacterState(const ECharacterState NewState);

	/** ĳ���� ���� ���� ���� �� �ִϸ��̼� ���� */
	void UpdateAnimationToStateChange(const ECharacterState NewState);

	/** ĳ���� �����̵� ���� ���� ���� */
	bool CanProssceSliding() const;

	/** ĳ���� �����̵� ���� ���� ���� */
	bool ShouldSlidingToCharacterLanded() const;

	/** ĳ���� ���ڸ� �̵� ���� �ð� �ʱ�ȭ */
	void ResetRightMoveDuringTime();

	/** ĳ���� ���ڸ� �̵� ���� �Ϸ� */
	void OnFinishMoveRight();

	/** ĳ���� ���� */
	ECharacterState CurrntState;

	/** ĳ���� ���ڸ� �̵� ���� �ð� */
	float CachedRightMoveDuringTime;

	/** ĳ���� �����̵� Ű ��ǲ ���� */
	bool bPressedSliding;

	/** ī�޶� ���� �Ϸ� */
	bool bCameraFixed;
};
