#include "PCH/LunaticPCH.h"
#include "Common/Viewport/ViewportCameraController.h"
#include "Math/Quat.h"

#include <algorithm>
#include <cmath>

namespace
{
float ClampCameraFloat(float Value, float MinValue, float MaxValue)
{
    return (std::max)(MinValue, (std::min)(Value, MaxValue));
}
}

void FViewportCameraController::SetCamera(FEditorViewportCamera* InCamera)
{
    Camera = InCamera;
    SyncTargetToCamera();
}

void FViewportCameraController::ResetLookAt(const FVector& Location, const FVector& Target)
{
    if (!Camera)
    {
        return;
    }

    Camera->SetWorldLocation(Location);
    Camera->LookAt(Target);
    SyncTargetToCamera();
}

void FViewportCameraController::SyncTargetToCamera()
{
    if (!Camera)
    {
        bTargetLocationInitialized = false;
        bLastAppliedCameraLocationInitialized = false;
        return;
    }

    const FVector CurrentLocation = Camera->GetWorldLocation();
    const bool bCameraMovedExternally = bLastAppliedCameraLocationInitialized &&
        FVector::DistSquared(CurrentLocation, LastAppliedCameraLocation) > 0.0001f;

    if (!bTargetLocationInitialized || bCameraMovedExternally)
    {
        TargetLocation = CurrentLocation;
        bTargetLocationInitialized = true;
    }

    LastAppliedCameraLocation = CurrentLocation;
    bLastAppliedCameraLocationInitialized = true;
}

void FViewportCameraController::ApplySmoothedLocation(float DeltaTime, float SmoothSpeed)
{
    if (!Camera)
    {
        return;
    }

    const FVector CurrentLocation = Camera->GetWorldLocation();
    const float LerpAlpha = ClampCameraFloat(DeltaTime * SmoothSpeed, 0.0f, 1.0f);
    const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
    Camera->SetWorldLocation(NewLocation);
    LastAppliedCameraLocation = NewLocation;
    bLastAppliedCameraLocationInitialized = true;
}

void FViewportCameraController::AddWorldTargetDelta(const FVector& WorldDelta)
{
    SyncTargetToCamera();
    TargetLocation += WorldDelta;
}

void FViewportCameraController::AddLocalTargetDelta(const FVector& LocalDelta)
{
    if (!Camera)
    {
        return;
    }

    AddWorldTargetDelta(
        Camera->GetForwardVector() * LocalDelta.X +
        Camera->GetRightVector() * LocalDelta.Y +
        Camera->GetUpVector() * LocalDelta.Z);
}

void FViewportCameraController::AddPanTargetDelta(float DeltaX, float DeltaY, float Scale)
{
    if (!Camera)
    {
        return;
    }

    AddWorldTargetDelta(
        Camera->GetRightVector() * (-DeltaX * Scale) +
        Camera->GetUpVector() * (DeltaY * Scale));
}

void FViewportCameraController::AddForwardTargetDelta(float Amount)
{
    if (!Camera)
    {
        return;
    }

    AddWorldTargetDelta(Camera->GetForwardVector() * Amount);
}

void FViewportCameraController::Rotate(float YawDeltaDegrees, float PitchDeltaDegrees)
{
    if (Camera)
    {
        Camera->Rotate(YawDeltaDegrees, PitchDeltaDegrees);
    }
}

void FViewportCameraController::MoveLocalImmediate(const FVector& LocalDelta)
{
    if (Camera)
    {
        Camera->MoveLocal(LocalDelta);
        SyncTargetToCamera();
    }
}

