#pragma once

#include "Common/UI/Style/AccentColor.h"
#include "Core/CoreTypes.h"
#include "Object/FName.h"
#include "Resource/ResourceManager.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>
#include <string>

struct ID3D11ShaderResourceView;

// 에디터 전역에서 공유하는 ImGui 스타일/위젯 헬퍼 모음.
// 레벨 에디터 패널에만 하드코딩되어 있던 Details, Outliner, Viewport Toolbar 스타일을
// 에셋 에디터에서도 같은 형태로 재사용하기 위해 이 파일에 모은다.
namespace FEditorUIStyle
{
inline constexpr ImVec4 PopupMenuItemColor = ImVec4(0.18f, 0.18f, 0.20f, 0.96f);
inline constexpr ImVec4 PopupMenuItemHoverColor = UIAccentColor::Value;
inline constexpr ImVec4 PopupMenuItemActiveColor = UIAccentColor::Value;
inline constexpr ImVec4 PopupMenuItemHoverTextColor = ImVec4(0.02f, 0.02f, 0.025f, 1.0f);
inline constexpr ImVec4 PopupSectionHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
inline constexpr ImVec4 PopupSectionLineColor = PopupSectionHeaderTextColor;
inline constexpr float PopupSectionLineThickness = 1.0f;
inline constexpr float PopupHorizontalPadding = 10.0f;

inline constexpr ImVec4 HeaderButtonColor = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
inline constexpr ImVec4 HeaderButtonHoveredColor = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
inline constexpr ImVec4 HeaderButtonActiveColor = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
inline constexpr ImVec4 HeaderButtonBorderColor = ImVec4(0.42f, 0.42f, 0.45f, 0.90f);

inline constexpr ImVec4 SurfaceBg = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
inline constexpr ImVec4 PopupBg = ImVec4(0.12f, 0.13f, 0.15f, 0.98f);
inline constexpr ImVec4 FieldBg = ImVec4(26.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 1.0f);
inline constexpr ImVec4 FieldHoverBg = ImVec4(33.0f / 255.0f, 33.0f / 255.0f, 33.0f / 255.0f, 1.0f);
inline constexpr ImVec4 FieldActiveBg = ImVec4(43.0f / 255.0f, 43.0f / 255.0f, 43.0f / 255.0f, 1.0f);
inline constexpr ImVec4 FieldBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);

inline constexpr ImVec4 DetailsSectionTextColor = ImVec4(0.76f, 0.76f, 0.78f, 1.0f);
inline constexpr ImVec4 DetailsSectionHeaderColor = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
inline constexpr ImVec4 DetailsSectionHeaderHoveredColor = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
inline constexpr ImVec4 DetailsSectionHeaderActiveColor = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

inline constexpr ImVec4 DetailsVectorLabelColor = ImVec4(0.83f, 0.84f, 0.87f, 1.0f);
inline constexpr ImVec4 DetailsVectorFieldBg = ImVec4(10.0f / 255.0f, 10.0f / 255.0f, 10.0f / 255.0f, 1.0f);
inline constexpr ImVec4 DetailsVectorFieldHoverBg = ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f, 1.0f);
inline constexpr ImVec4 DetailsVectorFieldActiveBg = ImVec4(20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f);
inline constexpr ImVec4 DetailsVectorResetButtonColor = ImVec4(0.22f, 0.22f, 0.23f, 1.0f);
inline constexpr ImVec4 DetailsVectorResetButtonHoveredColor = ImVec4(0.30f, 0.30f, 0.32f, 1.0f);
inline constexpr ImVec4 DetailsVectorResetButtonActiveColor = ImVec4(0.36f, 0.36f, 0.38f, 1.0f);
inline constexpr ImVec4 DetailsVectorResetButtonBorderColor = ImVec4(0.52f, 0.52f, 0.55f, 0.95f);
inline constexpr float DetailsLabelWidth = 124.0f;
inline constexpr float DetailsPropertyVerticalSpacing = 6.0f;
inline constexpr float DetailsVectorResetSpacing = 6.0f;

