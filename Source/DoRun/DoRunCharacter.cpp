// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DoRunCharacter.h"
#include "PaperFlipbookComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Camera/CameraComponent.h"
#include "Paper2D/Classes/PaperFlipbook.h"

DEFINE_LOG_CATEGORY_STATIC(SideScrollerCharacter, Log, All);

#define SpriteBoundRatio 0.75f

//////////////////////////////////////////////////////////////////////////
// ADoRunCharacter
PRAGMA_DISABLE_OPTIMIZATION
ADoRunCharacter::ADoRunCharacter()
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
}

void ADoRunCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (GetSprite())
	{
		GetSprite()->SetFlipbook(IdleAnimation);
	}

	SetCharacterState(ECharacterState::Run);
}

void ADoRunCharacter::Jump()
{
	Super::Jump();
	
	SetCharacterState(ECharacterState::Jump);
}

void ADoRunCharacter::StopJumping()
{
	Super::StopJumping();

	SetCharacterState(ECharacterState::Falling);
}

void ADoRunCharacter::OnLanded(const FHitResult& Hit)
{
	Super::OnLanded(Hit);
}

// GetState Name
const FString ADoRunCharacter::GetCharacterStateToString(ECharacterState InState) const
{
	const UEnum* CharacterState = FindObject<UEnum>(ANY_PACKAGE, TEXT("ECharacterState"));
	if (CharacterState)
	{
		return CharacterState->GetNameStringByIndex(static_cast<int32>(InState));
	}

	return TEXT("");
}

// 캐릭터 상태 변경
void ADoRunCharacter::SetCharacterState(const ECharacterState NewState)
{
	bool bChange = (CurrntState != NewState);
	if (bChange)
	{
		CurrntState = NewState;
		OnChangeCharacterState(NewState);
	}
}

// 캐릭터 상태 변경 성공 시 호출
void ADoRunCharacter::OnChangeCharacterState(const ECharacterState NewState)
{
	// 애니메이션 변경
	UpdateAnimation(NewState);

	UE_LOG(SideScrollerCharacter, Log, TEXT("Current: %s New: %s"), *GetCharacterStateToString(CurrntState), *GetCharacterStateToString(NewState));
}

bool ADoRunCharacter::CanFly() const
{
	return (0 < JumpCurrentCount);
}

//////////////////////////////////////////////////////////////////////////
// Animation

void ADoRunCharacter::UpdateAnimation(const ECharacterState NewState)
{
	UPaperFlipbook* DesiredAnimation = nullptr;
	switch (NewState)
	{
	case ECharacterState::Jump: // 점프 애니메이션만 따로 재생
		DesiredAnimation = JumpAnimation;
		break;
	default: // 그 외 애니메이션은 달리기
		DesiredAnimation = RunningAnimation;
		break;
	}

	bool bEnable = GetSprite() && DesiredAnimation && DesiredAnimation != GetSprite()->GetFlipbook();
	if (bEnable)
	{
		GetSprite()->SetFlipbook(DesiredAnimation);

		// Set the size of our collision capsule.
		FBoxSphereBounds SpriteBox = DesiredAnimation->GetRenderBounds();
		GetCapsuleComponent()->SetCapsuleHalfHeight(SpriteBox.GetBox().GetExtent().Y * SpriteBoundRatio);
		GetCapsuleComponent()->SetCapsuleRadius(SpriteBox.GetBox().GetSize().X * SpriteBoundRatio);
	}
}

void ADoRunCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateCharacter();	
}

//////////////////////////////////////////////////////////////////////////
// Input

void ADoRunCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Note: the 'Jump' action and the 'MoveRight' axis are bound to actual keys/buttons/sticks in DefaultInput.ini (editable from Project Settings..Input)
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	//PlayerInputComponent->BindAxis("MoveRight", this, &ADoRunCharacter::MoveRight);
	//PlayerInputComponent->BindTouch(IE_Pressed, this, &ADoRunCharacter::TouchStarted);
	//PlayerInputComponent->BindTouch(IE_Released, this, &ADoRunCharacter::TouchStopped);
}

void ADoRunCharacter::MoveRight(float Value)
{
	// Apply the input to the character motion
	AddMovementInput(FVector(1.0f, 0.0f, 0.0f), Value);
}

void ADoRunCharacter::TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// Jump on any touch
	Jump();
}

void ADoRunCharacter::TouchStopped(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// Cease jumping once touch stopped
	StopJumping();
}

void ADoRunCharacter::UpdateCharacter()
{
	bool bShouldForceMove = bForceRightMove;
	if (bShouldForceMove)
	{
		MoveRight(ForceRightMoveValue);
	}
}

PRAGMA_ENABLE_OPTIMIZATION