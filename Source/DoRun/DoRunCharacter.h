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

	/** Called for side to side input */
	void MoveRight(float Value);

	void UpdateCharacterMove(float DeltaSeconds);

	/** Handle touch inputs. */
	void TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location);

	/** Handle touch stop event. */
	void TouchStopped(const ETouchIndex::Type FingerIndex, const FVector Location);

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
	virtual void StopJumping();
	void OnLanded(const FHitResult& Hit);
	//~~ End ACharacter Interface

	UPROPERTY(EditAnywhere, Category = "DoRun | CharacterMove")
	float ForceRightMoveValue;

	UPROPERTY(EditAnywhere, Category = "DoRun | CharacterMove")
	float RightMoveDuringTime;

private:
	/** 캐릭터 상태 String 변환 */
	const FString GetCharacterStateToString(ECharacterState InState) const;

	/** 캐릭터 상태 업데이트 */
	void SetCharacterState(const ECharacterState NewState);

	/** 캐릭터 상태 변경 성공 시 호출됨 */
	void OnChangeCharacterState(const ECharacterState NewState);

	/** 캐릭터 상태 변경 성공 시 애니메이션 변경 */
	void UpdateAnimation(const ECharacterState NewState);

	/** 캐릭터 날기 가능 */
	bool CanFly() const;

	/** 캐릭터 제자리 이동 도달 시간 초기화 */
	void ResetRightMoveDuringTime();

	/** 캐릭터 상태 */
	ECharacterState CurrntState;

	/** 캐릭터 제자리 이동 도달 시간 */
	float CachedRightMoveDuringTime;
};
