// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DoRunCharacter.h"
#include "PaperFlipbookComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Camera/CameraComponent.h"
#include "Paper2D/Classes/PaperFlipbook.h"
#include "Paper2D/Classes/PaperTileMapActor.h"
#include "Paper2D/Classes/PaperTileMapComponent.h"
#include "Blueprint/UserWidget.h"
//#include "GameFramework/GameMode.h"
#include "DoRunGameMode.h"

#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(SideScrollerCharacter, Log, All);

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
	//GetCapsuleComponent()->SetCapsuleHalfHeight(51.0f);
	//GetCapsuleComponent()->SetCapsuleRadius(50.0f);

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
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = false;

	// Enable replication on the Sprite component so animations show up when networked
	GetSprite()->SetIsReplicated(true);
	bReplicates = true;

	MoveRightVector = FVector(1.0f, 0.0f, 0.0f);
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

	// ī�޶� ���� ���� ������ �Է� ����
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(true);

		// ���� ���� ���� ����
		if (StartingWidgetClass != nullptr)
		{
			CurrentWidget = CreateWidget<UUserWidget>(GetWorld(), StartingWidgetClass);
			if (!CurrentWidget->IsInViewport())
			{
				CurrentWidget->AddToViewport();
			}
		}
	}
}

void ADoRunCharacter::Jump()
{
	if (CurrentWidget->IsInViewport())
	{
		CurrentWidget->RemoveFromViewport();
		return;
	}

	Super::Jump();

	SetCharacterState(ECharacterState::Jump);
}

void ADoRunCharacter::Falling()
{
	Super::Falling();

	if (GetCharacterState() != ECharacterState::Dying)
	{
		SetCharacterState(ECharacterState::Falling);
	}
}

void ADoRunCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	//if (!HasAuthority())
	//{
		// �����̵� �Է��� �����ȴٸ� �����̵� ���·� ����
		bool bEnableSliding = bPressedSliding;
		if (bEnableSliding)
		{
			SetCharacterState(ECharacterState::Sliding);
		}
		else
		{
			SetCharacterState(ECharacterState::Run);
		}
	//}
}

// �����̵� �Է� �� ��� ���� ���� �������� Ȯ��
void ADoRunCharacter::PressedSliding()
{
	ProssceSliding(true);
}

// �����̵� �Է� �� ��� ���� ���� �������� Ȯ��
void ADoRunCharacter::ReleasedSliding()
{
	ProssceSliding(false);
}

