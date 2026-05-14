#pragma once

#include "Common/UI/Style/EditorUIStyle.h"
#include "Core/PropertyTypes.h"
#include "LevelEditor/UI/Panels/ContentBrowser/ContentItem.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Math/Vector.h"
#include "Platform/Paths.h"
#include "Texture/Texture2D.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <string>

// Level Editor Details와 Asset Editor Details가 같이 쓰는 Details 패널용 공통 위젯.
// 패널별 데이터 수집은 각 패널이 맡고, 실제 row/field 스타일은 여기서 맞춘다.
namespace FEditorDetailsWidgets
{
inline FString RemoveExtension(const FString &Path)
{
    const size_t DotPos = Path.find_last_of('.');
    return DotPos == FString::npos ? Path : Path.substr(0, DotPos);
}

inline FString GetStemFromPath(const FString &Path)
{
    const size_t SlashPos = Path.find_last_of("/\\");
    const FString FileName = SlashPos == FString::npos ? Path : Path.substr(SlashPos + 1);
    return RemoveExtension(FileName);
}

inline FString MakeAssetPreviewLabel(const FString &Path)
{
    return (Path.empty() || Path == "None") ? FString("None") : GetStemFromPath(Path);
}

inline bool IsNoneAssetPath(const FString &Path)
{
    return Path.empty() || Path == "None";
}

inline bool AssetPathsMatch(const FString &A, const FString &B)
{
    const FString NormalizedA = IsNoneAssetPath(A) ? FString("None") : A;
    const FString NormalizedB = IsNoneAssetPath(B) ? FString("None") : B;
    return NormalizedA == NormalizedB;
}

inline void DrawSmallIconButton(const char *Id, const char *Label, const char *Tooltip = nullptr)
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

inline void DrawDetailsSearchToolbar(const char *Id, char *SearchBuffer, size_t SearchBufferSize)
{
    const float ButtonSize = ImGui::GetFrameHeight();
    const float SearchWidth = (std::max)(80.0f, ImGui::GetContentRegionAvail().x - ButtonSize * 2.0f - 12.0f);

    FEditorUIStyle::DrawSearchInputWithIcon(Id, "Search", SearchBuffer, SearchBufferSize, SearchWidth);
    ImGui::SameLine();
    DrawSmallIconButton("##DetailsViewOptions", "V", "View Options");
    ImGui::SameLine();
    DrawSmallIconButton("##DetailsSettings", "S", "Settings");
    ImGui::Spacing();
}

inline bool BeginSection(const char *SectionName)
{
    return FEditorUIStyle::BeginDetailsSection(SectionName);
}

inline bool BeginReadOnlyTable(const char *Id, float LabelColumnWidth = FEditorUIStyle::DetailsLabelWidth)
{
    return FEditorUIStyle::BeginDetailsReadOnlyTable(Id, LabelColumnWidth);
}

inline void DrawReadOnlyTextRow(const char *Label, const char *Value)
{
    FEditorUIStyle::DrawReadOnlyTextRow(Label, Value);
}

inline void DrawReadOnlyIntRow(const char *Label, int32 Value)
{
    FEditorUIStyle::DrawReadOnlyIntRow(Label, Value);
}

inline bool DrawModeButton(const char *Label, bool bSelected, float Width = 96.0f)
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
    const bool bClicked = ImGui::Button(Label, ImVec2(Width, 24.0f));
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    return bClicked;
}

inline void DrawAxisField(const char *Id, const char *AxisLabel, float &Value, bool bReadOnly, float Width)
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

