#include "PCH/LunaticPCH.h"
#include "Common/Viewport/EditorViewportCamera.h"

#include <cmath>

void FEditorViewportCamera::OnResize(int32 Width, int32 Height)
{
    if (Height > 0)
    {
        CameraState.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    }
}

void FEditorViewportCamera::LookAt(const FVector& Target)
{
    const FVector Diff = (Target - CameraState.Location).Normalized();
    constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

    FRotator LookRotation = CameraState.Rotation;
    LookRotation.Pitch = -asinf(Diff.Z) * Rad2Deg;
    if (fabsf(Diff.Z) < 0.999f)
    {
        LookRotation.Yaw = atan2f(Diff.Y, Diff.X) * Rad2Deg;
    }
    CameraState.Rotation = LookRotation;
}

void FEditorViewportCamera::Rotate(float YawDeltaDegrees, float PitchDeltaDegrees)
{
    CameraState.Rotation.Yaw += YawDeltaDegrees;
    CameraState.Rotation.Pitch = Clamp(CameraState.Rotation.Pitch + PitchDeltaDegrees, -89.0f, 89.0f);
}

void FEditorViewportCamera::MoveLocal(const FVector& LocalDelta)
{
    CameraState.Location = CameraState.Location +
        GetForwardVector() * LocalDelta.X +
        GetRightVector() * LocalDelta.Y +
        GetUpVector() * LocalDelta.Z;
}

FMatrix FEditorViewportCamera::GetViewMatrix() const
{
    return FMatrix::MakeViewMatrix(
        CameraState.Rotation.GetRightVector(),
        CameraState.Rotation.GetUpVector(),
        CameraState.Rotation.GetForwardVector(),
        CameraState.Location);
}

FMatrix FEditorViewportCamera::GetProjectionMatrix() const
{
    if (!CameraState.bIsOrthogonal)
    {
        return FMatrix::PerspectiveFovLH(CameraState.FOV, CameraState.AspectRatio, CameraState.NearZ, CameraState.FarZ);
    }

    const float SafeAspectRatio = (std::fabs(CameraState.AspectRatio) > 1e-4f) ? CameraState.AspectRatio : 1.0f;
    const float HalfW = CameraState.OrthoWidth * 0.5f;
    const float HalfH = HalfW / SafeAspectRatio;
    return FMatrix::OrthoLH(HalfW * 2.0f, HalfH * 2.0f, CameraState.NearZ, CameraState.FarZ);
}

FMatrix FEditorViewportCamera::GetViewProjectionMatrix() const
{
    return GetViewMatrix() * GetProjectionMatrix();
}

FConvexVolume FEditorViewportCamera::GetConvexVolume() const
{
    FConvexVolume ConvexVolume;
    ConvexVolume.UpdateFromMatrix(GetViewProjectionMatrix());
    return ConvexVolume;
}

FRay FEditorViewportCamera::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight) const
{
    FRay Ray{};
    if (ScreenWidth <= 0.0f || ScreenHeight <= 0.0f)
    {
        Ray.Origin = CameraState.Location;
        Ray.Direction = GetForwardVector();
        return Ray;
    }

    const float NdcX = (2.0f * MouseX) / ScreenWidth - 1.0f;
    const float NdcY = 1.0f - (2.0f * MouseY) / ScreenHeight;

    const FVector NdcNear(NdcX, NdcY, 1.0f);
    const FVector NdcFar(NdcX, NdcY, 0.0f);
    const FMatrix InvViewProj = GetViewProjectionMatrix().GetInverse();
    const FVector WorldNear = InvViewProj.TransformPositionWithW(NdcNear);
    const FVector WorldFar = InvViewProj.TransformPositionWithW(NdcFar);

    FVector Dir = WorldFar - WorldNear;
    const float Length = std::sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y + Dir.Z * Dir.Z);
    Dir = (Length > 1e-4f) ? (Dir / Length) : GetForwardVector();

    if (CameraState.bIsOrthogonal)
    {
        Ray.Origin = WorldNear;
        Ray.Direction = GetForwardVector();
    }
    else
    {
        Ray.Origin = CameraState.Location;
        Ray.Direction = Dir;
    }
    return Ray;
}
