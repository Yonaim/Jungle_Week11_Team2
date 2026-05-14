#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/Gizmo/BoneTransformGizmoTarget.h"

#include "AssetEditor/SkeletalMesh/SkeletalMeshPreviewPoseController.h"
#include "Common/Viewport/EditorViewportClient.h"
#include "Component/SkeletalMeshComponent.h"
#include "Engine/Mesh/SkeletalMesh.h"

#include <utility>

FBoneTransformGizmoTarget::FBoneTransformGizmoTarget(USkeletalMeshComponent* InComponent,
                                                     std::shared_ptr<FSkeletalMeshPreviewPoseController> InPoseController,
                                                     int32 InBoneIndex,
                                                     const FEditorViewportClient* InOwnerViewportClient,
                                                     const bool* InOwnerContextActiveFlag,
                                                     uint64 InOwnerContextEpoch)
    : Component(InComponent)
    , PoseController(std::move(InPoseController))
    , BoneIndex(InBoneIndex)
    , OwnerViewportClient(InOwnerViewportClient)
    , OwnerContextActiveFlag(InOwnerContextActiveFlag)
    , OwnerContextEpoch(InOwnerContextEpoch)
{
}

bool FBoneTransformGizmoTarget::IsValid() const
{
    if (OwnerViewportClient && !OwnerViewportClient->CanProcessLiveContextWork())
    {
        return false;
    }

    if (OwnerViewportClient && OwnerViewportClient->GetEditorContextEpoch() != OwnerContextEpoch)
    {
        return false;
    }

    if (OwnerContextActiveFlag && !(*OwnerContextActiveFlag))
    {
        return false;
    }

    if (!Component || BoneIndex < 0)
    {
        return false;
    }

    if (PoseController)
    {
        return PoseController->IsBoneValid(BoneIndex);
    }

    const FSkeletonPose& Pose = Component->GetCurrentPose();
    return BoneIndex < static_cast<int32>(Pose.LocalTransforms.size()) &&
           BoneIndex < static_cast<int32>(Pose.ComponentTransforms.size());
}

FTransform FBoneTransformGizmoTarget::GetWorldTransform() const
{
    return FTransform::FromMatrix(GetWorldMatrix());
}

void FBoneTransformGizmoTarget::SetWorldTransform(const FTransform& NewWorldTransform)
{
    // Hard isolation guard: stale targets from inactive asset tabs must never write.
    if (!IsValid())
    {
        return;
    }
    SetWorldMatrix(NewWorldTransform.ToMatrix());
}

FMatrix FBoneTransformGizmoTarget::GetWorldMatrix() const
{
    if (!IsValid())
    {
        return FMatrix::Identity;
    }

    FMatrix BoneComponentMatrix = FMatrix::Identity;
    if (PoseController)
    {
        BoneComponentMatrix = PoseController->GetBoneComponentMatrix(BoneIndex);
    }
    else
    {
        const FSkeletonPose& Pose = Component->GetCurrentPose();
        BoneComponentMatrix = Pose.ComponentTransforms[BoneIndex];
    }

    // Bone pose matrices are component-space. The gizmo manager works in world-space.
    // Keep this boundary explicit so rotate/scale never writes a component-space matrix
    // as if it were world-space.
    return Component ? BoneComponentMatrix * Component->GetWorldMatrix() : BoneComponentMatrix;
}

void FBoneTransformGizmoTarget::SetWorldMatrix(const FMatrix& NewWorldMatrix)
{
    if (!IsValid() || !Component || !Component->GetSkeletalMesh() || !Component->GetSkeletalMesh()->GetSkeletalMeshAsset())
    {
        return;
    }

    const FSkeletalMesh* MeshAsset = Component->GetSkeletalMesh()->GetSkeletalMeshAsset();
    const int32 ParentIndex = MeshAsset->Bones[BoneIndex].ParentIndex;

    // Convert gizmo world-space back into bone component-space first.
    // Row-major convention in this codebase: World = Local * ParentWorld.
    const FMatrix ComponentWorldMatrix = Component->GetWorldMatrix();
    const FMatrix NewBoneComponentMatrix = NewWorldMatrix * ComponentWorldMatrix.GetInverse();

    FMatrix NewLocalMatrix = NewBoneComponentMatrix;
    if (ParentIndex != InvalidBoneIndex)
    {
        FMatrix ParentComponentMatrix = FMatrix::Identity;
        if (PoseController)
        {
            ParentComponentMatrix = PoseController->GetBoneComponentMatrix(ParentIndex);
        }
        else
        {
            const FSkeletonPose& Pose = Component->GetCurrentPose();
            if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(Pose.ComponentTransforms.size()))
            {
                ParentComponentMatrix = Pose.ComponentTransforms[ParentIndex];
            }
        }

        NewLocalMatrix = NewBoneComponentMatrix * ParentComponentMatrix.GetInverse();
    }

    SetLocalTransform(FTransform::FromMatrix(NewLocalMatrix));
}

FTransform FBoneTransformGizmoTarget::GetLocalTransform() const
{
    if (!IsValid())
    {
        return FTransform();
    }

    if (PoseController)
    {
        return PoseController->GetBoneLocalTransform(BoneIndex);
    }

    const FSkeletonPose& Pose = Component->GetCurrentPose();
    return Pose.LocalTransforms[BoneIndex];
}

void FBoneTransformGizmoTarget::SetLocalTransform(const FTransform& NewLocalTransform)
{
    if (!IsValid())
    {
        return;
    }

    if (PoseController)
    {
        PoseController->SetBoneLocalTransformFromUI(BoneIndex, NewLocalTransform);
        return;
    }

    Component->SetBoneLocalTransform(BoneIndex, NewLocalTransform);
    Component->RefreshSkinningNow();
}

void FBoneTransformGizmoTarget::BeginTransform()
{
    bTransforming = IsValid();
}

void FBoneTransformGizmoTarget::EndTransform()
{
    if (!bTransforming)
    {
        return;
    }

    if (!PoseController && IsValid() && Component)
    {
        Component->RefreshSkinningForEditor(0.0f);
    }

    bTransforming = false;
}

bool FBoneTransformGizmoTarget::BeginGizmoDrag(const FGizmoDragBeginContext& Context)
{
    if (!PoseController || !IsValid())
    {
        return false;
    }

    return PoseController->BeginBoneGizmoSession(BoneIndex, Context);
}

bool FBoneTransformGizmoTarget::ApplyGizmoDelta(const FGizmoDelta& Delta)
{
    if (!PoseController || !IsValid())
    {
        return false;
    }

    return PoseController->ApplyBoneGizmoDelta(BoneIndex, Delta);
}

void FBoneTransformGizmoTarget::EndGizmoDrag(bool bCancelled)
{
    if (!PoseController || !bTransforming)
    {
        return;
    }

    // End only the session that this live target actually opened.  A hidden tab can
    // still own an old target object, but it must not cancel/commit another tab's pose.
    PoseController->EndBoneGizmoSession(BoneIndex, bCancelled);
}
