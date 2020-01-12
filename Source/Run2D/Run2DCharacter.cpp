// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Run2DCharacter.h"
#include "PaperFlipbookComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Camera/CameraComponent.h"

DEFINE_LOG_CATEGORY_STATIC(SideScrollerCharacter, Log, All);

//////////////////////////////////////////////////////////////////////////
// ARun2DCharacter
PRAGMA_DISABLE_OPTIMIZATION

ARun2DCharacter::ARun2DCharacter()
{
	// Use only Yaw from the controller and ignore the rest of the rotation.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Set the size of our collision capsule.
	GetCapsuleComponent()->SetCapsuleHalfHeight(96.0f);
	GetCapsuleComponent()->SetCapsuleRadius(40.0f);

	// Create a camera boom attached to the root (capsule)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 500.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 75.0f);
	CameraBoom->bAbsoluteRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->RelativeRotation = FRotator(0.0f, -90.0f, 0.0f);
	

	// Create an orthographic camera (no perspective) and attach it to the boom
	SideViewCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("SideViewCamera"));
	SideViewCameraComponent->ProjectionMode = ECameraProjectionMode::Orthographic;
	SideViewCameraComponent->OrthoWidth = 2048.0f;
	SideViewCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Prevent all automatic rotation behavior on the camera, character, and camera component
	CameraBoom->bAbsoluteRotation = true;
	SideViewCameraComponent->bUsePawnControlRotation = false;
	SideViewCameraComponent->bAutoActivate = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// Configure character movement
	GetCharacterMovement()->GravityScale = 2.0f;
	GetCharacterMovement()->AirControl = 0.80f;
	GetCharacterMovement()->JumpZVelocity = 1000.f;
	GetCharacterMovement()->GroundFriction = 3.0f;
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	GetCharacterMovement()->MaxFlySpeed = 600.0f;

	// Lock character motion onto the XZ plane, so the character can't move in or out of the screen
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, -1.0f, 0.0f));

	// Behave like a traditional 2D platformer character, with a flat bottom instead of a curved capsule bottom
	// Note: This can cause a little floating when going up inclines; you can choose the tradeoff between better
	// behavior on the edge of a ledge versus inclines by setting this to true or false
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;

	// Enable replication on the Sprite component so animations show up when networked
	GetSprite()->SetIsReplicated(true);
	bReplicates = true;

	SetCharacterState(ECharacterState::Run);
}

void ARun2DCharacter::Jump()
{
	Super::Jump();
	
	SetCharacterState(ECharacterState::Jump);
}

void ARun2DCharacter::StopJumping()
{
	Super::StopJumping();

	SetCharacterState(ECharacterState::Falling);
}

// GetState Name
const FString ARun2DCharacter::GetCharacterStateToString(ECharacterState InState) const
{
	const UEnum* CharacterState = FindObject<UEnum>(ANY_PACKAGE, TEXT("ECharacterState"));
	if (CharacterState)
	{
		return CharacterState->GetNameStringByIndex(static_cast<int32>(InState));
	}

	return TEXT("");
}

// 캐릭터 상태 변경
void ARun2DCharacter::SetCharacterState(const ECharacterState NewState)
{
	bool bChange = (CurrntState != NewState);
	if (bChange)
	{
		CurrntState = NewState;
		OnChangeCharacterState(NewState);
	}
}

// 캐릭터 상태 변경 성공 시 호출
void ARun2DCharacter::OnChangeCharacterState(const ECharacterState NewState)
{
	// 애니메이션 변경
	UpdateAnimation(NewState);

	UE_LOG(SideScrollerCharacter, Log, TEXT("Current: %s New: %s"), *GetCharacterStateToString(CurrntState), *GetCharacterStateToString(NewState));
}

bool ARun2DCharacter::CanFly() const
{
	return (0 < JumpCurrentCount);
}

//////////////////////////////////////////////////////////////////////////
// Animation

void ARun2DCharacter::UpdateAnimation(const ECharacterState NewState)
{
	UE_LOG(SideScrollerCharacter, Log, TEXT("UpdateAnimation"));

	switch (NewState)
	{
	case ECharacterState::Run:
		if (RunningAnimation)
		{
			UPaperFlipbook* DesiredAnimation = RunningAnimation;
			if (GetSprite()->GetFlipbook() != DesiredAnimation)
			{
				UE_LOG(SideScrollerCharacter, Log, TEXT("SetFlipbook "));
				GetSprite()->SetFlipbook(DesiredAnimation);
			}
		}
		break;
	default:
		break;
	}
}

void ARun2DCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateCharacter();	
}

//////////////////////////////////////////////////////////////////////////
// Input

void ARun2DCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Note: the 'Jump' action and the 'MoveRight' axis are bound to actual keys/buttons/sticks in DefaultInput.ini (editable from Project Settings..Input)
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	//PlayerInputComponent->BindAxis("MoveRight", this, &ARun2DCharacter::MoveRight);
	//PlayerInputComponent->BindTouch(IE_Pressed, this, &ARun2DCharacter::TouchStarted);
	//PlayerInputComponent->BindTouch(IE_Released, this, &ARun2DCharacter::TouchStopped);
}

void ARun2DCharacter::MoveRight(float Value)
{
	// Apply the input to the character motion
	AddMovementInput(FVector(1.0f, 0.0f, 0.0f), Value);
}

void ARun2DCharacter::TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// Jump on any touch
	Jump();
}

void ARun2DCharacter::TouchStopped(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// Cease jumping once touch stopped
	StopJumping();
}

void ARun2DCharacter::UpdateCharacter()
{
	bool bShouldForceMove = bForceRightMove;
	if (bShouldForceMove)
	{
		MoveRight(ForceRightMoveValue);
	}
}

PRAGMA_ENABLE_OPTIMIZATION