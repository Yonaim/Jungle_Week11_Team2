#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshDetailsPanel.h"

#include "AssetEditor/SkeletalMesh/SkeletalMeshPreviewPoseController.h"
#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"
#include "Common/UI/Details/EditorDetailsWidgets.h"
#include "Common/UI/Style/EditorUIStyle.h"
#include "Component/SkeletalMeshComponent.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Math/Rotator.h"
#include "Mesh/SkeletonPose.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace
{
FTransform MakeTransformFromMatrix(const FMatrix &Matrix)
{
    FVector AxisX(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
    FVector AxisY(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);
    FVector AxisZ(Matrix.M[2][0], Matrix.M[2][1], Matrix.M[2][2]);

    FVector Scale(AxisX.Length(), AxisY.Length(), AxisZ.Length());
    if (Scale.X > 1e-6f)
    {
        AxisX /= Scale.X;
    }
    if (Scale.Y > 1e-6f)
    {
        AxisY /= Scale.Y;
    }
    if (Scale.Z > 1e-6f)
    {
        AxisZ /= Scale.Z;
    }

    FMatrix RotationMatrix = FMatrix::Identity;
    RotationMatrix.M[0][0] = AxisX.X;
    RotationMatrix.M[0][1] = AxisX.Y;
    RotationMatrix.M[0][2] = AxisX.Z;
    RotationMatrix.M[1][0] = AxisY.X;
    RotationMatrix.M[1][1] = AxisY.Y;
    RotationMatrix.M[1][2] = AxisY.Z;
    RotationMatrix.M[2][0] = AxisZ.X;
    RotationMatrix.M[2][1] = AxisZ.Y;
    RotationMatrix.M[2][2] = AxisZ.Z;

    return FTransform(Matrix.GetLocation(), FQuat::FromMatrix(RotationMatrix), Scale);
}

void DrawSmallIconButton(const char *Id, const char *Label, const char *Tooltip)
{
    FEditorUIStyle::PushHeaderButtonStyle(4.0f);
    ImGui::Button(Id, ImVec2(22.0f, 22.0f));
    FEditorUIStyle::PopHeaderButtonStyle();
    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 LabelSize = ImGui::CalcTextSize(Label);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(Min.x + (22.0f - LabelSize.x) * 0.5f, Min.y + (22.0f - LabelSize.y) * 0.5f),
        ImGui::GetColorU32(ImGuiCol_Text), Label);
    if (Tooltip && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", Tooltip);
    }
}

bool DrawModeButton(const char *Label, bool bSelected)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    if (bSelected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.24f, 0.26f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.32f, 1.0f));
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, FEditorUIStyle::HeaderButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FEditorUIStyle::HeaderButtonHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, FEditorUIStyle::HeaderButtonActiveColor);
    }
    ImGui::PushStyleColor(ImGuiCol_Border, FEditorUIStyle::HeaderButtonBorderColor);
    const bool bClicked = ImGui::Button(Label, ImVec2(96.0f, 24.0f));
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    return bClicked;
}

void DrawAxisField(const char *Id, const char *AxisLabel, float &Value, bool bReadOnly, float Width)
{
    ImGui::PushID(Id);
    ImGui::PushStyleColor(ImGuiCol_Text, FEditorUIStyle::DetailsVectorLabelColor);
    ImGui::TextUnformatted(AxisLabel);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(Width);
    FEditorUIStyle::PushDetailsVectorFieldStyle();
    if (bReadOnly)
    {
        ImGui::BeginDisabled();
    }
    ImGui::InputFloat("##Value", &Value, 0.0f, 0.0f, "%.6g");
    if (bReadOnly)
    {
        ImGui::EndDisabled();
    }
    FEditorUIStyle::PopDetailsVectorFieldStyle();
    ImGui::PopID();
}

