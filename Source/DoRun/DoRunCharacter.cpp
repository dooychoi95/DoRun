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

	// 카메라 월드 고정 전까지 입력 막음
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(true);

		// 게임 종료 위젯 생성
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
	//}
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

// 이동 중 충돌되는 경우 사망 연출 후 게임 종료
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
	// 입력 막음
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(true);
	}

	// 이동 벡터를 0으로 만들어 이동 불가하게 함
	MoveRightVector = FVector::ZeroVector;

	CachedRightMoveDuringTime = 0.0f;

	// 캐릭터를 위로 살짝 띄워준 후
	if (GetMovementComponent())
	{
		GetMovementComponent()->Velocity = FVector::ZeroVector;
		GetMovementComponent()->Velocity.Z = 350.f;
		GetMovementComponent()->UpdateComponentVelocity();
	}

	// Collision 을 꺼서 월드 바깥으로 떨어져 사망하게끔 함
	SetActorEnableCollision(false);

	// 사망 상태 변경 및 애니메이션 재생
	SetCharacterState(ECharacterState::Dying);

	// 카메라 현재 위치 고정
	if (SideViewCameraComponent)
	{
		SideViewCameraComponent->AddLocalRotation(FRotator());
		SideViewCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	// 게임 재시작 주기 설정
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

// 캐릭터 제자리 이동 도달 시간 초기화
void ADoRunCharacter::ResetRightMoveDuringTime()
{
	if (0 < RightMoveDuringTime)
	{
		CachedRightMoveDuringTime = RightMoveDuringTime;
	}
	else
	{
		// 없다면 기본 달리기로
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
	case ECharacterState::Jump: // 점프 애니메이션
		DesiredAnimation = JumpAnimation;
		break;
	case ECharacterState::Sliding: // 슬라이딩 애니메이션
		DesiredAnimation = SlidingAnimation;
		break;
	case ECharacterState::Dying:
		DesiredAnimation = DyingAnimation;
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
		//
		/**
		 * 캐릭터 Z (Half Height)
		 *  a. 서있는 경우 100uu -> 50
		 *  b. 엎드린 경우 50uu -> 25
		 * 캐릭터 X (Radius)
		 *  a. 서있는 경우 25
		 *  b. 엎드린 경우 50
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
	// 점프
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ADoRunCharacter::Jump);

	// 슬라이딩
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
		
		// 장애물에 막힌 경우 X, Y 값이 변화하지 않기 때문에 Velocity 로 이동 Todo 고드 정리 ㅠㅠ
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
			// 카메라 현재 위치 고정
			if (SideViewCameraComponent)
			{
				SideViewCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}

			// 카메라 고정 후 인풋 가능하게 변경
			if (Controller)
			{
				Controller->SetIgnoreMoveInput(false);
			}

			// 카메라 고정 후 캐릭터가 뒤로 위치하도록
			ForceRightMoveValue *= -1.f;
			bCameraFixed = true;
		}
	}
}
PRAGMA_ENABLE_OPTIMIZATION