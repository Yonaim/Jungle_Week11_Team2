#pragma once

#include "Camera/MinimalViewInfo.h"
#include "Core/RayTypes.h"
#include "Math/Matrix.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Collision/ConvexVolume.h"
#include "Math/MathUtils.h"

// 에디터 전용 뷰포트 카메라 상태.
// 의도적으로 UCameraComponent가 아니며 World에 등록해서도 안 된다.
class FEditorViewportCamera
{
public:
    const FMinimalViewInfo& GetCameraState() const { return CameraState; }
    void SetCameraState(const FMinimalViewInfo& NewState) { CameraState = NewState; }

    void SetWorldLocation(const FVector& InLocation) { CameraState.Location = InLocation; }
    void SetRelativeLocation(const FVector& InLocation) { SetWorldLocation(InLocation); }
    const FVector& GetWorldLocation() const { return CameraState.Location; }

    void SetRelativeRotation(const FRotator& InRotation) { CameraState.Rotation = InRotation; }
    void SetRelativeRotation(const FVector& EulerRotation) { SetRelativeRotation(FRotator(EulerRotation)); }
    void SetWorldRotation(const FRotator& InRotation) { SetRelativeRotation(InRotation); }
    void SetWorldRotation(const FVector& EulerRotation) { SetRelativeRotation(EulerRotation); }
    const FRotator& GetRelativeRotation() const { return CameraState.Rotation; }

    void SetFOV(float InFOV) { CameraState.FOV = InFOV; }
    float GetFOV() const { return CameraState.FOV; }

    void SetOrthographic(bool bInOrtho) { CameraState.bIsOrthogonal = bInOrtho; }
    bool IsOrthogonal() const { return CameraState.bIsOrthogonal; }

    void SetOrthoWidth(float InWidth) { CameraState.OrthoWidth = InWidth; }
    float GetOrthoWidth() const { return CameraState.OrthoWidth; }
    float GetNearPlane() const { return CameraState.NearZ; }
    float GetFarPlane() const { return CameraState.FarZ; }

    void OnResize(int32 Width, int32 Height);
    void LookAt(const FVector& Target);
    void Rotate(float YawDeltaDegrees, float PitchDeltaDegrees);
    void MoveLocal(const FVector& LocalDelta);

    FVector GetForwardVector() const { return CameraState.Rotation.GetForwardVector(); }
    FVector GetRightVector() const { return CameraState.Rotation.GetRightVector(); }
    FVector GetUpVector() const { return CameraState.Rotation.GetUpVector(); }

    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;
    FMatrix GetViewProjectionMatrix() const;
    FConvexVolume GetConvexVolume() const;
    FRay DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight) const;

private:
    FMinimalViewInfo CameraState;
};
