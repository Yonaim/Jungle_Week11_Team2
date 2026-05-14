#pragma once

#include "Core/CoreTypes.h"
#include "ImGui/imgui.h"
#include "UI/SWindow.h"

/**
 * Viewport toolbar의 공통 배치/스타일 helper.
 *
 * 주의:
 * - 이 클래스는 viewport toolbar의 껍데기만 담당한다.
 * - Level Editor의 ViewMode/Camera/Show/Layout 버튼, SkeletalMesh Editor의 LOD 조정처럼
 *   실제 항목 구성은 각 viewport/editor가 직접 그린다.
 */
class FViewportToolbar
{
  public:
    static float GetDefaultHeight()
    {
        return ImGui::GetFrameHeight() + 8.0f;
    }

    static float GetHeight(float PaneWidth)
    {
        (void)PaneWidth;
        return 38.0f;
    }

    static bool Begin(const char *Id, float Height = 0.0f)
    {
        if (Height <= 0.0f)
        {
            Height = GetDefaultHeight();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075f, 0.075f, 0.085f, 1.0f));

        return ImGui::BeginChild(Id, ImVec2(0.0f, Height), false,
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    static void End()
    {
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    static bool BeginInRect(const char *Id, const FRect &PaneRect, float Height = 0.0f)
    {
        if (PaneRect.Width <= 0.0f || PaneRect.Height <= 0.0f)
        {
            return false;
        }

        if (Height <= 0.0f)
        {
            Height = GetHeight(PaneRect.Width);
        }

        ImGui::SetNextWindowPos(ImVec2(PaneRect.X, PaneRect.Y));
        ImGui::SetNextWindowSize(ImVec2(PaneRect.Width, Height));

        constexpr ImGuiWindowFlags ToolbarWindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                                  ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                                  ImGuiWindowFlags_NoMove;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.065f, 0.065f, 0.075f, 0.96f));

        const bool bOpen = ImGui::Begin(Id, nullptr, ToolbarWindowFlags);
        if (!bOpen)
        {
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            return false;
        }
        return true;
    }

    static void EndInRect()
    {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
};
