#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshPreviewPoseController.h"

#include "Component/SkeletalMeshComponent.h"
#include "Engine/Mesh/SkeletalMesh.h"

namespace
{
const FSkeletalMesh* GetSkeletalMeshAsset(const USkeletalMeshComponent* Component)
{
    if (!Component || !Component->GetSkeletalMesh())
    {
        return nullptr;
    }

    return Component->GetSkeletalMesh()->GetSkeletalMeshAsset();
}
}

FSkeletalMeshPreviewPoseController::FSkeletalMeshPreviewPoseController(USkeletalMeshComponent* InPreviewComponent)
{
    BindPreviewComponent(InPreviewComponent);
}

void FSkeletalMeshPreviewPoseController::BindPreviewComponent(USkeletalMeshComponent* InPreviewComponent)
{
    if (PreviewComponent == InPreviewComponent)
    {
        return;
    }

    PreviewComponent = InPreviewComponent;
    ActiveSession = FBoneGizmoEditSession{};
    InitializeFromComponentPose();
}

void FSkeletalMeshPreviewPoseController::InitializeFromComponentPose()
{
    ActiveSession = FBoneGizmoEditSession{};

    const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(PreviewComponent);
    if (!PreviewComponent || !MeshAsset)
    {
        WorkingPose.Reset();
        bPoseInitialized = false;
        return;
    }

    WorkingPose = PreviewComponent->GetCurrentPose();
    const int32 BoneCount = static_cast<int32>(MeshAsset->Bones.size());
    bPoseInitialized =
        static_cast<int32>(WorkingPose.LocalTransforms.size()) == BoneCount &&
        static_cast<int32>(WorkingPose.ComponentTransforms.size()) == BoneCount;

    if (!bPoseInitialized)
    {
        WorkingPose.Reset();
    }
}

bool FSkeletalMeshPreviewPoseController::IsBoneValid(int32 BoneIndex) const
{
    EnsurePoseInitialized();
    return BoneIndex >= 0 &&
           BoneIndex < static_cast<int32>(WorkingPose.LocalTransforms.size()) &&
           BoneIndex < static_cast<int32>(WorkingPose.ComponentTransforms.size());
}

FTransform FSkeletalMeshPreviewPoseController::GetBoneLocalTransform(int32 BoneIndex) const
{
    if (!IsBoneValid(BoneIndex))
    {
        return FTransform();
    }

    return WorkingPose.LocalTransforms[BoneIndex];
}

FTransform FSkeletalMeshPreviewPoseController::GetBoneComponentTransform(int32 BoneIndex) const
{
    if (!IsBoneValid(BoneIndex))
    {
        return FTransform();
    }

    return FTransform::FromMatrix(WorkingPose.ComponentTransforms[BoneIndex]);
}

FMatrix FSkeletalMeshPreviewPoseController::GetBoneComponentMatrix(int32 BoneIndex) const
{
    if (!IsBoneValid(BoneIndex))
    {
        return FMatrix::Identity;
    }

    return WorkingPose.ComponentTransforms[BoneIndex];
}

bool FSkeletalMeshPreviewPoseController::BeginBoneGizmoSession(int32 BoneIndex, const FGizmoDragBeginContext& Context)
{
    if (!IsBoneValid(BoneIndex))
    {
        return false;
    }

    const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(PreviewComponent);
    if (!MeshAsset || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
    {
        return false;
    }

    ActiveSession = FBoneGizmoEditSession{};
    ActiveSession.bActive = true;
    ActiveSession.BoneIndex = BoneIndex;
    ActiveSession.DragContext = Context;
    ActiveSession.StartLocalTransform = WorkingPose.LocalTransforms[BoneIndex];

    const int32 ParentIndex = MeshAsset->Bones[BoneIndex].ParentIndex;
    if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(WorkingPose.ComponentTransforms.size()))
    {
        ActiveSession.bHasParent = true;
        ActiveSession.StartParentComponentMatrix = WorkingPose.ComponentTransforms[ParentIndex];
    }

    return true;
}

bool FSkeletalMeshPreviewPoseController::ApplyBoneGizmoDelta(int32 BoneIndex, const FGizmoDelta& Delta)
{
    if (!ActiveSession.bActive || ActiveSession.BoneIndex != BoneIndex || Delta.Mode != EGizmoMode::Translate)
    {
        return false;
    }

    const FVector DeltaParent = ConvertComponentDeltaToParentDelta(ActiveSession, Delta.LinearDeltaComponent);

    FTransform NewLocal = ActiveSession.StartLocalTransform;
    NewLocal.Location = ActiveSession.StartLocalTransform.Location + DeltaParent;
    return ApplyLocalTransform(BoneIndex, NewLocal);
}

void FSkeletalMeshPreviewPoseController::EndBoneGizmoSession(int32 BoneIndex, bool bCancelled)
{
    if (!ActiveSession.bActive || ActiveSession.BoneIndex != BoneIndex)
    {
        ActiveSession = FBoneGizmoEditSession{};
        return;
    }

    if (bCancelled)
    {
        ApplyLocalTransform(BoneIndex, ActiveSession.StartLocalTransform);
    }

    ActiveSession = FBoneGizmoEditSession{};
}

bool FSkeletalMeshPreviewPoseController::SetBoneLocalTransformFromUI(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (!IsBoneValid(BoneIndex))
    {
        return false;
    }

    return ApplyLocalTransform(BoneIndex, NewLocalTransform);
}

bool FSkeletalMeshPreviewPoseController::ResetBoneLocalTransform(int32 BoneIndex)
{
    const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(PreviewComponent);
    if (!MeshAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
    {
        return false;
    }

    return ApplyLocalTransform(BoneIndex, MeshAsset->Bones[BoneIndex].LocalBindTransform);
}

void FSkeletalMeshPreviewPoseController::ResetAllBoneTransforms()
{
    const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(PreviewComponent);
    if (!MeshAsset)
    {
        WorkingPose.Reset();
        bPoseInitialized = false;
        ActiveSession = FBoneGizmoEditSession{};
        return;
    }

    WorkingPose.InitializeFromBindPose(*MeshAsset);
    bPoseInitialized = true;
    ActiveSession = FBoneGizmoEditSession{};
    PushWorkingPoseToPreviewComponent();
}

void FSkeletalMeshPreviewPoseController::EnsurePoseInitialized() const
{
    if (!bPoseInitialized)
    {
        const_cast<FSkeletalMeshPreviewPoseController*>(this)->InitializeFromComponentPose();
    }
}

FVector FSkeletalMeshPreviewPoseController::ConvertComponentDeltaToParentDelta(
    const FBoneGizmoEditSession& Session,
    const FVector& DeltaComponent) const
{
    if (!Session.bHasParent)
    {
        return DeltaComponent;
    }

    const FMatrix ParentInverse = Session.StartParentComponentMatrix.GetInverse();
    return ParentInverse.TransformVector(DeltaComponent);
}

bool FSkeletalMeshPreviewPoseController::ApplyLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(PreviewComponent);
    if (!MeshAsset || !IsBoneValid(BoneIndex))
    {
        return false;
    }

    if (!WorkingPose.SetLocalTransform(BoneIndex, NewLocalTransform))
    {
        return false;
    }

    WorkingPose.RebuildComponentSpace(MeshAsset->Bones);
    PushWorkingPoseToPreviewComponent();
    return true;
}

void FSkeletalMeshPreviewPoseController::PushWorkingPoseToPreviewComponent()
{
    if (!PreviewComponent || !bPoseInitialized)
    {
        return;
    }

    PreviewComponent->SetPreviewPoseForEditor(WorkingPose);
}