inline bool DrawVector3Row(const char *Label, FVector &Values, bool bReadOnly, bool bShowReset = true,
                           const FVector *ResetValues = nullptr)
{
    bool bChanged = false;
    ImGui::PushID(Label);
    if (ImGui::BeginTable("##Vector3Row", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, FEditorUIStyle::DetailsLabelWidth);
        ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, bShowReset ? 28.0f : 4.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(Label);

        const float AxisWidth = (std::max)(54.0f, ImGui::GetContentRegionAvail().x * 0.55f);
        ImGui::TableNextColumn();
        const float OldX = Values.X;
        DrawAxisField("X", "X", Values.X, bReadOnly, AxisWidth);
        bChanged |= OldX != Values.X;

        ImGui::TableNextColumn();
        const float OldY = Values.Y;
        DrawAxisField("Y", "Y", Values.Y, bReadOnly, AxisWidth);
        bChanged |= OldY != Values.Y;

        ImGui::TableNextColumn();
        const float OldZ = Values.Z;
        DrawAxisField("Z", "Z", Values.Z, bReadOnly, AxisWidth);
        bChanged |= OldZ != Values.Z;

        ImGui::TableNextColumn();
        if (bShowReset)
        {
            if (bReadOnly)
            {
                ImGui::BeginDisabled();
            }

            const bool bResetClicked = ImGui::Button("##Reset", ImVec2(22.0f, 22.0f));
            const ImVec2 Min = ImGui::GetItemRectMin();
            const ImVec2 LabelSize = ImGui::CalcTextSize("R");
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(Min.x + (22.0f - LabelSize.x) * 0.5f, Min.y + (22.0f - LabelSize.y) * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text), "R");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Reset");
            }

            if (bReadOnly)
            {
                ImGui::EndDisabled();
            }

            if (bResetClicked && !bReadOnly && ResetValues)
            {
                Values = *ResetValues;
                bChanged = true;
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
    return bChanged;
}

struct FMaterialSlotWidgetArgs
{
    int32 ElementIndex = -1;
    const char *SlotName = "None";
    FMaterialSlot *Slot = nullptr;
    const char *ResetPath = nullptr;
    bool bReadOnly = false;
    bool bAllowDragDrop = true;
    bool bShowPreviewImages = true;
    float LeftColumnWidth = 120.0f;
};

inline bool DrawMaterialSlotRow(const FMaterialSlotWidgetArgs &Args)
{
    if (!Args.Slot)
    {
        return false;
    }

    bool bChanged = false;
    const FString SlotName = (Args.SlotName && Args.SlotName[0] != '\0') ? FString(Args.SlotName) : FString("None");

    ImGui::PushID(Args.ElementIndex >= 0 ? Args.ElementIndex : 0);
    ImGui::BeginGroup();
    if (Args.ElementIndex >= 0)
    {
        ImGui::Text("Element %d", Args.ElementIndex);
    }
    else
    {
        ImGui::TextUnformatted("Element");
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::TextUnformatted(SlotName.c_str());
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", SlotName.c_str());
    }
    ImGui::EndGroup();

    ImGui::SameLine(Args.LeftColumnWidth);

    ImGui::BeginGroup();
    constexpr float PreviewImageSize = 26.0f;
    constexpr float PreviewSpacing = 6.0f;
    constexpr float ResetButtonSize = 22.0f;

    const FString ResetPath = (Args.ResetPath && Args.ResetPath[0] != '\0') ? FString(Args.ResetPath) : FString();
    const bool bShowResetButton = !Args.bReadOnly && !ResetPath.empty();
    const bool bCanReset = bShowResetButton && !AssetPathsMatch(Args.Slot->Path, ResetPath);

    UMaterial *CurrentMaterial = nullptr;
    UTexture2D *CurrentPreviewTexture = nullptr;
    if (Args.bShowPreviewImages)
    {
        CurrentMaterial = (Args.Slot->Path.empty() || Args.Slot->Path == "None")
                              ? nullptr
                              : FMaterialManager::Get().GetOrCreateMaterial(Args.Slot->Path);
        CurrentPreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(CurrentMaterial);
    }
    const float ReservedPreviewWidth =
        (CurrentPreviewTexture && CurrentPreviewTexture->GetSRV()) ? (PreviewImageSize + PreviewSpacing) : 0.0f;
    const float ReservedResetWidth = bShowResetButton ? (ResetButtonSize + PreviewSpacing) : 0.0f;
    float ComboWidth = ImGui::GetContentRegionAvail().x - ReservedPreviewWidth - ReservedResetWidth;
    if (ComboWidth < 120.0f)
    {
        ComboWidth = 120.0f;
    }

    FEditorUIStyle::PushDetailsPropertyEditStyle();
    ImGui::SetNextItemWidth(ComboWidth);
    const FString Preview = MakeAssetPreviewLabel(Args.Slot->Path);

    if (Args.bReadOnly)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::BeginCombo("##MatCombo", Preview.c_str()))
    {
        const bool bSelectedNone = (Args.Slot->Path == "None" || Args.Slot->Path.empty());
        if (ImGui::Selectable("None", bSelectedNone))
        {
            Args.Slot->Path = "None";
            bChanged = true;
        }
        if (bSelectedNone)
        {
            ImGui::SetItemDefaultFocus();
        }

        const TArray<FMaterialAssetListItem> &MatFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
        for (const FMaterialAssetListItem &Item : MatFiles)
        {
            const bool bSelected = (Args.Slot->Path == Item.FullPath);
            UTexture2D *PreviewTexture = Args.bShowPreviewImages
                ? FMaterialManager::Get().GetMaterialPreviewTexture(Item.FullPath)
                : nullptr;
            if (PreviewTexture && PreviewTexture->GetSRV())
            {
                ImGui::Image(PreviewTexture->GetSRV(), ImVec2(24.0f, 24.0f));
                ImGui::SameLine();
            }
            if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
            {
                Args.Slot->Path = Item.FullPath;
                bChanged = true;
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (Args.bReadOnly)
    {
        ImGui::EndDisabled();
    }

    FEditorUIStyle::PopDetailsPropertyEditStyle();

    if (CurrentPreviewTexture && CurrentPreviewTexture->GetSRV())
    {
        ImGui::SameLine(0.0f, PreviewSpacing);
        ImGui::Image(CurrentPreviewTexture->GetSRV(), ImVec2(PreviewImageSize, PreviewImageSize));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Preview.c_str());
        }
    }

    if (bShowResetButton)
    {
        ImGui::SameLine(0.0f, PreviewSpacing);
        if (!bCanReset)
        {
            ImGui::BeginDisabled();
        }
        const bool bResetClicked = ImGui::Button("##ResetMaterialSlot", ImVec2(ResetButtonSize, ResetButtonSize));
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 LabelSize = ImGui::CalcTextSize("R");
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(Min.x + (ResetButtonSize - LabelSize.x) * 0.5f, Min.y + (ResetButtonSize - LabelSize.y) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_Text), "R");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Reset to default material");
        }
        if (!bCanReset)
        {
            ImGui::EndDisabled();
        }

        if (bResetClicked && bCanReset)
        {
            Args.Slot->Path = ResetPath;
            bChanged = true;
        }
    }

    if (!Args.bReadOnly && Args.bAllowDragDrop && ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *Payload = ImGui::AcceptDragDropPayload("MaterialContentItem"))
        {
            FContentItem ContentItem = *reinterpret_cast<const FContentItem *>(Payload->Data);
            Args.Slot->Path = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());
            bChanged = true;
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndGroup();
    ImGui::PopID();
    return bChanged;
}
} // namespace FEditorDetailsWidgets
