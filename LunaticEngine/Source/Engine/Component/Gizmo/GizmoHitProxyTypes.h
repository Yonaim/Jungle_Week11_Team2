#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

// Gizmo picking is ID-buffer based, not raycast based.
// The picker rasterizes the currently visible gizmo mesh into a tiny pick buffer
// around the mouse cursor and returns the handle id stored at those pixels.
struct FGizmoHitProxyContext
{
    FMatrix ViewProjection = FMatrix::Identity;

    // Hit proxy picking must use the same screen-space scaling inputs as
    // FGizmoSceneProxy::UpdatePerViewport(). Otherwise zoom/dolly or an
    // offscreen viewport size mismatch makes the pick mesh diverge from the
    // rendered gizmo mesh.
    FVector CameraLocation = FVector::ZeroVector;
    bool bIsOrtho = false;
    float OrthoWidth = 10.0f;

    float ViewportWidth = 0.0f;
    float ViewportHeight = 0.0f;
    float MouseX = 0.0f; // viewport-local pixel coordinate
    float MouseY = 0.0f; // viewport-local pixel coordinate
    int32 PickRadius = 2; // 2 => 5x5 buffer
};

struct FGizmoHitProxyResult
{
    bool bHit = false;
    int32 Axis = -1; // 0=X, 1=Y, 2=Z, 3=center/plane handle
    float Depth = 0.0f;

    void Reset()
    {
        bHit = false;
        Axis = -1;
        Depth = 0.0f;
    }
};