inline constexpr ImVec4 OutlinerSelectionHeaderColor = UIAccentColor::Value;
inline constexpr ImVec4 OutlinerSelectionHeaderHoveredColor = UIAccentColor::Value;
inline constexpr ImVec4 OutlinerSelectionHeaderActiveColor = UIAccentColor::Value;
inline constexpr ImVec4 OutlinerFolderArrowColor = ImVec4(0.66f, 0.66f, 0.68f, 1.0f);
inline constexpr ImVec4 OutlinerItemLabelColor = ImVec4(0.86f, 0.86f, 0.88f, 1.0f);
inline constexpr ImU32 OutlinerFolderIconTint = IM_COL32(184, 140, 58, 255);

inline ID3D11ShaderResourceView *GetIcon(const char *Key)
{
    return FResourceManager::Get().FindLoadedTexture(FResourceManager::Get().ResolvePath(FName(Key))).Get();
}

inline void DrawTitleTextWithLine(const char *Label)
{
    if (!Label || Label[0] == '\0')
    {
        return;
    }

    ImDrawList *DrawList = ImGui::GetWindowDrawList();

    const ImVec2 Cursor = ImGui::GetCursorScreenPos();
    const ImVec2 TextSize = ImGui::CalcTextSize(Label);
    const ImU32  TextColor = ImGui::GetColorU32(PopupSectionHeaderTextColor);
    const ImU32  LineColor = ImGui::GetColorU32(PopupSectionLineColor);

    // Text()가 가진 baseline/line-height 차이 때문에 선과 글자가 살짝 어긋나 보일 수 있다.
    // 직접 AddText/AddLine으로 그려서 둘 다 같은 CenterY를 기준으로 정렬한다.
    const float HeaderHeight = (std::max)(TextSize.y, ImGui::GetTextLineHeight());
    const float CenterY = Cursor.y + HeaderHeight * 0.5f;
    const float TextY = CenterY - TextSize.y * 0.5f;
    const float TextX = Cursor.x + PopupHorizontalPadding;

    DrawList->AddText(ImVec2(TextX, TextY), TextColor, Label);

    const float LineStartX = TextX + TextSize.x + 8.0f;
    const float LineEndX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - PopupHorizontalPadding;
    const float LineY = CenterY + 0.5f;
    if (LineEndX > LineStartX)
    {
        DrawList->AddLine(ImVec2(LineStartX, LineY), ImVec2(LineEndX, LineY), LineColor, PopupSectionLineThickness);
    }

    ImGui::Dummy(ImVec2(0.0f, HeaderHeight));
}

inline void DrawPopupSectionHeader(const char *Label)
{
    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    DrawTitleTextWithLine(Label);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
}

inline void DrawPopupSeparator(float SpacingBefore = 4.0f, float SpacingAfter = 6.0f)
{
    if (SpacingBefore > 0.0f)
    {
        ImGui::Dummy(ImVec2(0.0f, SpacingBefore));
    }

    const ImVec2 Cursor = ImGui::GetCursorScreenPos();
    const float LineStartX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x;
    const float LineEndX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    const float LineY = Cursor.y + 0.5f;
    if (LineEndX > LineStartX)
    {
        ImGui::GetWindowDrawList()->AddLine(ImVec2(LineStartX, LineY), ImVec2(LineEndX, LineY),
                                            ImGui::GetColorU32(PopupSectionLineColor), PopupSectionLineThickness);
    }

    ImGui::Dummy(ImVec2(0.0f, PopupSectionLineThickness + SpacingAfter));
}


inline ImRect GetFullPopupRowRect(const ImVec2 &ItemMin, const ImVec2 &ItemMax)
{
    ImGuiWindow *Window = ImGui::GetCurrentWindow();
    if (!Window)
    {
        return ImRect(ItemMin, ItemMax);
    }

    // Selectable() only paints the content region, so WindowPadding.x remains dark on hover.
    // For editor popups we intentionally extend the hover/selected fill to the popup edge.
    return ImRect(ImVec2(Window->Pos.x, ItemMin.y), ImVec2(Window->Pos.x + Window->Size.x, ItemMax.y));
}

