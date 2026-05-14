#pragma once

#include "Common/UI/Style/AccentColor.h"
#include "ImGui/imgui.h"

#include <d3d11.h>
#include <functional>
#include <vector>

struct FEditorPlayToolbarButtonDesc
{
    const char* Id = "";
    ID3D11ShaderResourceView* Icon = nullptr;
    const char* FallbackLabel = "";
    bool bDisabled = false;
    ImVec4 TintColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    const char* Tooltip = "";
    std::function<void()> OnClicked;
};

namespace FEditorPlayToolbar
{
    inline bool DrawIconButton(const FEditorPlayToolbarButtonDesc& Button, float ButtonSize, float IconSize)
    {
        if (Button.bDisabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
        bool bClicked = false;
        if (Button.Icon)
        {
            bClicked = ImGui::ImageButton(Button.Id, reinterpret_cast<ImTextureID>(Button.Icon), ImVec2(IconSize, IconSize),
                                          ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), Button.TintColor);
        }
        else
        {
            bClicked = ImGui::Button(Button.FallbackLabel, ImVec2(ButtonSize, ButtonSize));
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && Button.Tooltip && Button.Tooltip[0] != '\0')
        {
            ImGui::SetTooltip("%s", Button.Tooltip);
        }
        if (Button.bDisabled)
        {
            ImGui::PopStyleVar();
            bClicked = false;
        }
        if (bClicked && Button.OnClicked) Button.OnClicked();
        return bClicked;
    }

    inline void DrawButtonSeparator(float ButtonSize, float ButtonSpacing)
    {
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        const ImVec2 CursorScreenPos = ImGui::GetCursorScreenPos();
        const float SeparatorX = CursorScreenPos.x + ButtonSpacing * 0.5f;
        DrawList->AddLine(ImVec2(SeparatorX, CursorScreenPos.y + 3.0f),
                          ImVec2(SeparatorX, CursorScreenPos.y + ButtonSize - 3.0f),
                          ImGui::GetColorU32(ImVec4(0.36f, 0.36f, 0.40f, 0.9f)), 1.0f);
    }

    inline void RenderGroup(const char* Id, float X, float Y, float Width, float Height, float ButtonSize, float IconSize,
                            float ButtonSpacing, float GroupPaddingX, const std::vector<FEditorPlayToolbarButtonDesc>& Buttons)
    {
        if (Buttons.empty()) return;
        ImGui::SetCursorPos(ImVec2(X, Y));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.11f, 0.13f, 0.95f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::BeginChild(Id, ImVec2(Width, Height), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::SetCursorPos(ImVec2(GroupPaddingX, 5.0f));
        for (size_t Index = 0; Index < Buttons.size(); ++Index)
        {
            if (Index > 0)
            {
                DrawButtonSeparator(ButtonSize, ButtonSpacing);
                ImGui::SameLine(0.0f, ButtonSpacing);
            }
            DrawIconButton(Buttons[Index], ButtonSize, IconSize);
        }
        ImGui::EndChild();
    }

    inline void Render(const char* ToolbarId, float Width,
                       const std::vector<FEditorPlayToolbarButtonDesc>& PlayButtons,
                       const std::vector<FEditorPlayToolbarButtonDesc>& HistoryButtons,
                       float ToolbarHeight = 44.0f,
                       float IconSize = 20.0f,
                       float ButtonSpacing = 8.0f)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
        if (!ImGui::BeginChild(ToolbarId, ImVec2(Width, ToolbarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            return;
        }
        const float ButtonSize = ToolbarHeight - 14.0f;
        const float ButtonPadding = (ToolbarHeight - ButtonSize) * 0.5f;
        const float GroupPaddingX = 8.0f;
        const float GroupSpacing = 12.0f;
        const float GroupHeight = ButtonSize + 10.0f;
        const float GroupY = (ToolbarHeight - GroupHeight) * 0.5f;
        const float GroupInnerPadding = 6.0f;
        auto GroupWidth = [&](size_t Count) -> float
        {
            return Count == 0 ? 0.0f : GroupPaddingX * 2.0f + GroupInnerPadding * 2.0f + ButtonSize * static_cast<float>(Count) + ButtonSpacing * static_cast<float>(Count - 1);
        };
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 7));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UIAccentColor::Value);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIAccentColor::Value);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.35f, 0.39f, 0.9f));
        const float PlayGroupWidth = GroupWidth(PlayButtons.size());
        const float HistoryGroupWidth = GroupWidth(HistoryButtons.size());
        const float PlayGroupX = ButtonPadding;
        const float HistoryGroupX = PlayGroupX + PlayGroupWidth + GroupSpacing;
        RenderGroup("##PlayToolbarPlayGroup", PlayGroupX, GroupY, PlayGroupWidth, GroupHeight, ButtonSize, IconSize, ButtonSpacing, GroupPaddingX, PlayButtons);
        RenderGroup("##PlayToolbarHistoryGroup", HistoryGroupX, GroupY, HistoryGroupWidth, GroupHeight, ButtonSize, IconSize, ButtonSpacing, GroupPaddingX, HistoryButtons);
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
}
