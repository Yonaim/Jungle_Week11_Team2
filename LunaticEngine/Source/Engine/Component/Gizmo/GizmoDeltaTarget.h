#pragma once

#include "Component/Gizmo/GizmoTypes.h"
#include "Math/Matrix.h"
#include "Math/Transform.h"

struct FGizmoDragBeginContext
{
    EGizmoMode Mode = EGizmoMode::Select;
    EGizmoSpace Space = EGizmoSpace::Local;
    int32 ActiveAxis = -1;
    FTransform StartTargetTransform;
    FMatrix StartTargetWorldMatrix = FMatrix::Identity;
    FVector StartAxisVector = FVector::ZeroVector;
};

struct FGizmoDelta
{
    EGizmoMode Mode = EGizmoMode::Select;
    EGizmoSpace Space = EGizmoSpace::Local;
    int32 ActiveAxis = -1;
    FVector AxisVectorComponent = FVector::ZeroVector;
    FVector LinearDeltaComponent = FVector::ZeroVector;
    float AngularDeltaRadians = 0.0f;
    FVector ScaleDelta = FVector::ZeroVector;
    FTransform StartTargetTransform;
};

class IGizmoDeltaTarget
{
public:
    virtual ~IGizmoDeltaTarget() = default;

    virtual bool BeginGizmoDrag(const FGizmoDragBeginContext& Context) = 0;
    virtual bool ApplyGizmoDelta(const FGizmoDelta& Delta) = 0;
    virtual void EndGizmoDrag(bool bCancelled) = 0;
};
