#pragma once

#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Common/UI/Style/AccentColor.h"
#include "Core/CoreTypes.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace FEditorDocumentTabBar
{
struct FTabDesc
{
    std::string Label;
    std::string Tooltip;
    const char *IconKey = nullptr;
    ImVec4 IconTint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    bool bDirty = false;
};

struct FRenderResult
{
    int32 SelectedIndex = -1;
    int32 CloseRequestedIndex = -1;
    bool bAnyHovered = false;
};

inline float GetHeight()
{
    return 34.0f;
}

struct FStyle
{
    float OuterPaddingX = 8.0f;
    float Gap = 1.0f;
    float MinTabWidth = 116.0f;
    float MaxTabWidth = 260.0f;
    float HorizontalPadding = 12.0f;
    float IconSize = 17.0f;
    float IconTextGap = 8.0f;
    float CloseButtonSize = 18.0f;
    float Rounding = 7.0f;
    ImVec4 BarBg = ImVec4(0.035f, 0.035f, 0.040f, 1.0f);
    ImVec4 BarBottomLine = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
    ImVec4 ActiveBg = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
    ImVec4 HoverBg = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
    ImVec4 InactiveBg = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    ImVec4 Text = ImVec4(0.96f, 0.96f, 0.98f, 1.0f);
    ImVec4 ActiveText = ImVec4(0.20f, 0.58f, 1.0f, 1.0f);
    ImVec4 CloseText = ImVec4(0.86f, 0.86f, 0.88f, 1.0f);
    ImVec4 CloseHoverBg = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);
};

inline const FStyle &GetDefaultStyle()
{
    static FStyle Style;
    return Style;
}