inline bool DrawPopupSelectableFullRow(const char *Label, bool bSelected = false, const ImVec2 &Size = ImVec2(0.0f, 24.0f))
{
    ImGui::PushID(Label ? Label : "##PopupSelectableFullRow");
    const float RowWidth = Size.x > 0.0f ? Size.x : ImGui::GetContentRegionAvail().x;
    const float RowHeight = Size.y > 0.0f ? Size.y : 24.0f;

    // Use an ID-only Selectable and draw the label ourselves. That lets the hover background
    // cover the popup's full width without being painted on top of ImGui's default text.
    const bool bClicked = ImGui::Selectable("##Option", bSelected, ImGuiSelectableFlags_None, ImVec2(RowWidth, RowHeight));
    const bool bHovered = ImGui::IsItemHovered();
    const ImVec2 ItemMin = ImGui::GetItemRectMin();
    const ImVec2 ItemMax = ImGui::GetItemRectMax();

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    if (bHovered || bSelected)
    {
        const ImRect RowRect = GetFullPopupRowRect(ItemMin, ItemMax);
        DrawList->AddRectFilled(RowRect.Min, RowRect.Max, ImGui::GetColorU32(UIAccentColor::Value));
    }

    const ImU32 TextColor = ImGui::GetColorU32((bHovered || bSelected) ? PopupMenuItemHoverTextColor : ImGui::GetStyleColorVec4(ImGuiCol_Text));
    const char *VisibleEnd = ImGui::FindRenderedTextEnd(Label);
    DrawList->AddText(ImVec2(ItemMin.x + ImGui::GetStyle().FramePadding.x,
                             ItemMin.y + (RowHeight - ImGui::GetTextLineHeight()) * 0.5f),
                      TextColor, Label ? Label : "", VisibleEnd);

    ImGui::PopID();
    return bClicked;
}

inline void PushPopupWindowStyle()
{
    ImGui::PushStyleColor(ImGuiCol_PopupBg, PopupBg);
    ImGui::PushStyleColor(ImGuiCol_Header, PopupMenuItemColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, PopupMenuItemHoverColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, PopupMenuItemActiveColor);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, PopupMenuItemHoverColor);
    ImGui::PushStyleColor(ImGuiCol_Separator, PopupSectionLineColor);
    ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, PopupSectionLineColor);
    ImGui::PushStyleColor(ImGuiCol_SeparatorActive, PopupSectionLineColor);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 14.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(5.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
}

inline void PopPopupWindowStyle()
{
    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(8);
}

inline void PushHeaderButtonStyle(float FrameRounding = 6.0f)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, FrameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, HeaderButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HeaderButtonHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HeaderButtonActiveColor);
    ImGui::PushStyleColor(ImGuiCol_Border, HeaderButtonBorderColor);
}

inline void PopHeaderButtonStyle()
{
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

inline void PushPopupMenuStyle()
{
    ImGui::PushStyleColor(ImGuiCol_PopupBg, PopupBg);
    ImGui::PushStyleColor(ImGuiCol_Header, PopupMenuItemColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, PopupMenuItemHoverColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, PopupMenuItemActiveColor);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, PopupMenuItemHoverColor);
    ImGui::PushStyleColor(ImGuiCol_Separator, PopupSectionLineColor);
    ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, PopupSectionLineColor);
    ImGui::PushStyleColor(ImGuiCol_SeparatorActive, PopupSectionLineColor);
}

inline void PopPopupMenuStyle()
{
    ImGui::PopStyleColor(8);
}

