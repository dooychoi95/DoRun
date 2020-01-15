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

#define SpriteBoundRatio 0.5f
#define SpriteSlideBoundRatio 0.25f

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

	ResetRightMoveDuringTime();

	SetCharacterState(ECharacterState::Run);
}

void ADoRunCharacter::Jump()
{
	Super::Jump();
	
	SetCharacterState(ECharacterState::Jump);
}

void ADoRunCharacter::Falling()
{
	Super::Falling();

	SetCharacterState(ECharacterState::Falling);
}

void ADoRunCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 슬라이딩 입력이 유지된다면 슬라이딩 상태로 변경
	bool bEnableSliding = bPressedSliding;
	if (bEnableSliding)
	{
		SetCharacterState(ECharacterState::Sliding);
	}
	else
	{
		SetCharacterState(ECharacterState::Run);
	}
}

// 슬라이딩 입력 시 즉시 상태 변경 가능한지 확인
void ADoRunCharacter::PressedSliding()
{
	ProssceSliding(true);
}

// 슬라이딩 입력 시 즉시 상태 변경 가능한지 확인
void ADoRunCharacter::ReleasedSliding()
{
	ProssceSliding(false);
}

// 슬라이딩 입력 시 즉시 상태 변경 가능한지 확인 
void ADoRunCharacter::ProssceSliding(bool bPressed)
{
	// 입력 보관
	// 점프 -> 착지 시 입력 상태에 따라 다음 행동을 제어합니다.
	bPressedSliding = bPressed;

	if (bPressed)
	{
		if (GetCharacterState() == ECharacterState::Run)
		{
			SetCharacterState(ECharacterState::Sliding);
		}
	}
	else
	{
		if (GetCharacterState() == ECharacterState::Sliding)
		{
			SetCharacterState(ECharacterState::Run);
		}
	}
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

const ECharacterState ADoRunCharacter::GetCharacterState() const
{
	return CurrntState;
}

// 캐릭터 상태 변경 성공 시 호출
void ADoRunCharacter::OnChangeCharacterState(const ECharacterState NewState)
{
	// 애니메이션 변경
	UpdateAnimation(NewState);

	UE_LOG(SideScrollerCharacter, Log, TEXT("Current: %s New: %s"), *GetCharacterStateToString(CurrntState), *GetCharacterStateToString(NewState));
}

bool ADoRunCharacter::CanProssceSliding() const
{
	return (GetCharacterState() == ECharacterState::Run);
}

bool ADoRunCharacter::ShouldSlidingToCharacterLanded() const
{
	return bPressedSliding;
}

// 캐릭터 제자리 이동 도달 시간 초기화
void ADoRunCharacter::ResetRightMoveDuringTime()
{
	if (0 < RightMoveDuringTime)
	{
		CachedRightMoveDuringTime = RightMoveDuringTime;
	}
}

//////////////////////////////////////////////////////////////////////////
// Animation

void ADoRunCharacter::UpdateAnimation(const ECharacterState NewState)
{
	UPaperFlipbook* DesiredAnimation = nullptr;
	switch (NewState)
	{
	case ECharacterState::Jump: // 점프 애니메이션
		DesiredAnimation = JumpAnimation;
		break;
	case ECharacterState::Sliding: // 슬라이딩 애니메이션
		DesiredAnimation = SlidingAnimation;
		break;
	default: 
		// 그 외 애니메이션은 달리기
		DesiredAnimation = RunningAnimation;
		break;
	}

	bool bEnable = GetSprite() && DesiredAnimation && DesiredAnimation != GetSprite()->GetFlipbook();
	if (bEnable)
	{
		GetSprite()->SetFlipbook(DesiredAnimation);

		// 캐릭터의 Y값 -> 서있는 경우 100uu, 엎드린 경우 50uu
		float BoundYSize = (DesiredAnimation == SlidingAnimation) ? SpriteSlideBoundRatio : SpriteBoundRatio;

		// Set the size of our collision capsule.
		FBoxSphereBounds SpriteBox = DesiredAnimation->GetRenderBounds();
		GetCapsuleComponent()->SetCapsuleHalfHeight(BoundYSize);
	}
}

void ADoRunCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateCharacterMove(DeltaSeconds);	
}

//////////////////////////////////////////////////////////////////////////
// Input

void ADoRunCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// 점프
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ADoRunCharacter::Jump);

	// 슬라이딩
	PlayerInputComponent->BindAction("Slide", IE_Pressed, this, &ADoRunCharacter::PressedSliding);
	PlayerInputComponent->BindAction("Slide", IE_Released, this, &ADoRunCharacter::ReleasedSliding);
}

void ADoRunCharacter::MoveRight(float Value)
{
	// Apply the input to the character motion
	AddMovementInput(FVector(1.0f, 0.0f, 0.0f), Value);
}

void ADoRunCharacter::UpdateCharacterMove(float DeltaSeconds)
{
	bool bShouldMoveUpdate = 0 < CachedRightMoveDuringTime;
	if (bShouldMoveUpdate)
	{
		CachedRightMoveDuringTime -= DeltaSeconds;

		MoveRight(ForceRightMoveValue);
	}
}
PRAGMA_ENABLE_OPTIMIZATION