bool DrawTransformRow(const char *Label, FVector &Values, bool bReadOnly)
{
    bool bChanged = false;
    ImGui::PushID(Label);
    if (ImGui::BeginTable("##TransformRow", 5,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, FEditorUIStyle::DetailsLabelWidth);
        ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(Label);

        const float AxisWidth = (std::max)(54.0f, ImGui::GetContentRegionAvail().x * 0.55f);
        ImGui::TableNextColumn();
        float OldX = Values.X;
        DrawAxisField("X", "X", Values.X, bReadOnly, AxisWidth);
        bChanged |= OldX != Values.X;

        ImGui::TableNextColumn();
        float OldY = Values.Y;
        DrawAxisField("Y", "Y", Values.Y, bReadOnly, AxisWidth);
        bChanged |= OldY != Values.Y;

        ImGui::TableNextColumn();
        float OldZ = Values.Z;
        DrawAxisField("Z", "Z", Values.Z, bReadOnly, AxisWidth);
        bChanged |= OldZ != Values.Z;

        ImGui::TableNextColumn();
        DrawSmallIconButton("##Reset", "↶", "Reset placeholder");
        ImGui::EndTable();
    }
    ImGui::PopID();
    return bChanged;
}

bool IsValidBone(USkeletalMesh *Mesh, int32 BoneIndex)
{
    return Mesh && BoneIndex >= 0 && BoneIndex < Mesh->GetBoneCount();
}
} // namespace

void FSkeletalMeshDetailsPanel::Render(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent,
                                       FSkeletalMeshPreviewPoseController *PoseController,
                                       FSkeletalMeshEditorState &State,
                                       FSkeletalMeshSelectionManager &SelectionManager,
                                       const FPanelDesc &PanelDesc)
{
    if (!FPanel::Begin(PanelDesc))
    {
        FPanel::End();
        return;
    }

    RenderSearchToolbar();

    if (!Mesh)
    {
        ImGui::TextDisabled("No SkeletalMesh asset selected.");
        FPanel::End();
        return;
    }

    RenderBoneSection(Mesh, SelectionManager);
    ImGui::Spacing();
    RenderTransformSection(Mesh, PreviewComponent, PoseController, State, SelectionManager);

    FPanel::End();
}

void FSkeletalMeshDetailsPanel::RenderSearchToolbar()
{
    static char SearchBuffer[128] = {};
    FEditorDetailsWidgets::DrawDetailsSearchToolbar("##SkeletalMeshDetailsSearch", SearchBuffer, sizeof(SearchBuffer));
}

void FSkeletalMeshDetailsPanel::RenderBoneSection(USkeletalMesh *Mesh, FSkeletalMeshSelectionManager &SelectionManager)
{
    if (!FEditorDetailsWidgets::BeginSection("Bone"))
    {
        return;
    }

    const int32 BoneIndex = SelectionManager.GetPrimaryBoneIndex();
    const char *BoneName = IsValidBone(Mesh, BoneIndex) ? Mesh->GetBoneName(BoneIndex) : "None";

    if (FEditorDetailsWidgets::BeginReadOnlyTable("##SkeletalMeshDetailsBoneTable"))
    {
        FEditorDetailsWidgets::DrawReadOnlyTextRow("Bone Name", BoneName);
        FEditorDetailsWidgets::DrawReadOnlyIntRow("Bone Index", BoneIndex);
        FEditorDetailsWidgets::DrawReadOnlyIntRow("Selected Count", SelectionManager.GetSelectedCount());
        ImGui::EndTable();
    }
}