inline bool DrawSearchInputWithIcon(const char *Id, const char *Hint, char *Buffer, size_t BufferSize, float Width)
{
    ImGuiStyle &Style = ImGui::GetStyle();
    ImGui::SetNextItemWidth(Width);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 11.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Style.FramePadding.x + 26.0f, Style.FramePadding.y));
    ImGui::PushStyleColor(ImGuiCol_Border, HeaderButtonBorderColor);
    const std::string PaddedHint = std::string("   ") + Hint;
    const bool bChanged = ImGui::InputTextWithHint(Id, PaddedHint.c_str(), Buffer, BufferSize);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 Max = ImGui::GetItemRectMax();
    const float LeadingSlotWidth = (std::min)(30.0f, Max.x - Min.x);
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(Min.x + 1.0f, Min.y + 1.0f),
                                              ImVec2(Min.x + LeadingSlotWidth, Max.y - 1.0f), IM_COL32(5, 5, 5, 255),
                                              11.0f, ImDrawFlags_RoundCornersLeft);

    if (ID3D11ShaderResourceView *SearchIcon = GetIcon("Editor.Icon.Search"))
    {
        const float IconSize = ImGui::GetFrameHeight() - 12.0f;
        const float IconY = Min.y + (ImGui::GetFrameHeight() - IconSize) * 0.5f;
        ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(SearchIcon), ImVec2(Min.x + 7.0f, IconY),
                                             ImVec2(Min.x + 7.0f + IconSize, IconY + IconSize), ImVec2(1.0f, 0.0f),
                                             ImVec2(0.0f, 1.0f), IM_COL32(210, 210, 210, 255));
    }

    return bChanged;
}

inline bool BeginDetailsSection(const char *SectionName)
{
    const std::string HeaderId = std::string(SectionName) + "##DetailsSection";
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, DetailsSectionTextColor);
    ImGui::PushStyleColor(ImGuiCol_Header, DetailsSectionHeaderColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, DetailsSectionHeaderHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, DetailsSectionHeaderActiveColor);
    const bool bOpen = ImGui::CollapsingHeader(HeaderId.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    return bOpen;
}

inline void PushDetailsFieldStyle()
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, FieldBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, FieldHoverBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, FieldActiveBg);
    ImGui::PushStyleColor(ImGuiCol_Border, FieldBorder);
}

inline void PopDetailsFieldStyle()
{
    ImGui::PopStyleColor(4);
}

inline void PushDetailsVectorFieldStyle()
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, DetailsVectorFieldBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, DetailsVectorFieldHoverBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, DetailsVectorFieldActiveBg);
    ImGui::PushStyleColor(ImGuiCol_Border, FieldBorder);
}

inline void PopDetailsVectorFieldStyle()
{
    ImGui::PopStyleColor(4);
}

inline void PushDetailsVectorResetButtonStyle()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, DetailsVectorResetButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DetailsVectorResetButtonHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, DetailsVectorResetButtonActiveColor);
    ImGui::PushStyleColor(ImGuiCol_Border, DetailsVectorResetButtonBorderColor);
}

inline void PopDetailsVectorResetButtonStyle()
{
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
}

inline void PushDetailsPropertyEditStyle()
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.06f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.10f, 0.11f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.05f, 0.05f, 0.06f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
}

inline void PopDetailsPropertyEditStyle()
{
    ImGui::PopStyleColor(7);
}