void FViewportCameraController::SetViewportType(ELevelViewportType Type, float OrthoDistance)
{
    if (!Camera)
    {
        return;
    }

    if (Type == ELevelViewportType::Perspective)
    {
        Camera->SetOrthographic(false);
        SyncTargetToCamera();
        return;
    }

    Camera->SetOrthographic(true);
    if (Type == ELevelViewportType::FreeOrthographic)
    {
        SyncTargetToCamera();
        return;
    }

    FVector Position = FVector::ZeroVector;
    switch (Type)
    {
    case ELevelViewportType::Top:
        Position = FVector(0.0f, 0.0f, OrthoDistance);
        break;
    case ELevelViewportType::Bottom:
        Position = FVector(0.0f, 0.0f, -OrthoDistance);
        break;
    case ELevelViewportType::Front:
        Position = FVector(OrthoDistance, 0.0f, 0.0f);
        break;
    case ELevelViewportType::Back:
        Position = FVector(-OrthoDistance, 0.0f, 0.0f);
        break;
    case ELevelViewportType::Left:
        Position = FVector(0.0f, -OrthoDistance, 0.0f);
        break;
    case ELevelViewportType::Right:
        Position = FVector(0.0f, OrthoDistance, 0.0f);
        break;
    default:
        break;
    }

    Camera->SetRelativeLocation(Position);
    Camera->SetRelativeRotation(GetFixedViewportRotation(Type));
    SyncTargetToCamera();
}

void FViewportCameraController::StartFocus(const FVector& TargetLocationIn, float Distance, float Duration)
{
    if (!Camera)
    {
        return;
    }

    StartFocus(TargetLocationIn, Camera->GetForwardVector(), Distance, Duration);
}

void FViewportCameraController::StartFocus(const FVector& TargetLocationIn, const FVector& Forward, float Distance, float Duration)
{
    if (!Camera)
    {
        return;
    }

    const FVector OriginalLoc = Camera->GetWorldLocation();
    const FRotator OriginalRot = Camera->GetRelativeRotation();
    const FVector NewCameraLoc = TargetLocationIn - Forward * Distance;

    Camera->SetWorldLocation(NewCameraLoc);
    Camera->LookAt(TargetLocationIn);
    const FRotator TargetRot = Camera->GetRelativeRotation();

    Camera->SetWorldLocation(OriginalLoc);
    Camera->SetRelativeRotation(OriginalRot);

    bFocusAnimating = true;
    FocusTimer = 0.0f;
    FocusDuration = (std::max)(0.001f, Duration);
    FocusStartLoc = OriginalLoc;
    FocusStartRot = OriginalRot;
    FocusEndLoc = NewCameraLoc;
    FocusEndRot = TargetRot;
}

void FViewportCameraController::StartFocusToTransform(const FVector& EndLocation, const FRotator& EndRotation, float Duration)
{
    if (!Camera)
    {
        return;
    }

    bFocusAnimating = true;
    FocusTimer = 0.0f;
    FocusDuration = (std::max)(0.001f, Duration);
    FocusStartLoc = Camera->GetWorldLocation();
    FocusStartRot = Camera->GetRelativeRotation();
    FocusEndLoc = EndLocation;
    FocusEndRot = EndRotation;
}

bool FViewportCameraController::TickFocus(float DeltaTime)
{
    if (!Camera || !bFocusAnimating)
    {
        return false;
    }

    FocusTimer += DeltaTime;
    float Alpha = FocusTimer / FocusDuration;
    if (Alpha >= 1.0f)
    {
        Alpha = 1.0f;
        bFocusAnimating = false;
    }

    const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
    const FVector NewLoc = FocusStartLoc * (1.0f - SmoothAlpha) + FocusEndLoc * SmoothAlpha;
    const FQuat StartQuat = FocusStartRot.ToQuaternion();
    const FQuat EndQuat = FocusEndRot.ToQuaternion();
    const FQuat BlendedQuat = FQuat::Slerp(StartQuat, EndQuat, SmoothAlpha);

    Camera->SetWorldLocation(NewLoc);
    Camera->SetRelativeRotation(FRotator::FromQuaternion(BlendedQuat));
    TargetLocation = NewLoc;
    LastAppliedCameraLocation = NewLoc;
    bTargetLocationInitialized = true;
    bLastAppliedCameraLocationInitialized = true;
    return true;
}

void FViewportCameraController::SetOrbit(const FVector& Target, float Distance, float YawDegrees, float PitchDegrees)
{
    OrbitTarget = Target;
    OrbitDistance = (std::max)(0.15f, Distance);
    OrbitYaw = YawDegrees;
    OrbitPitch = PitchDegrees;
    ApplyOrbitToCamera(true);
}