void FSkeletalMeshDetailsPanel::RenderTransformSection(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent,
                                                       FSkeletalMeshPreviewPoseController *PoseController,
                                                       FSkeletalMeshEditorState &State,
                                                       FSkeletalMeshSelectionManager &SelectionManager)
{
    if (!FEditorDetailsWidgets::BeginSection("Transforms"))
    {
        return;
    }

    const int32 BoneIndex = SelectionManager.GetPrimaryBoneIndex();
    if (!IsValidBone(Mesh, BoneIndex))
    {
        ImGui::TextDisabled("Select a bone in Skeleton Tree or viewport.");
        return;
    }

    const FSkeletalMesh *MeshAsset = Mesh->GetSkeletalMeshAsset();
    if (!MeshAsset || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
    {
        ImGui::TextDisabled("Invalid skeletal mesh data.");
        return;
    }

    if (FEditorDetailsWidgets::DrawModeButton("Bone", State.BoneDetailsSpace == ESkeletalMeshBoneDetailsSpace::Bone))
    {
        State.BoneDetailsSpace = ESkeletalMeshBoneDetailsSpace::Bone;
    }
    ImGui::SameLine();
    if (FEditorDetailsWidgets::DrawModeButton("Reference", State.BoneDetailsSpace == ESkeletalMeshBoneDetailsSpace::Reference))
    {
        State.BoneDetailsSpace = ESkeletalMeshBoneDetailsSpace::Reference;
    }
    ImGui::SameLine();
    if (FEditorDetailsWidgets::DrawModeButton("Mesh Relative", State.BoneDetailsSpace == ESkeletalMeshBoneDetailsSpace::MeshRelative))
    {
        State.BoneDetailsSpace = ESkeletalMeshBoneDetailsSpace::MeshRelative;
    }

    FTransform DisplayTransform = MeshAsset->Bones[BoneIndex].LocalBindTransform;
    bool bCanEdit = false;

    if (PoseController)
    {
        if (State.BoneDetailsSpace == ESkeletalMeshBoneDetailsSpace::Bone)
        {
            DisplayTransform = PoseController->GetBoneLocalTransform(BoneIndex);
            bCanEdit = State.bEnablePoseEditMode;
        }
        else if (State.BoneDetailsSpace == ESkeletalMeshBoneDetailsSpace::MeshRelative)
        {
            DisplayTransform = PoseController->GetBoneComponentTransform(BoneIndex);
        }
    }
    else if (PreviewComponent)
    {
        const FSkeletonPose &Pose = PreviewComponent->GetCurrentPose();
        if (BoneIndex < static_cast<int32>(Pose.LocalTransforms.size()))
        {
            if (State.BoneDetailsSpace == ESkeletalMeshBoneDetailsSpace::Bone)
            {
                DisplayTransform = Pose.LocalTransforms[BoneIndex];
                bCanEdit = State.bEnablePoseEditMode;
            }
            else if (State.BoneDetailsSpace == ESkeletalMeshBoneDetailsSpace::MeshRelative &&
                     BoneIndex < static_cast<int32>(Pose.ComponentTransforms.size()))
            {
                DisplayTransform = MakeTransformFromMatrix(Pose.ComponentTransforms[BoneIndex]);
            }
        }
    }

    const bool bReadOnly = !bCanEdit;
    if (State.BoneDetailsSpace != ESkeletalMeshBoneDetailsSpace::Bone)
    {
        ImGui::TextDisabled("Reference / Mesh Relative values are displayed as read-only.");
    }
    else if (!State.bEnablePoseEditMode)
    {
        ImGui::TextDisabled("Enable Pose Edit Mode to edit Bone transform values.");
    }

    const FTransform &ResetTransform = MeshAsset->Bones[BoneIndex].LocalBindTransform;
    FVector Location = DisplayTransform.Location;
    FVector RotationEuler = DisplayTransform.Rotation.ToRotator().ToVector();
    FVector Scale = DisplayTransform.Scale;
    const FVector ResetLocation = ResetTransform.Location;
    const FVector ResetRotationEuler = ResetTransform.Rotation.ToRotator().ToVector();
    const FVector ResetScale = ResetTransform.Scale;

    ImGui::Spacing();
    bool bChanged = false;
    bChanged |= FEditorDetailsWidgets::DrawVector3Row("Location", Location, bReadOnly, bCanEdit, &ResetLocation);
    bChanged |= FEditorDetailsWidgets::DrawVector3Row("Rotation", RotationEuler, bReadOnly, bCanEdit, &ResetRotationEuler);
    bChanged |= FEditorDetailsWidgets::DrawVector3Row("Scale", Scale, bReadOnly, bCanEdit, &ResetScale);

    if (bChanged && bCanEdit)
    {
        FTransform NewLocal = DisplayTransform;
        NewLocal.Location = Location;
        NewLocal.Rotation = FRotator(RotationEuler).ToQuaternion();
        NewLocal.Scale = Scale;
        if (PoseController)
        {
            PoseController->SetBoneLocalTransformFromUI(BoneIndex, NewLocal);
        }
        else if (PreviewComponent)
        {
            PreviewComponent->SetBoneLocalTransform(BoneIndex, NewLocal);
            PreviewComponent->RefreshSkinningNow();
        }
    }
}