// �����̵� �Է� �� ��� ���� ���� �������� Ȯ�� 
void ADoRunCharacter::ProssceSliding(bool bPressed)
{
	// �Է� ����
	// ���� -> ���� �� �Է� ���¿� ���� ���� �ൿ�� �����մϴ�.
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

// �̵� �� �浹�Ǵ� ��� ��� ���� �� ���� ����
void ADoRunCharacter::MoveBlockedBy(const FHitResult& Impact)
{
	Super::MoveBlockedBy(Impact);

	if (Impact.Actor.IsValid())
	{
		CharacterMoveBlockedEnding();

		UE_LOG(SideScrollerCharacter, Log, TEXT("Character MoveBlockedBy"), *Impact.Actor.Get()->GetName());
	}
}

void ADoRunCharacter::CharacterMoveBlockedEnding()
{
	// �Է� ����
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(true);
	}

	// �̵� ���͸� 0���� ����� �̵� �Ұ��ϰ� ��
	MoveRightVector = FVector::ZeroVector;

	CachedRightMoveDuringTime = 0.0f;

	// ĳ���͸� ���� ��¦ ����� ��
	if (GetMovementComponent())
	{
		GetMovementComponent()->Velocity = FVector::ZeroVector;
		GetMovementComponent()->Velocity.Z = 350.f;
		GetMovementComponent()->UpdateComponentVelocity();
	}

	// Collision �� ���� ���� �ٱ����� ������ ����ϰԲ� ��
	SetActorEnableCollision(false);

	// ��� ���� ���� �� �ִϸ��̼� ���
	SetCharacterState(ECharacterState::Dying);

	// ī�޶� ���� ��ġ ����
	if (SideViewCameraComponent)
	{
		SideViewCameraComponent->AddLocalRotation(FRotator());
		SideViewCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	// ���� ����� �ֱ� ����
	if (HasAuthority() && GetWorld())
	{
		ADoRunGameMode* DoRunGameMode = GetWorld()->GetAuthGameMode() ? Cast<ADoRunGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
		if (DoRunGameMode)
		{
			DoRunGameMode->SetGameRestart();
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

// ĳ���� ���� ����
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

// ĳ���� ���� ���� ���� �� ȣ��
void ADoRunCharacter::OnChangeCharacterState(const ECharacterState NewState)
{
	// �ִϸ��̼� ����
	UpdateAnimationToStateChange(NewState);

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

// ĳ���� ���ڸ� �̵� ���� �ð� �ʱ�ȭ
void ADoRunCharacter::ResetRightMoveDuringTime()
{
	if (0 < RightMoveDuringTime)
	{
		CachedRightMoveDuringTime = RightMoveDuringTime;
	}
	else
	{
		// ���ٸ� �⺻ �޸����
		CachedRightMoveDuringTime = 100000.f;
	}
}

//////////////////////////////////////////////////////////////////////////
// Animation

void ADoRunCharacter::UpdateAnimationToStateChange(const ECharacterState NewState)
{
	UPaperFlipbook* DesiredAnimation = nullptr;
	switch (NewState)
	{
	case ECharacterState::Jump: // ���� �ִϸ��̼�
		DesiredAnimation = JumpAnimation;
		break;
	case ECharacterState::Sliding: // �����̵� �ִϸ��̼�
		DesiredAnimation = SlidingAnimation;
		break;
	case ECharacterState::Dying:
		DesiredAnimation = DyingAnimation;
		break;
	default: 
		// �� �� �ִϸ��̼��� �޸���
		DesiredAnimation = RunningAnimation;
		break;
	}

	bool bEnable = GetSprite() && DesiredAnimation && DesiredAnimation != GetSprite()->GetFlipbook();
	if (bEnable)
	{
		GetSprite()->SetFlipbook(DesiredAnimation);

		// ĳ������ Y�� -> ���ִ� ��� 100uu, ���帰 ��� 50uu
		//
		/**
		 * ĳ���� Z (Half Height)
		 *  a. ���ִ� ��� 100uu -> 50
		 *  b. ���帰 ��� 50uu -> 25
		 * ĳ���� X (Radius)
		 *  a. ���ִ� ��� 25
		 *  b. ���帰 ��� 50
		 */
		float NewRadius = 0.f;
		float NewHalfHeight = 0.f;
		bool bSliding = DesiredAnimation == SlidingAnimation;
		if (bSliding)
		{
			NewRadius = 20.f;
			NewHalfHeight = 10.f;
		}
		else
		{
			NewRadius = 25.f;
			NewHalfHeight = 50.f;
		}

		GetCapsuleComponent()->SetCapsuleSize(NewRadius, NewHalfHeight, true);
	}
}

void ADoRunCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CurrentWidget && !CurrentWidget->IsInViewport())
	{
		UpdateCharacterMove(DeltaSeconds);
		UpdateCameraDetach(DeltaSeconds);
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ADoRunCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// ����
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ADoRunCharacter::Jump);

	// �����̵�
	PlayerInputComponent->BindAction("Slide", IE_Pressed, this, &ADoRunCharacter::PressedSliding);
	PlayerInputComponent->BindAction("Slide", IE_Released, this, &ADoRunCharacter::ReleasedSliding);
}

void ADoRunCharacter::MoveRight(float Value)
{
	AddMovementInput(MoveRightVector, Value);
}

void ADoRunCharacter::UpdateCharacterMove(float DeltaSeconds)
{
	bool bShouldMoveUpdate = 0 < CachedRightMoveDuringTime;
	if (bShouldMoveUpdate)
	{
		CachedRightMoveDuringTime -= DeltaSeconds;

		MoveRight(ForceRightMoveValue);
		
		// ��ֹ��� ���� ��� X, Y ���� ��ȭ���� �ʱ� ������ Velocity �� �̵� Todo ��� ���� �Ф�
		if (GetMovementComponent()->Velocity.X <= 350.f)
		{
			GetMovementComponent()->Velocity.X = 350.f;
		}
	}
}

void ADoRunCharacter::UpdateCameraDetach(float DeltaSeconds)
{
	bool bShouldCameraFixed = !bCameraFixed && 0 < CameraDetachDuringTime;
	if (bShouldCameraFixed)
	{
		CameraDetachDuringTime -= DeltaSeconds;
		
		bool bFinishTime = (CameraDetachDuringTime < 0.f);
		if (bFinishTime)
		{
			// ī�޶� ���� ��ġ ����
			if (SideViewCameraComponent)
			{
				SideViewCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}

			// ī�޶� ���� �� ��ǲ �����ϰ� ����
			if (Controller)
			{
				Controller->SetIgnoreMoveInput(false);
			}

			// ī�޶� ���� �� ĳ���Ͱ� �ڷ� ��ġ�ϵ���
			ForceRightMoveValue *= -1.f;
			bCameraFixed = true;
		}
	}
}
PRAGMA_ENABLE_OPTIMIZATION