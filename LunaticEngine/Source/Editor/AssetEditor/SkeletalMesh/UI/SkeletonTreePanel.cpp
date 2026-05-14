#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletonTreePanel.h"

#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"
#include "Common/UI/Panels/Panel.h"
#include "Common/UI/Style/EditorUIStyle.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
bool ContainsCaseInsensitive(const std::string& Text, const char* Pattern)
{
    if (!Pattern || Pattern[0] == '\0')
    {
        return true;
    }

    std::string LowerText = Text;
    std::string LowerPattern = Pattern;
    for (char& Ch : LowerText)
    {
        Ch = static_cast<char>(::tolower(static_cast<unsigned char>(Ch)));
    }
    for (char& Ch : LowerPattern)
    {
        Ch = static_cast<char>(::tolower(static_cast<unsigned char>(Ch)));
    }
    return LowerText.find(LowerPattern) != std::string::npos;
}

void CollectTreeVisibleBoneOrder(const TArray<TArray<int32>>& BoneChildren, int32 BoneIndex, TArray<int32>& OutOrder)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(BoneChildren.size()))
    {
        return;
    }

    OutOrder.push_back(BoneIndex);
    for (int32 ChildBoneIndex : BoneChildren[BoneIndex])
    {
        CollectTreeVisibleBoneOrder(BoneChildren, ChildBoneIndex, OutOrder);
    }
}
} // namespace

void FSkeletonTreePanel::Render(USkeletalMesh* Mesh, FSkeletalMeshEditorState& State,
                                FSkeletalMeshSelectionManager& SelectionManager, const FPanelDesc& PanelDesc)
{
    if (!FPanel::Begin(PanelDesc))
    {
        FPanel::End();
        return;
    }

    if (!Mesh)
    {
        ImGui::TextDisabled("No Skeleton.");
        FPanel::End();
        return;
    }

    const int32 BoneCount = Mesh->GetBoneCount();

    FEditorUIStyle::PushHeaderButtonStyle();
    if (ImGui::Button("Clear Selection"))
    {
        SelectionManager.ClearSelection();
        State.SelectedBoneIndex = -1;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Bones: %d / Selected: %d", BoneCount, SelectionManager.GetSelectedCount());
    FEditorUIStyle::PopHeaderButtonStyle();

    const float SearchWidth = (std::max)(120.0f, ImGui::GetContentRegionAvail().x);
    FEditorUIStyle::DrawSearchInputWithIcon("##SkeletalBoneSearch", "Search bone...", SearchBuffer, sizeof(SearchBuffer),
                                            SearchWidth);

    ImGui::Separator();

    if (BoneCount <= 0)
    {
        ImGui::TextDisabled("This mesh has no bone data.");
        ImGui::TextDisabled("FBX Importer가 skeleton/bone 데이터를 채우면 여기에 표시된다.");
        FPanel::End();
        return;
    }

    const TArray<FBoneInfo>& Bones = Mesh->GetBones();

    if (Bones.empty())
    {
        ImGui::TextDisabled("Bone count is valid, but bone array is empty.");
        FPanel::End();
        return;
    }

    TArray<int32> VisibleBoneOrder;
    const bool bHasSearch = SearchBuffer[0] != '\0';
    if (bHasSearch)
    {
        for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
        {
            const FBoneInfo& Bone = Bones[BoneIndex];
            const char* BoneName = Bone.Name.empty() ? "(unnamed bone)" : Bone.Name.c_str();
            if (ContainsCaseInsensitive(BoneName, SearchBuffer))
            {
                VisibleBoneOrder.push_back(BoneIndex);
            }
        }
    }
    else
    {
        const TArray<TArray<int32>>& BoneChildren = Mesh->GetBoneChildren();
        const TArray<int32>& RootBoneIndices = Mesh->GetRootBoneIndices();
        if (BoneChildren.size() == Bones.size())
        {
            for (int32 RootBoneIndex : RootBoneIndices)
            {
                CollectTreeVisibleBoneOrder(BoneChildren, RootBoneIndex, VisibleBoneOrder);
            }
        }
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput &&
        ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false))
    {
        SelectionManager.SelectAll(VisibleBoneOrder);
        SyncLegacySelectedBoneIndex(State, SelectionManager);
    }

    if (!FEditorUIStyle::BeginSkeletonTreeTable("##SkeletalBoneTreeTable"))
    {
        FPanel::End();
        return;
    }

    FEditorUIStyle::SetupSkeletonTreeTableColumns();

    if (bHasSearch)
    {
        DrawFilteredBoneList(Bones, State, SelectionManager, VisibleBoneOrder);
    }
    else
    {
        const TArray<TArray<int32>>& BoneChildren = Mesh->GetBoneChildren();
        const TArray<int32>& RootBoneIndices = Mesh->GetRootBoneIndices();

        if (RootBoneIndices.empty())
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("No root bone found.");
        }
        else if (BoneChildren.size() != Bones.size())
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("Bone hierarchy cache is invalid. Bones: %d, Children: %d",
                                static_cast<int32>(Bones.size()), static_cast<int32>(BoneChildren.size()));
        }
        else
        {
            std::vector<bool> AncestorHasNextSibling;
            for (int32 RootIndex = 0; RootIndex < static_cast<int32>(RootBoneIndices.size()); ++RootIndex)
            {
                const bool bIsLastRoot = RootIndex == static_cast<int32>(RootBoneIndices.size()) - 1;
                DrawBoneTreeNode(Bones, BoneChildren, RootBoneIndices[RootIndex], 0, bIsLastRoot, AncestorHasNextSibling,
                                 State, SelectionManager, VisibleBoneOrder);
            }
        }
    }

    FEditorUIStyle::EndSkeletonTreeTable();
    FPanel::End();
}

