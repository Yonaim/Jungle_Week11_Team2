#pragma once

#include "Component/Gizmo/GizmoTypes.h"
#include "Math/Transform.h"

// Transform을 가진 조작 대상을 추상화하는 엔진 공용 인터페이스.
// 구현체는 Editor/Runtime 어느 쪽에 있어도 되지만, 인터페이스 자체는
// UGizmoVisualComponent / FGizmoManager가 공통으로 참조할 수 있도록 Engine에 둔다.
class ITransformGizmoTarget
{
public:
    virtual ~ITransformGizmoTarget() = default;

    // Object validity plus owner-context validity.
    // Editor targets owned by inactive tabs/viewports must return false even if the object pointer still exists.
    virtual bool IsValid() const = 0;

    virtual FTransform GetWorldTransform() const = 0;
    virtual void SetWorldTransform(const FTransform& NewWorldTransform) = 0;

    // Scale World는 FTransform::Scale만으로는 표현이 부족하므로 matrix 경로를 함께 제공한다.
    // 기본 구현은 matrix를 determinant 보정 포함 FTransform으로 분해해 기존 경로로 보낸다.
    virtual FMatrix GetWorldMatrix() const { return GetWorldTransform().ToMatrix(); }
    virtual void SetWorldMatrix(const FMatrix& NewWorldMatrix) { SetWorldTransform(FTransform::FromMatrix(NewWorldMatrix)); }

    virtual FTransform GetLocalTransform() const = 0;
    virtual void SetLocalTransform(const FTransform& NewLocalTransform) = 0;

    virtual float GetScaleDeltaSensitivity(EGizmoSpace InSpace) const
    {
        (void)InSpace;
        return 1.0f;
    }

    virtual void BeginTransform() {}
    virtual void EndTransform() {}
};
