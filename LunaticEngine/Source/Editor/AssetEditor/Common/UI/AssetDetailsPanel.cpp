#include "PCH/LunaticPCH.h"
#include "AssetEditor/Common/UI/AssetDetailsPanel.h"

#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"
#include "Common/File/EditorFileUtils.h"
#include "Core/Log.h"
#include "Core/Notification.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetFileSerializer.h"

#include "Component/SkeletalMeshComponent.h"
#include "Common/UI/Details/EditorDetailsWidgets.h"
#include "Common/UI/Style/EditorUIStyle.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Texture/Texture2D.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>
#include <filesystem>

namespace
{
const char *PreviewModeToText(ESkeletalMeshPreviewMode Mode)
{
    switch (Mode)
    {
    case ESkeletalMeshPreviewMode::ReferencePose:
        return "Reference Pose";
    case ESkeletalMeshPreviewMode::SkinnedPose:
        return "Skinned Pose";
    default:
        return "Unknown";
    }
}

const char *SafeText(const FString &Text)
{
    return Text.empty() ? "None" : Text.c_str();
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

std::filesystem::path MakeDefaultPoseSavePath(const std::filesystem::path &AssetPath)
{
    const std::filesystem::path PoseDir = std::filesystem::path(FPaths::ContentDir()) / L"Poses";
    const std::wstring Stem = AssetPath.empty() ? L"NewPose" : AssetPath.stem().wstring();
    return PoseDir / (Stem + L"_Pose.uasset");
}

FString MakeProjectRelativePath(const std::filesystem::path &Path)
{
    const std::filesystem::path Root(FPaths::RootDir());
    const std::filesystem::path Normalized = Path.lexically_normal();
    const std::filesystem::path Relative = Normalized.lexically_relative(Root);
    if (!Relative.empty() && Relative.native().find(L"..") != 0)
    {
        return FPaths::ToUtf8(Relative.generic_wstring());
    }
    return FPaths::ToUtf8(Normalized.generic_wstring());
}

void NotifyPoseTool(const FString &Message, ENotificationType Type = ENotificationType::Info)
{
    FNotificationManager::Get().AddNotification(Message, Type, 3.5f);
    UE_LOG_CATEGORY(AssetEditor, Info, "%s", Message.c_str());
}
} // namespace

bool FAssetDetailsPanel::RenderSkeletalMesh(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent,
                                            const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State,
                                            FSkeletalMeshSelectionManager &SelectionManager,
                                            const FPanelDesc &PanelDesc)
{
    if (!FPanel::Begin(PanelDesc))
    {
        FPanel::End();
        return false;
    }

    RenderSearchToolbar();

    if (!Mesh)
    {
        ImGui::TextDisabled("No SkeletalMesh asset selected.");
        FPanel::End();
        return false;
    }

    bool bChanged = false;
    bChanged |= RenderMaterialSlots(Mesh, PreviewComponent, State);
    ImGui::Spacing();
    RenderMeshInfo(Mesh, AssetPath, State, SelectionManager);
    ImGui::Spacing();
    RenderViewerActions(State, SelectionManager);
    ImGui::Spacing();
    RenderPoseAssetTools(Mesh, PreviewComponent, AssetPath);
    ImGui::Spacing();
    RenderBoneSelectionSummary(State, SelectionManager);

    FPanel::End();
    return bChanged;
}

void FAssetDetailsPanel::RenderSearchToolbar()
{
    static char SearchBuffer[128] = {};
    FEditorDetailsWidgets::DrawDetailsSearchToolbar("##AssetDetailsSearch", SearchBuffer, sizeof(SearchBuffer));
}

bool FAssetDetailsPanel::RenderMaterialSlots(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent,
                                             FSkeletalMeshEditorState &State)
{
    if (!FEditorDetailsWidgets::BeginSection("Material Slots"))
    {
        return false;
    }

    TArray<FStaticMaterial> &Materials = Mesh->GetStaticMaterialsMutable();
    const int32 MaterialCount = static_cast<int32>(Materials.size());
    bool bChangedAnySlot = false;

    if (FEditorDetailsWidgets::BeginReadOnlyTable("##AssetDetailsMaterialSlotSummary"))
    {
        FEditorDetailsWidgets::DrawReadOnlyIntRow("Material Slots", MaterialCount);
        ImGui::EndTable();
    }

    if (MaterialCount <= 0)
    {
        ImGui::TextDisabled("No material slots.");
        return false;
    }

    ImGui::Spacing();
    for (int32 Index = 0; Index < MaterialCount; ++Index)
    {
        const FStaticMaterial &StaticSlot = Materials[Index];
        FMaterialSlot Slot;
        Slot.Path = StaticSlot.MaterialInterface ? StaticSlot.MaterialInterface->GetAssetPathFileName() : "None";

        ImGui::PushID(Index);
        const bool bChanged = FEditorDetailsWidgets::DrawMaterialSlotRow({
            Index,
            StaticSlot.MaterialSlotName.empty() ? "None" : StaticSlot.MaterialSlotName.c_str(),
            &Slot,
            nullptr,
            false,
            true,
            false,
            120.0f,
        });
        if (bChanged)
        {
            UMaterial *NewMaterial =
                (Slot.Path.empty() || Slot.Path == "None") ? nullptr : FMaterialManager::Get().GetOrCreateMaterial(Slot.Path);
            Mesh->SetStaticMaterialInterface(Index, NewMaterial);
            if (PreviewComponent)
            {
                PreviewComponent->SetMaterial(Index, NewMaterial);
            }
            bChangedAnySlot = true;
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            State.SelectedMaterialSlotIndex = Index;
        }

        if (Index + 1 < MaterialCount)
        {
            ImGui::Spacing();
        }
        ImGui::PopID();
    }

    return bChangedAnySlot;
}

void FAssetDetailsPanel::RenderMeshInfo(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath,
                                        FSkeletalMeshEditorState &State,
                                        FSkeletalMeshSelectionManager &SelectionManager)
{
    if (!FEditorUIStyle::BeginDetailsSection("Mesh Info"))
    {
        return;
    }

    const FString FileName = AssetPath.empty() ? FString("Untitled") : FPaths::ToUtf8(AssetPath.filename().wstring());
    const FString FullPath = AssetPath.empty() ? FString("") : FPaths::ToUtf8(AssetPath.wstring());

    if (FEditorUIStyle::BeginDetailsReadOnlyTable("##AssetDetailsSkeletalMeshInfoTable"))
    {
        FEditorUIStyle::DrawReadOnlyTextRow("Asset", FileName.c_str());
        if (!FullPath.empty() && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", FullPath.c_str());
        }

        FEditorUIStyle::DrawReadOnlyIntRow("Bone Count", Mesh->GetBoneCount());
        FEditorUIStyle::DrawReadOnlyIntRow("Vertex Count", Mesh->GetVertexCount());
        FEditorUIStyle::DrawReadOnlyIntRow("Index Count", Mesh->GetIndexCount());
        FEditorUIStyle::DrawReadOnlyTextRow("Preview Mode", PreviewModeToText(State.PreviewMode));
        ImGui::EndTable();
    }
}

void FAssetDetailsPanel::RenderViewerActions(FSkeletalMeshEditorState &State, FSkeletalMeshSelectionManager &SelectionManager)
{
    if (!FEditorUIStyle::BeginDetailsSection("Viewer Actions"))
    {
        return;
    }

    FEditorUIStyle::PushHeaderButtonStyle();
    if (ImGui::Button("Frame Mesh"))
    {
        State.bFramePreviewRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Bone Selection"))
    {
        SelectionManager.ClearSelection();
        State.SelectedBoneIndex = -1;
    }
    FEditorUIStyle::PopHeaderButtonStyle();

    ImGui::Checkbox("Show Mesh Stats Overlay", &State.bShowMeshStatsOverlay);
    ImGui::Checkbox("Show Bones", &State.bShowBones);
}

void FAssetDetailsPanel::RenderBoneSelectionSummary(FSkeletalMeshEditorState &State,
                                                    FSkeletalMeshSelectionManager &SelectionManager)
{
    if (!FEditorUIStyle::BeginDetailsSection("Bone Editing"))
    {
        return;
    }

    ImGui::Checkbox("Pose Edit Mode", &State.bEnablePoseEditMode);

    if (FEditorUIStyle::BeginDetailsReadOnlyTable("##AssetDetailsBoneSelectionSummary"))
    {
        FEditorUIStyle::DrawReadOnlyIntRow("Primary Bone Index", SelectionManager.GetPrimaryBoneIndex());
        FEditorUIStyle::DrawReadOnlyIntRow("Selected Bone Count", SelectionManager.GetSelectedCount());
        ImGui::EndTable();
    }
}


void FAssetDetailsPanel::RenderPoseAssetTools(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent, const std::filesystem::path &AssetPath)
{
    if (!FEditorUIStyle::BeginDetailsSection("Pose Asset"))
    {
        return;
    }

    if (!Mesh || !PreviewComponent || !Mesh->GetSkeletalMeshAsset())
    {
        ImGui::TextDisabled("No preview pose is available.");
        return;
    }

    const FSkeletalMesh *MeshAsset = Mesh->GetSkeletalMeshAsset();
    const FSkeletonPose &CurrentPose = PreviewComponent->GetCurrentPose();
    const int32 BoneCount = static_cast<int32>(MeshAsset->Bones.size());

    if (FEditorUIStyle::BeginDetailsReadOnlyTable("##PoseAssetSummary"))
    {
        FEditorUIStyle::DrawReadOnlyTextRow("Type", "Pose.uasset");
        FEditorUIStyle::DrawReadOnlyTextRow("Space", "Local");
        FEditorUIStyle::DrawReadOnlyIntRow("Bone Transforms", BoneCount);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    FEditorUIStyle::PushHeaderButtonStyle();
    if (ImGui::Button("Save Current Pose..."))
    {
        const std::filesystem::path DefaultPath = MakeDefaultPoseSavePath(AssetPath);
        std::filesystem::create_directories(DefaultPath.parent_path());
        const std::wstring DefaultDirectory = DefaultPath.parent_path().wstring();
        const std::wstring DefaultFileName = DefaultPath.filename().wstring();
        const FString SavePathUtf8 = FEditorFileUtils::SaveFileDialog({
            .Filter = L"Pose Asset (*.uasset)\0*.uasset\0All Files (*.*)\0*.*\0",
            .Title = L"Save Skeleton Pose Asset",
            .DefaultExtension = L"uasset",
            .InitialDirectory = DefaultDirectory.c_str(),
            .DefaultFileName = DefaultFileName.c_str(),
            .bFileMustExist = false,
            .bPathMustExist = true,
            .bPromptOverwrite = true,
            .bReturnRelativeToProjectRoot = false,
        });

        if (!SavePathUtf8.empty())
        {
            std::filesystem::path SavePath(FPaths::ToWide(SavePathUtf8));
            if (SavePath.extension().empty())
            {
                SavePath += L".uasset";
            }
            std::filesystem::create_directories(SavePath.parent_path());

            USkeletonPoseAsset *PoseAsset = UObjectManager::Get().CreateObject<USkeletonPoseAsset>();
            PoseAsset->SetFName(FName(FPaths::ToUtf8(SavePath.stem().wstring())));
            PoseAsset->TargetSkeletonPath = MeshAsset->PathFileName.empty()
                ? MakeProjectRelativePath(AssetPath)
                : MeshAsset->PathFileName;
            PoseAsset->Space = "Local";
            PoseAsset->BoneTransforms.resize(BoneCount);

            for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
            {
                FPoseBoneTransform &PoseBone = PoseAsset->BoneTransforms[BoneIndex];
                PoseBone.BoneIndex = BoneIndex;
                PoseBone.BoneName = MeshAsset->Bones[BoneIndex].Name;
                if (BoneIndex < static_cast<int32>(CurrentPose.LocalTransforms.size()))
                {
                    PoseBone.LocalTransform = CurrentPose.LocalTransforms[BoneIndex];
                }
                else
                {
                    PoseBone.LocalTransform = MeshAsset->Bones[BoneIndex].LocalBindTransform;
                }
            }

            FString Error;
            const bool bSaved = FAssetFileSerializer::SaveObjectToAssetFile(SavePath, PoseAsset, &Error);
            UObjectManager::Get().DestroyObject(PoseAsset);

            if (bSaved)
            {
                const FString RelativePath = MakeProjectRelativePath(SavePath);
                NotifyPoseTool("Saved pose asset: " + RelativePath, ENotificationType::Success);
            }
            else
            {
                NotifyPoseTool(Error.empty() ? "Failed to save pose asset." : Error, ENotificationType::Error);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Pose..."))
    {
        const std::filesystem::path InitialDir = std::filesystem::path(FPaths::ContentDir()) / L"Poses";
        std::filesystem::create_directories(InitialDir);
        const FString OpenPathUtf8 = FEditorFileUtils::OpenFileDialog({
            .Filter = L"Pose Asset (*.uasset)\0*.uasset\0All Files (*.*)\0*.*\0",
            .Title = L"Load Skeleton Pose Asset",
            .InitialDirectory = InitialDir.c_str(),
            .bFileMustExist = true,
            .bPathMustExist = true,
            .bPromptOverwrite = false,
            .bReturnRelativeToProjectRoot = false,
        });

        if (!OpenPathUtf8.empty())
        {
            const std::filesystem::path OpenPath(FPaths::ToWide(OpenPathUtf8));
            FString Error;
            UObject *LoadedObject = FAssetFileSerializer::LoadObjectFromAssetFile(OpenPath, &Error);
            USkeletonPoseAsset *PoseAsset = Cast<USkeletonPoseAsset>(LoadedObject);
            if (!PoseAsset)
            {
                if (LoadedObject)
                {
                    UObjectManager::Get().DestroyObject(LoadedObject);
                }
                NotifyPoseTool(Error.empty() ? "Selected asset is not a Skeleton Pose asset." : Error, ENotificationType::Error);
            }
            else
            {
                TArray<FTransform> LocalTransforms;
                LocalTransforms.resize(BoneCount);
                for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
                {
                    LocalTransforms[BoneIndex] = MeshAsset->Bones[BoneIndex].LocalBindTransform;
                }

                int32 AppliedCount = 0;
                for (const FPoseBoneTransform &PoseBone : PoseAsset->BoneTransforms)
                {
                    int32 TargetBoneIndex = PoseBone.BoneIndex;
                    if (TargetBoneIndex < 0 || TargetBoneIndex >= BoneCount ||
                        (!PoseBone.BoneName.empty() && MeshAsset->Bones[TargetBoneIndex].Name != PoseBone.BoneName))
                    {
                        TargetBoneIndex = -1;
                        for (int32 SearchIndex = 0; SearchIndex < BoneCount; ++SearchIndex)
                        {
                            if (MeshAsset->Bones[SearchIndex].Name == PoseBone.BoneName)
                            {
                                TargetBoneIndex = SearchIndex;
                                break;
                            }
                        }
                    }

                    if (TargetBoneIndex >= 0 && TargetBoneIndex < BoneCount)
                    {
                        LocalTransforms[TargetBoneIndex] = PoseBone.LocalTransform;
                        ++AppliedCount;
                    }
                }

                if (PreviewComponent->ApplyLocalPoseTransforms(LocalTransforms))
                {
                    NotifyPoseTool("Loaded pose asset: " + MakeProjectRelativePath(OpenPath) + " (" + std::to_string(AppliedCount) + " bones)", ENotificationType::Success);
                }
                else
                {
                    NotifyPoseTool("Failed to apply pose asset.", ENotificationType::Error);
                }
                UObjectManager::Get().DestroyObject(PoseAsset);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Bind Pose"))
    {
        PreviewComponent->ResetToBindPose();
        NotifyPoseTool("Reset preview pose to bind pose.", ENotificationType::Info);
    }
    FEditorUIStyle::PopHeaderButtonStyle();
}