void FSkeletonTreePanel::DrawBoneTreeNode(const TArray<FBoneInfo>& Bones, const TArray<TArray<int32>>& BoneChildren,
                                          int32 BoneIndex, int32 Depth, bool bIsLastSibling,
                                          const std::vector<bool>& AncestorHasNextSibling,
                                          FSkeletalMeshEditorState& State,
                                          FSkeletalMeshSelectionManager& SelectionManager,
                                          const TArray<int32>& VisibleBoneOrder)
{
    const int32 BoneCount = static_cast<int32>(Bones.size());
    const int32 ChildrenCount = static_cast<int32>(BoneChildren.size());

    if (BoneIndex < 0 || BoneIndex >= BoneCount || BoneIndex >= ChildrenCount)
    {
        return;
    }

    const bool bHasChildren = !BoneChildren[BoneIndex].empty();

    const bool bOpen = DrawBoneRow(Bones[BoneIndex], BoneIndex, Depth, bHasChildren, false, bIsLastSibling,
                                   AncestorHasNextSibling, State, SelectionManager, VisibleBoneOrder);

    if (bHasChildren && bOpen)
    {
        std::vector<bool> ChildAncestorHasNextSibling = AncestorHasNextSibling;
        ChildAncestorHasNextSibling.push_back(!bIsLastSibling);

        const TArray<int32>& Children = BoneChildren[BoneIndex];
        for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(Children.size()); ++ChildIndex)
        {
            const bool bIsLastChild = ChildIndex == static_cast<int32>(Children.size()) - 1;
            DrawBoneTreeNode(Bones, BoneChildren, Children[ChildIndex], Depth + 1, bIsLastChild,
                             ChildAncestorHasNextSibling, State, SelectionManager, VisibleBoneOrder);
        }
    }
}

void FSkeletonTreePanel::DrawFilteredBoneList(const TArray<FBoneInfo>& Bones, FSkeletalMeshEditorState& State,
                                              FSkeletalMeshSelectionManager& SelectionManager,
                                              const TArray<int32>& VisibleBoneOrder)
{
    for (int32 BoneIndex : VisibleBoneOrder)
    {
        if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
        {
            continue;
        }

        DrawBoneRow(Bones[BoneIndex], BoneIndex, 0, false, true, true, std::vector<bool>(), State, SelectionManager, VisibleBoneOrder);
    }
}