void FViewportCameraController::GetOrbit(FVector& OutTarget, float& OutDistance, float& OutYawDegrees, float& OutPitchDegrees) const
{
    OutTarget = OrbitTarget;
    OutDistance = OrbitDistance;
    OutYawDegrees = OrbitYaw;
    OutPitchDegrees = OrbitPitch;
}

void FViewportCameraController::ApplyOrbitToCamera(bool bLookAtTarget)
{
    if (!Camera)
    {
        return;
    }

    const FVector CameraLocation = MakeOrbitCameraLocation(OrbitTarget, OrbitDistance, OrbitYaw, OrbitPitch);
    Camera->SetWorldLocation(CameraLocation);
    if (bLookAtTarget)
    {
        Camera->LookAt(OrbitTarget);
    }
    SyncTargetToCamera();
}

void FViewportCameraController::Orbit(float DeltaYawDegrees, float DeltaPitchDegrees, float MinPitch, float MaxPitch)
{
    OrbitYaw += DeltaYawDegrees;
    OrbitPitch = ClampCameraFloat(OrbitPitch + DeltaPitchDegrees, MinPitch, MaxPitch);
    ApplyOrbitToCamera(true);
}

void FViewportCameraController::PanOrbitTarget(float DeltaX, float DeltaY, float Scale)
{
    if (!Camera)
    {
        return;
    }

    OrbitTarget = OrbitTarget - Camera->GetRightVector() * (DeltaX * Scale) + Camera->GetUpVector() * (DeltaY * Scale);
    ApplyOrbitToCamera(true);
}

void FViewportCameraController::DollyOrbit(float WheelDelta, float ZoomScale, float MinDistance)
{
    OrbitDistance = (std::max)(MinDistance, OrbitDistance * (1.0f - WheelDelta * ZoomScale));
    ApplyOrbitToCamera(true);
}

void FViewportCameraController::MoveOrbitTargetLocal(const FVector& LocalDirection, float Amount)
{
    if (!Camera)
    {
        return;
    }

    FVector Direction =
        Camera->GetForwardVector() * LocalDirection.X +
        Camera->GetRightVector() * LocalDirection.Y +
        Camera->GetUpVector() * LocalDirection.Z;

    const float Length = std::sqrt(Direction.X * Direction.X + Direction.Y * Direction.Y + Direction.Z * Direction.Z);
    if (Length > 0.0001f)
    {
        OrbitTarget += Direction * (Amount / Length);
        ApplyOrbitToCamera(true);
    }
}

FVector FViewportCameraController::MakeOrbitCameraLocation(const FVector& Target, float Distance, float YawDeg, float PitchDeg)
{
    constexpr float DegToRad = 3.14159265358979323846f / 180.0f;
    const float Yaw = YawDeg * DegToRad;
    const float Pitch = PitchDeg * DegToRad;

    const float CP = std::cos(Pitch);
    const float SP = std::sin(Pitch);
    const float CY = std::cos(Yaw);
    const float SY = std::sin(Yaw);

    const FVector Forward(CP * CY, CP * SY, SP);
    return Target - Forward * Distance;
}

FRotator FViewportCameraController::GetFixedViewportRotation(ELevelViewportType Type)
{
    switch (Type)
    {
    case ELevelViewportType::Top: return FRotator(90.0f, 0.0f, 0.0f);
    case ELevelViewportType::Bottom: return FRotator(-90.0f, 0.0f, 0.0f);
    case ELevelViewportType::Left: return FRotator(0.0f, 90.0f, 0.0f);
    case ELevelViewportType::Right: return FRotator(0.0f, -90.0f, 0.0f);
    case ELevelViewportType::Front: return FRotator(0.0f, 180.0f, 0.0f);
    case ELevelViewportType::Back: return FRotator(0.0f, 0.0f, 0.0f);
    case ELevelViewportType::FreeOrthographic:
    case ELevelViewportType::Perspective:
    default: return FRotator::ZeroRotator;
    }
}
