#pragma once

#include "Common/Viewport/EditorViewportCamera.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Render/Types/ViewTypes.h"

/**
 * Editor viewport camera 공통 컨트롤러.
 *
 * 역할:
 * - Level Editor / Asset Preview Viewport가 공유하는 카메라 이동, 회전, 팬, 줌, 오비트, 포커스 보간을 담당한다.
 * - 실제 카메라 데이터는 FEditorViewportCamera가 소유하고, 이 클래스는 입력 결과를 카메라에 적용하는 controller다.
 *
 * 주의:
 * - Actor 선택, Bone 선택, Gizmo drag 같은 editor별 정책은 여기로 가져오지 않는다.
 * - 이 클래스는 순수 viewport camera 동작만 다룬다.
 */
class FViewportCameraController
{
public:
    void SetCamera(FEditorViewportCamera* InCamera);
    FEditorViewportCamera* GetCamera() const { return Camera; }

    void ResetLookAt(const FVector& Location, const FVector& Target);

    void SyncTargetToCamera();
    void ApplySmoothedLocation(float DeltaTime, float SmoothSpeed);

    void AddWorldTargetDelta(const FVector& WorldDelta);
    void AddLocalTargetDelta(const FVector& LocalDelta);
    void AddPanTargetDelta(float DeltaX, float DeltaY, float Scale);
    void AddForwardTargetDelta(float Amount);
    void Rotate(float YawDeltaDegrees, float PitchDeltaDegrees);
    void MoveLocalImmediate(const FVector& LocalDelta);

    void SetViewportType(ELevelViewportType Type, float OrthoDistance = 50.0f);

    void StartFocus(const FVector& TargetLocation, float Distance, float Duration);
    void StartFocus(const FVector& TargetLocation, const FVector& Forward, float Distance, float Duration);
    void StartFocusToTransform(const FVector& EndLocation, const FRotator& EndRotation, float Duration);
    bool TickFocus(float DeltaTime);
    bool IsFocusAnimating() const { return bFocusAnimating; }
    void CancelFocus() { bFocusAnimating = false; }

    void SetOrbit(const FVector& Target, float Distance, float YawDegrees, float PitchDegrees);
    void GetOrbit(FVector& OutTarget, float& OutDistance, float& OutYawDegrees, float& OutPitchDegrees) const;
    void ApplyOrbitToCamera(bool bLookAtTarget = true);
    void Orbit(float DeltaYawDegrees, float DeltaPitchDegrees, float MinPitch = -85.0f, float MaxPitch = 85.0f);
    void PanOrbitTarget(float DeltaX, float DeltaY, float Scale);
    void DollyOrbit(float WheelDelta, float ZoomScale = 0.08f, float MinDistance = 0.15f);
    void MoveOrbitTargetLocal(const FVector& LocalDirection, float Amount);

    void SetOrbitTarget(const FVector& InTarget) { OrbitTarget = InTarget; }
    const FVector& GetOrbitTarget() const { return OrbitTarget; }
    void SetOrbitDistance(float InDistance) { OrbitDistance = InDistance; }
    float GetOrbitDistance() const { return OrbitDistance; }

private:
    static FVector MakeOrbitCameraLocation(const FVector& Target, float Distance, float YawDeg, float PitchDeg);
    static FRotator GetFixedViewportRotation(ELevelViewportType Type);

private:
    FEditorViewportCamera* Camera = nullptr;

    FVector TargetLocation = FVector::ZeroVector;
    bool bTargetLocationInitialized = false;
    FVector LastAppliedCameraLocation = FVector::ZeroVector;
    bool bLastAppliedCameraLocationInitialized = false;

    bool bFocusAnimating = false;
    FVector FocusStartLoc = FVector::ZeroVector;
    FRotator FocusStartRot = FRotator::ZeroRotator;
    FVector FocusEndLoc = FVector::ZeroVector;
    FRotator FocusEndRot = FRotator::ZeroRotator;
    float FocusTimer = 0.0f;
    float FocusDuration = 0.5f;

    FVector OrbitTarget = FVector::ZeroVector;
    float OrbitDistance = 6.0f;
    float OrbitYaw = 180.0f;
    float OrbitPitch = -10.0f;
};
