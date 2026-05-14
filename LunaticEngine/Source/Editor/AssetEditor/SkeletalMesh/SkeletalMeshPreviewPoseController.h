#pragma once

#include "Component/Gizmo/GizmoDeltaTarget.h"
#include "Mesh/SkeletonPose.h"

class USkeletalMeshComponent;

struct FBoneGizmoEditSession
{
    bool bActive = false;
    int32 BoneIndex = -1;
    FGizmoDragBeginContext DragContext;
    FTransform StartLocalTransform;
    FMatrix StartParentComponentMatrix = FMatrix::Identity;
    bool bHasParent = false;
};

class FSkeletalMeshPreviewPoseController
{
public:
    explicit FSkeletalMeshPreviewPoseController(USkeletalMeshComponent* InPreviewComponent = nullptr);

    void BindPreviewComponent(USkeletalMeshComponent* InPreviewComponent);
    void InitializeFromComponentPose();

    bool HasActiveBoneGizmoSession() const { return ActiveSession.bActive; }
    bool IsBoneValid(int32 BoneIndex) const;

    FTransform GetBoneLocalTransform(int32 BoneIndex) const;
    FTransform GetBoneComponentTransform(int32 BoneIndex) const;
    FMatrix GetBoneComponentMatrix(int32 BoneIndex) const;

    bool BeginBoneGizmoSession(int32 BoneIndex, const FGizmoDragBeginContext& Context);
    bool ApplyBoneGizmoDelta(int32 BoneIndex, const FGizmoDelta& Delta);
    void EndBoneGizmoSession(int32 BoneIndex, bool bCancelled);

    bool SetBoneLocalTransformFromUI(int32 BoneIndex, const FTransform& NewLocalTransform);
    bool ResetBoneLocalTransform(int32 BoneIndex);
    void ResetAllBoneTransforms();

private:
    void EnsurePoseInitialized() const;
    FVector ConvertComponentDeltaToParentDelta(const FBoneGizmoEditSession& Session, const FVector& DeltaComponent) const;
    bool ApplyLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform);
    void PushWorkingPoseToPreviewComponent();

private:
    USkeletalMeshComponent* PreviewComponent = nullptr;
    mutable FSkeletonPose WorkingPose;
    mutable bool bPoseInitialized = false;
    FBoneGizmoEditSession ActiveSession;
};
