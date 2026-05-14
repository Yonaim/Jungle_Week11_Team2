#pragma once

#include "Common/Viewport/EditorViewportClient.h"
#include "Component/Gizmo/TransformGizmoTarget.h"
#include "Component/SceneComponent.h"

// SceneComponent를 기즈모 조작 대상으로 감싸는 공통 어댑터.
// Level Editor의 Actor/Component 선택과 Asset Preview의 PreviewComponent 선택 모두 이 경로를 쓸 수 있다.
class FSceneComponentTransformGizmoTarget final : public ITransformGizmoTarget
{
public:
    explicit FSceneComponentTransformGizmoTarget(USceneComponent* InComponent,
                                                 const FEditorViewportClient* InOwnerViewportClient = nullptr,
                                                 const bool* InOwnerContextActiveFlag = nullptr,
                                                 uint64 InOwnerContextEpoch = 0)
        : Component(InComponent)
        , OwnerViewportClient(InOwnerViewportClient)
        , OwnerContextActiveFlag(InOwnerContextActiveFlag)
        , OwnerContextEpoch(InOwnerContextEpoch)
    {
    }

    bool IsValid() const override
    {
        if (!Component)
        {
            return false;
        }

        if (OwnerViewportClient && !OwnerViewportClient->CanProcessLiveContextWork())
        {
            return false;
        }

        if (OwnerViewportClient && OwnerViewportClient->GetEditorContextEpoch() != OwnerContextEpoch)
        {
            return false;
        }

        return !OwnerContextActiveFlag || *OwnerContextActiveFlag;
    }

    FTransform GetWorldTransform() const override
    {
        if (!IsValid())
        {
            return FTransform();
        }

        return FTransform(Component->GetWorldLocation(), Component->GetWorldQuat(), Component->GetWorldScale());
    }

    void SetWorldTransform(const FTransform& NewWorldTransform) override
    {
        // Hard isolation guard: stale targets from inactive tabs must never write.
        if (!IsValid())
        {
            return;
        }
        SetWorldMatrix(NewWorldTransform.ToMatrix());
    }

    FMatrix GetWorldMatrix() const override
    {
        return IsValid() ? Component->GetWorldMatrix() : FMatrix::Identity;
    }

    void SetWorldMatrix(const FMatrix& NewWorldMatrix) override
    {
        // This target can survive inside an old FGizmoManager after a tab/context switch.
        // Never allow that stale wrapper to mutate a component unless its owner viewport is
        // still the single live editor viewport.
        if (!IsValid())
        {
            return;
        }

        FMatrix NewRelativeMatrix = NewWorldMatrix;
        if (USceneComponent* Parent = Component->GetParent())
        {
            // Row-major convention: World = Local * ParentWorld.
            // Therefore Local = World * inverse(ParentWorld).
            NewRelativeMatrix = NewWorldMatrix * Parent->GetWorldMatrix().GetInverse();
        }

        const FTransform NewRelativeTransform = FTransform::FromMatrix(NewRelativeMatrix);
        Component->SetRelativeLocation(NewRelativeTransform.GetLocation());
        Component->SetRelativeRotation(NewRelativeTransform.Rotation);
        Component->SetRelativeScale(NewRelativeTransform.Scale);
    }

    FTransform GetLocalTransform() const override
    {
        return IsValid() ? Component->GetRelativeTransform() : FTransform();
    }

    void SetLocalTransform(const FTransform& NewLocalTransform) override
    {
        if (!IsValid())
        {
            return;
        }
        Component->SetRelativeLocation(NewLocalTransform.GetLocation());
        Component->SetRelativeRotation(NewLocalTransform.Rotation);
        Component->SetRelativeScale(NewLocalTransform.Scale);
    }

private:
    USceneComponent* Component = nullptr;
    const FEditorViewportClient* OwnerViewportClient = nullptr;
    const bool* OwnerContextActiveFlag = nullptr;
    uint64 OwnerContextEpoch = 0;
};