inline void DrawWideTooltip(const FTabDesc &Tab)
{
    if (Tab.Label.empty() && Tab.Tooltip.empty())
    {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(360.0f, 0.0f), ImVec2(720.0f, FLT_MAX));
    if (ImGui::BeginTooltip())
    {
        ImGui::TextUnformatted(Tab.Label.c_str());
        if (!Tab.Tooltip.empty())
        {
            ImGui::Separator();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 640.0f);
            ImGui::TextUnformatted(Tab.Tooltip.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndTooltip();
    }
}

inline FRenderResult Render(const char *Id, const std::vector<FTabDesc> &Tabs, int32 ActiveTabIndex, const FStyle &Style = GetDefaultStyle())
{
    FRenderResult Result;
    Result.SelectedIndex = ActiveTabIndex;

    if (Tabs.empty())
    {
        return Result;
    }

    const float BarHeight = ImGui::GetContentRegionAvail().y;
    const float BarWidth = ImGui::GetContentRegionAvail().x;
    if (BarWidth <= 32.0f || BarHeight <= 0.0f)
    {
        return Result;
    }

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const ImVec2 BarMin = ImGui::GetCursorScreenPos();
    const ImVec2 BarMax(BarMin.x + BarWidth, BarMin.y + BarHeight);

    const int32 TabCount = static_cast<int32>(Tabs.size());
    const float Gap = Style.Gap;
    const float CloseButtonSize = Style.CloseButtonSize;
    const float IconSize = Style.IconSize;
    const float HorizontalPadding = Style.HorizontalPadding;
    const float IconTextGap = Style.IconTextGap;
    const float TextRightPadding = 8.0f + CloseButtonSize + 8.0f;
    const float AvailableWidth = (std::max)(0.0f, BarWidth - Gap * (std::max)(0, TabCount - 1));
    const float AutoWidth = TabCount > 0 ? AvailableWidth / static_cast<float>(TabCount) : AvailableWidth;
    const float TabWidth = (std::max)(Style.MinTabWidth, (std::min)(Style.MaxTabWidth, AutoWidth));
    const float TabHeight = BarHeight - 5.0f;
    const float TabTop = BarMin.y + 3.0f;

    const ImU32 BarBgColor = ImGui::GetColorU32(Style.BarBg);
    const ImU32 BarBottomLineColor = ImGui::GetColorU32(Style.BarBottomLine);
    const ImU32 TabHoverColor = ImGui::GetColorU32(Style.HoverBg);
    const ImU32 TabActiveColor = ImGui::GetColorU32(Style.ActiveBg);
    const ImU32 TabInactiveColor = ImGui::GetColorU32(Style.InactiveBg);
    const ImU32 TextColor = ImGui::GetColorU32(Style.Text);
    const ImU32 ActiveTextColor = ImGui::GetColorU32(Style.ActiveText);
    const ImU32 CloseTextColor = ImGui::GetColorU32(Style.CloseText);
    const ImU32 CloseHoverColor = ImGui::GetColorU32(Style.CloseHoverBg);

    DrawList->PushClipRect(BarMin, BarMax, true);
    DrawList->AddRectFilled(BarMin, BarMax, BarBgColor, 0.0f);
    DrawList->AddLine(ImVec2(BarMin.x, BarMax.y - 1.0f), ImVec2(BarMax.x, BarMax.y - 1.0f), BarBottomLineColor, 1.0f);

    float X = BarMin.x + Style.OuterPaddingX;
    ImGui::PushID(Id ? Id : "EditorDocumentTabBar");
    for (int32 Index = 0; Index < TabCount; ++Index)
    {
        const FTabDesc &Tab = Tabs[Index];

        if (X >= BarMax.x)
        {
            break;
        }

        const float CurrentTabWidth = (std::min)(TabWidth, BarMax.x - X - Style.OuterPaddingX);
        if (CurrentTabWidth <= 54.0f)
        {
            break;
        }

        std::string DisplayTitle = Tab.Label.empty() ? "Untitled" : Tab.Label;
        if (Tab.bDirty)
        {
            DisplayTitle += "*";
        }

        const ImVec2 TabMin(X, TabTop);
        const ImVec2 TabMax(X + CurrentTabWidth, TabTop + TabHeight);

        ImGui::PushID(Index);

        const ImVec2 CloseMin(TabMax.x - CloseButtonSize - 8.0f, TabMin.y + (TabHeight - CloseButtonSize) * 0.5f);
        const ImVec2 CloseMax(CloseMin.x + CloseButtonSize, CloseMin.y + CloseButtonSize);
        const ImVec2 SelectMax((std::max)(TabMin.x + 1.0f, CloseMin.x - 4.0f), TabMax.y);

        ImGui::SetCursorScreenPos(TabMin);
        ImGui::InvisibleButton("##DocumentTabSelect", ImVec2((std::max)(1.0f, SelectMax.x - TabMin.x), TabHeight));
        const bool bSelectHovered = ImGui::IsItemHovered();
        Result.bAnyHovered |= bSelectHovered;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            Result.SelectedIndex = Index;
        }

        const bool bSelected = Index == ActiveTabIndex;
        const ImU32 FillColor = bSelected ? TabActiveColor : (bSelectHovered ? TabHoverColor : TabInactiveColor);
        if (bSelected || bSelectHovered)
        {
            DrawList->AddRectFilled(TabMin, TabMax, FillColor, Style.Rounding, ImDrawFlags_RoundCornersTop);
        }

        float TextX = TabMin.x + HorizontalPadding;
        if (ID3D11ShaderResourceView *Icon = PanelTitleUtils::GetIcon(Tab.IconKey))
        {
            const float IconY = TabMin.y + (TabHeight - IconSize) * 0.5f;
            const ImVec2 IconMin(TextX, IconY);
            DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon), IconMin,
                               ImVec2(IconMin.x + IconSize, IconMin.y + IconSize), ImVec2(0.0f, 0.0f),
                               ImVec2(1.0f, 1.0f), ImGui::GetColorU32(Tab.IconTint));
            TextX += IconSize + IconTextGap;
        }

        const ImVec2 TextSize = ImGui::CalcTextSize(DisplayTitle.c_str());
        const float TextY = TabMin.y + (TabHeight - TextSize.y) * 0.5f;
        const ImVec2 TextMin(TextX, TextY);
        const ImVec2 TextClipMax(TabMax.x - TextRightPadding, TabMax.y);
        const ImU32 CurrentTextColor = bSelected ? ActiveTextColor : TextColor;
        DrawList->PushClipRect(TextMin, TextClipMax, true);
        DrawList->AddText(TextMin, CurrentTextColor, DisplayTitle.c_str());
        if (bSelected)
        {
            // ImGui 기본 폰트만 있는 환경에서도 active tab을 굵게 보이게 하기 위해
            // 같은 텍스트를 1px 우측에 한 번 더 그린다.
            DrawList->AddText(ImVec2(TextMin.x + 1.0f, TextMin.y), CurrentTextColor, DisplayTitle.c_str());
        }
        DrawList->PopClipRect();

        ImGui::SetCursorScreenPos(CloseMin);
        ImGui::InvisibleButton("##CloseDocumentTab", ImVec2(CloseButtonSize, CloseButtonSize));
        const bool bCloseHovered = ImGui::IsItemHovered();
        if (bCloseHovered)
        {
            DrawList->AddRectFilled(CloseMin, ImVec2(CloseMin.x + CloseButtonSize, CloseMin.y + CloseButtonSize), CloseHoverColor, Style.Rounding * 0.5f);
        }
        DrawList->AddText(ImVec2(CloseMin.x + 4.0f, CloseMin.y - 1.0f), CloseTextColor, "x");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            Result.CloseRequestedIndex = Index;
        }

        if ((bSelectHovered || bCloseHovered) && !bCloseHovered)
        {
            DrawWideTooltip(Tab);
        }

        ImGui::PopID();
        X += CurrentTabWidth + Gap;
    }
    ImGui::PopID();

    DrawList->PopClipRect();
    return Result;
}
} // namespace FEditorDocumentTabBar