inline bool BeginDetailsReadOnlyTable(const char *Id, float LabelColumnWidth = DetailsLabelWidth)
{
    if (!ImGui::BeginTable(Id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
    {
        return false;
    }
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, LabelColumnWidth);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

inline void DrawReadOnlyTextRow(const char *Label, const char *Value)
{
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(Label);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(Value ? Value : "");
}

inline void DrawReadOnlyIntRow(const char *Label, int32 Value)
{
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(Label);
    ImGui::TableNextColumn();
    ImGui::Text("%d", Value);
}

inline bool BeginOutlinerTable(const char *Id)
{
    return ImGui::BeginTable(Id, 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp);
}

inline void SetupOutlinerTableColumns()
{
    ImGui::TableSetupColumn("##Visibility", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 28.0f);
    ImGui::TableSetupColumn("##Lock", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 28.0f);
    ImGui::TableSetupColumn("Item Label", ImGuiTableColumnFlags_WidthStretch, 260.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableHeadersRow();
}

inline void PushOutlinerSelectionRowStyle()
{
    ImGui::PushStyleColor(ImGuiCol_Text, OutlinerItemLabelColor);
    ImGui::PushStyleColor(ImGuiCol_Header, OutlinerSelectionHeaderColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, OutlinerSelectionHeaderHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, OutlinerSelectionHeaderActiveColor);
}

inline void PopOutlinerSelectionRowStyle()
{
    ImGui::PopStyleColor(4);
}


// Skeleton Tree 전용 스타일.
// Outliner의 표 형태를 그대로 쓰지 않고, Bone hierarchy의 부모/자식 관계가 잘 보이도록
// 단일 Name 컬럼 + 들여쓰기 + 가지선 중심의 트리 스타일을 제공한다.
inline constexpr ImVec4 SkeletonTreeHeaderBg = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
inline constexpr ImVec4 SkeletonTreeRowBg = ImVec4(0.065f, 0.065f, 0.065f, 1.0f);
inline constexpr ImVec4 SkeletonTreeRowAltBg = ImVec4(0.085f, 0.085f, 0.085f, 1.0f);
inline constexpr ImVec4 SkeletonTreeLineColor = ImVec4(0.30f, 0.30f, 0.31f, 1.0f);
inline constexpr ImVec4 SkeletonTreeTextColor = ImVec4(0.76f, 0.76f, 0.78f, 1.0f);
inline constexpr ImVec4 SkeletonTreeMutedTextColor = ImVec4(0.50f, 0.50f, 0.52f, 1.0f);
inline constexpr ImVec4 SkeletonTreeSelectedTextColor = ImVec4(0.00f, 0.00f, 0.00f, 1.0f);
inline constexpr ImVec4 SkeletonTreeSelectionColor = UIAccentColor::Value;
inline constexpr ImVec4 SkeletonTreeSelectionHoveredColor = UIAccentColor::Value;
inline constexpr ImVec4 SkeletonTreeSelectionActiveColor = UIAccentColor::Value;
inline constexpr float SkeletonTreeIndentWidth = 18.0f;
inline constexpr float SkeletonTreeRowHeight = 18.0f;

inline bool BeginSkeletonTreeTable(const char* Id)
{
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, SkeletonTreeHeaderBg);
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, SkeletonTreeRowBg);
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, SkeletonTreeRowAltBg);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 1.0f));

    const bool bOpen = ImGui::BeginTable(
        Id,
        1,
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp);

    if (!bOpen)
    {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    return bOpen;
}

inline void SetupSkeletonTreeTableColumns()
{
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
}

inline void EndSkeletonTreeTable()
{
    ImGui::EndTable();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
}

inline void PushSkeletonTreeRowStyle(bool bSelected)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Header, bSelected ? SkeletonTreeSelectionColor : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, bSelected ? SkeletonTreeSelectionHoveredColor : ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, bSelected ? SkeletonTreeSelectionActiveColor : ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, bSelected ? SkeletonTreeSelectedTextColor : SkeletonTreeTextColor);
}

inline void PopSkeletonTreeRowStyle()
{
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

inline void DrawSkeletonTreeGuides(int32 Depth, bool bHasParent, float CellStartScreenX)
{
    // Skeleton Tree는 행 전체에 반복되는 세로 가이드선을 그리지 않는다.
    // 계층 표현은 TreeNode 화살표, Bone 아이콘, 들여쓰기만으로 처리한다.
    (void)Depth;
    (void)bHasParent;
    (void)CellStartScreenX;
}

inline const char* GetSkeletonBoneIcon(bool bSelected)
{
    return bSelected ? "◆" : "◇";
}
} // namespace FEditorUIStyle