bool FSkeletonTreePanel::DrawBoneRow(const FBoneInfo& Bone, int32 BoneIndex, int32 Depth, bool bHasChildren,
                                     bool bFilteredList, bool bIsLastSibling,
                                     const std::vector<bool>& AncestorHasNextSibling,
                                     FSkeletalMeshEditorState& State,
                                     FSkeletalMeshSelectionManager& SelectionManager,
                                     const TArray<int32>& VisibleBoneOrder)
{
    const bool bSelected = SelectionManager.IsSelected(BoneIndex);
    const char* BoneName = Bone.Name.empty() ? "(unnamed bone)" : Bone.Name.c_str();

    ImGui::TableNextRow(ImGuiTableRowFlags_None, FEditorUIStyle::SkeletonTreeRowHeight);
    ImGui::TableSetColumnIndex(0);

    ImGui::PushID(BoneIndex);

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    const ImVec2 RowMin = ImGui::GetCursorScreenPos();
    const float RowWidth = ImGui::GetContentRegionAvail().x;
    const ImVec2 RowMax(RowMin.x + RowWidth, RowMin.y + FEditorUIStyle::SkeletonTreeRowHeight);
    const float RowCenterY = (RowMin.y + RowMax.y) * 0.5f;

    const ImU32 TextColor = ImGui::GetColorU32(bSelected ? FEditorUIStyle::SkeletonTreeSelectedTextColor
                                                         : FEditorUIStyle::SkeletonTreeTextColor);
    const ImU32 IconTint = TextColor;
    const ImU32 LineColor = ImGui::GetColorU32(FEditorUIStyle::SkeletonTreeLineColor);
    const ImU32 RowHoverColor = ImGui::GetColorU32(ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
    const ImU32 RowSelectedColor = ImGui::GetColorU32(FEditorUIStyle::SkeletonTreeSelectionColor);

    ImGui::InvisibleButton("##BoneRowHit", ImVec2(RowWidth, FEditorUIStyle::SkeletonTreeRowHeight));
    const bool bHovered = ImGui::IsItemHovered();
    const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool bDoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    if (bSelected)
    {
        DrawList->AddRectFilled(RowMin, RowMax, RowSelectedColor);
    }
    else if (bHovered)
    {
        DrawList->AddRectFilled(RowMin, RowMax, RowHoverColor);
    }

    const float LeftPadding = 4.0f;
    const float IndentWidth = FEditorUIStyle::SkeletonTreeIndentWidth;
    const float BranchX = RowMin.x + LeftPadding + IndentWidth * static_cast<float>(Depth) + IndentWidth * 0.5f;
    const float ArrowSize = 8.0f;
    const float ConnectorLength = 11.0f;
    const float ConnectorEndX = BranchX + ConnectorLength;
    const float ConnectorToArrowGap = 4.0f;
    const float ArrowX = ConnectorEndX + ConnectorToArrowGap;
    const float IconSize = 13.0f;
    const float ArrowToIconGap = 9.0f;
    const float LeafConnectorToIconGap = 8.0f;
    const float IconToTextGap = 8.0f;
    const float IconX = bFilteredList
                            ? RowMin.x + LeftPadding
                            : (bHasChildren ? ArrowX + ArrowSize + ArrowToIconGap
                                            : ConnectorEndX + LeafConnectorToIconGap);
    const float IconY = RowCenterY - IconSize * 0.5f;
    const float TextX = IconX + IconSize + IconToTextGap;
    const float TextY = RowCenterY - ImGui::GetTextLineHeight() * 0.5f;

    if (!bFilteredList)
    {
        for (int32 AncestorDepth = 0; AncestorDepth < static_cast<int32>(AncestorHasNextSibling.size()); ++AncestorDepth)
        {
            if (!AncestorHasNextSibling[AncestorDepth])
            {
                continue;
            }

            const float GuideX = RowMin.x + LeftPadding + IndentWidth * static_cast<float>(AncestorDepth) + IndentWidth * 0.5f;
            DrawList->AddLine(ImVec2(GuideX, RowMin.y), ImVec2(GuideX, RowMax.y), LineColor, 1.0f);
        }

        if (Depth > 0)
        {
            DrawList->AddLine(ImVec2(BranchX, RowMin.y), ImVec2(BranchX, RowCenterY), LineColor, 1.0f);
            if (!bIsLastSibling)
            {
                DrawList->AddLine(ImVec2(BranchX, RowCenterY), ImVec2(BranchX, RowMax.y), LineColor, 1.0f);
            }
            // 현재 row의 가지선은 왼쪽 부모 줄에서 오른쪽 아이콘 방향으로만 짧게 뻗는다.
            // Bone 아이콘과 너무 붙어 보이지 않도록 ConnectorEndX에서 끊고, 이후에 공백을 둔다.
            DrawList->AddLine(ImVec2(BranchX, RowCenterY), ImVec2(ConnectorEndX, RowCenterY), LineColor, 1.0f);
        }
    }

    const bool bOpen = !bHasChildren || bFilteredList || IsBoneExpanded(BoneIndex);

    if (bHasChildren && !bFilteredList)
    {
        if (bOpen)
        {
            DrawList->AddTriangleFilled(
                ImVec2(ArrowX, RowCenterY - 2.0f),
                ImVec2(ArrowX + ArrowSize, RowCenterY - 2.0f),
                ImVec2(ArrowX + ArrowSize * 0.5f, RowCenterY + 4.0f),
                TextColor);
        }
        else
        {
            DrawList->AddTriangleFilled(
                ImVec2(ArrowX + 2.0f, RowCenterY - 4.0f),
                ImVec2(ArrowX + 2.0f, RowCenterY + 4.0f),
                ImVec2(ArrowX + 7.0f, RowCenterY),
                TextColor);
        }
    }

    if (ID3D11ShaderResourceView* BoneIcon = FEditorUIStyle::GetIcon("Editor.Icon.Bone"))
    {
        DrawList->AddImage(reinterpret_cast<ImTextureID>(BoneIcon), ImVec2(IconX, IconY),
                           ImVec2(IconX + IconSize, IconY + IconSize), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                           IconTint);
    }
    else
    {
        DrawList->AddText(ImVec2(IconX, TextY), TextColor, "*");
    }

    DrawList->AddText(ImVec2(TextX, TextY), TextColor, BoneName);

    if (bClicked || bDoubleClicked)
    {
        const float MouseX = ImGui::GetIO().MousePos.x;
        const bool bClickedDisclosure = bHasChildren && !bFilteredList && MouseX >= ArrowX - 4.0f && MouseX <= IconX;

        if (bClickedDisclosure || bDoubleClicked)
        {
            ToggleBoneExpanded(BoneIndex);
        }
        else
        {
            ApplyBoneClickSelection(BoneIndex, State, SelectionManager, VisibleBoneOrder);
        }
    }

    ImGui::PopID();
    return bOpen;
}

bool FSkeletonTreePanel::IsBoneExpanded(int32 BoneIndex) const
{
    return CollapsedBoneIndices.find(BoneIndex) == CollapsedBoneIndices.end();
}

void FSkeletonTreePanel::ToggleBoneExpanded(int32 BoneIndex)
{
    auto It = CollapsedBoneIndices.find(BoneIndex);
    if (It != CollapsedBoneIndices.end())
    {
        CollapsedBoneIndices.erase(It);
    }
    else
    {
        CollapsedBoneIndices.insert(BoneIndex);
    }
}

void FSkeletonTreePanel::ApplyBoneClickSelection(int32 BoneIndex, FSkeletalMeshEditorState& State,
                                                 FSkeletalMeshSelectionManager& SelectionManager,
                                                 const TArray<int32>& VisibleBoneOrder)
{
    if (ImGui::GetIO().KeyShift)
    {
        SelectionManager.SelectRange(BoneIndex, VisibleBoneOrder);
    }
    else if (ImGui::GetIO().KeyCtrl)
    {
        SelectionManager.ToggleBone(BoneIndex);
    }
    else
    {
        SelectionManager.SelectBone(BoneIndex);
    }

    SyncLegacySelectedBoneIndex(State, SelectionManager);
}

void FSkeletonTreePanel::SyncLegacySelectedBoneIndex(FSkeletalMeshEditorState& State,
                                                     const FSkeletalMeshSelectionManager& SelectionManager)
{
    State.SelectedBoneIndex = SelectionManager.GetPrimaryBoneIndex();
}
