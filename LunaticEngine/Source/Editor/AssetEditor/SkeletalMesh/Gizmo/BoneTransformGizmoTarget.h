#pragma once

#include "Component/Gizmo/GizmoDeltaTarget.h"
#include "Component/Gizmo/TransformGizmoTarget.h"
#include "Core/CoreTypes.h"

#include <memory>

class USkeletalMeshComponent;
class FSkeletalMeshPreviewPoseController;
class FEditorViewportClient;

// SkeletalMeshEditor 전용 Bone Transform Gizmo Target.
// Bone은 SceneComponent가 아니므로 BoneIndex를 통해 USkeletalMeshComponent의 Pose를 수정한다.
class FBoneTransformGizmoTarget final : public ITransformGizmoTarget, public IGizmoDeltaTarget
{
public:
    FBoneTransformGizmoTarget(USkeletalMeshComponent* InComponent,
                              std::shared_ptr<FSkeletalMeshPreviewPoseController> InPoseController,
                              int32 InBoneIndex,
                              const FEditorViewportClient* InOwnerViewportClient,
                              const bool* InOwnerContextActiveFlag,
                              uint64 InOwnerContextEpoch);

    bool IsValid() const override;

    FTransform GetWorldTransform() const override;
    void SetWorldTransform(const FTransform& NewWorldTransform) override;
    FMatrix GetWorldMatrix() const override;
    void SetWorldMatrix(const FMatrix& NewWorldMatrix) override;

    FTransform GetLocalTransform() const override;
    void SetLocalTransform(const FTransform& NewLocalTransform) override;

    void BeginTransform() override;
    void EndTransform() override;
    bool BeginGizmoDrag(const FGizmoDragBeginContext& Context) override;
    bool ApplyGizmoDelta(const FGizmoDelta& Delta) override;
    void EndGizmoDrag(bool bCancelled) override;

    int32 GetBoneIndex() const { return BoneIndex; }

private:
    USkeletalMeshComponent* Component = nullptr;
    std::shared_ptr<FSkeletalMeshPreviewPoseController> PoseController;
    int32 BoneIndex = -1;
    const FEditorViewportClient* OwnerViewportClient = nullptr;
    const bool* OwnerContextActiveFlag = nullptr;
    uint64 OwnerContextEpoch = 0;
    bool bTransforming = false;
};
