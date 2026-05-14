#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Input/InputAction.h"

// Common per-frame input accumulation used by editor-style viewport camera controls.
// Input binding is still owned by the concrete editor context, but the actual data shape
// is shared by Level Editor and Asset Preview viewport clients.
struct FEditorViewportInputState
{
    FVector MoveAccumulator = FVector::ZeroVector;
    FVector RotateAccumulator = FVector::ZeroVector;
    FVector PanAccumulator = FVector::ZeroVector;
    float ZoomAccumulator = 0.0f;

    void ResetFrame()
    {
        MoveAccumulator = FVector::ZeroVector;
        RotateAccumulator = FVector::ZeroVector;
        PanAccumulator = FVector::ZeroVector;
        ZoomAccumulator = 0.0f;
    }
};

// Owns the editor-viewport common input actions.
//
// The mapping/binding policy can differ per editor context, so this class deliberately
// does not bind callbacks by itself. It only removes duplicated action ownership from
// concrete viewport clients.
class FEditorViewportInputController
{
public:
    FEditorViewportInputController() = default;
    ~FEditorViewportInputController() { ReleaseActions(); }

    void CreateCommonActions()
    {
        if (Move)
        {
            return;
        }

        Move = new FInputAction("IA_EditorMove", EInputActionValueType::Axis3D);
        Rotate = new FInputAction("IA_EditorRotate", EInputActionValueType::Axis2D);
        Pan = new FInputAction("IA_EditorPan", EInputActionValueType::Axis2D);
        Zoom = new FInputAction("IA_EditorZoom", EInputActionValueType::Float);
        Orbit = new FInputAction("IA_EditorOrbit", EInputActionValueType::Axis2D);

        Focus = new FInputAction("IA_EditorFocus", EInputActionValueType::Bool);
        ToggleGizmoMode = new FInputAction("IA_EditorToggleGizmoMode", EInputActionValueType::Bool);
        ToggleCoordSystem = new FInputAction("IA_EditorToggleCoordSystem", EInputActionValueType::Bool);
        Escape = new FInputAction("IA_EditorEscape", EInputActionValueType::Bool);

        DecreaseSnap = new FInputAction("IA_EditorDecreaseSnap", EInputActionValueType::Bool);
        IncreaseSnap = new FInputAction("IA_EditorIncreaseSnap", EInputActionValueType::Bool);
        ToggleGridSnap = new FInputAction("IA_ToggleGridSnap", EInputActionValueType::Bool);
        ToggleRotationSnap = new FInputAction("IA_ToggleRotationSnap", EInputActionValueType::Bool);
        ToggleScaleSnap = new FInputAction("IA_ToggleScaleSnap", EInputActionValueType::Bool);
    }

    void ReleaseActions()
    {
        delete Move; Move = nullptr;
        delete Rotate; Rotate = nullptr;
        delete Pan; Pan = nullptr;
        delete Zoom; Zoom = nullptr;
        delete Orbit; Orbit = nullptr;
        delete Focus; Focus = nullptr;
        delete ToggleGizmoMode; ToggleGizmoMode = nullptr;
        delete ToggleCoordSystem; ToggleCoordSystem = nullptr;
        delete Escape; Escape = nullptr;
        delete DecreaseSnap; DecreaseSnap = nullptr;
        delete IncreaseSnap; IncreaseSnap = nullptr;
        delete ToggleGridSnap; ToggleGridSnap = nullptr;
        delete ToggleRotationSnap; ToggleRotationSnap = nullptr;
        delete ToggleScaleSnap; ToggleScaleSnap = nullptr;
    }

    FEditorViewportInputState InputState;

    FInputAction* Move = nullptr;
    FInputAction* Rotate = nullptr;
    FInputAction* Pan = nullptr;
    FInputAction* Zoom = nullptr;
    FInputAction* Orbit = nullptr;

    FInputAction* Focus = nullptr;
    FInputAction* ToggleGizmoMode = nullptr;
    FInputAction* ToggleCoordSystem = nullptr;
    FInputAction* Escape = nullptr;
    FInputAction* DecreaseSnap = nullptr;
    FInputAction* IncreaseSnap = nullptr;
    FInputAction* ToggleGridSnap = nullptr;
    FInputAction* ToggleRotationSnap = nullptr;
    FInputAction* ToggleScaleSnap = nullptr;
};
