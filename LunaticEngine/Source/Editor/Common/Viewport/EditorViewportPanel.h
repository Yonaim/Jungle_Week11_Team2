#pragma once

#include "Common/Viewport/EditorViewportClient.h"

#include "ImGui/imgui.h"

/**
 * ImGui 패널 안에 FEditorViewportClient를 배치하기 위한 공통 adapter.
 *
 * 역할:
 * - 현재 ImGui content region을 FRect로 계산한다.
 * - ViewportClient에 screen rect / size / focus 상태를 넘긴다.
 * - viewport 전체를 InvisibleButton 입력 surface로 등록해 mouse/key focus를 안정적으로 잡는다.
 * - ViewportClient::RenderViewportImage() 호출 후 ImGui layout cursor를 viewport 크기만큼 전진시킨다.
 */
class FEditorViewportPanel
{
  public:
    static FRect CalculateContentRect()
    {
        const ImVec2 Pos = ImGui::GetCursorScreenPos();
        ImVec2 Size = ImGui::GetContentRegionAvail();
        if (Size.x < 1.0f)
        {
            Size.x = 1.0f;
        }
        if (Size.y < 1.0f)
        {
            Size.y = 1.0f;
        }

        FRect Rect;
        Rect.X = Pos.x;
        Rect.Y = Pos.y;
        Rect.Width = Size.x;
        Rect.Height = Size.y;
        return Rect;
    }

    static FRect RenderViewportClient(FEditorViewportClient &Client, bool bActiveFromOwner)
    {
        return RenderViewportClientInRect(Client, CalculateContentRect(), bActiveFromOwner);
    }

    static FRect RenderViewportClientInRect(FEditorViewportClient &Client, const FRect &InRect, bool bActiveFromOwner)
    {
        FRect Rect = InRect;
        if (Rect.Width < 1.0f)
        {
            Rect.Width = 1.0f;
        }
        if (Rect.Height < 1.0f)
        {
            Rect.Height = 1.0f;
        }

        const ImVec2 Min(Rect.X, Rect.Y);
        const ImVec2 Max(Rect.X + Rect.Width, Rect.Y + Rect.Height);

        ImGui::SetCursorScreenPos(Min);
        ImGui::PushID(&Client);
        ImGui::InvisibleButton("##EditorViewportInputSurface", ImVec2(Rect.Width, Rect.Height));

        const bool bItemHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        const bool bRectHovered = ImGui::IsMouseHoveringRect(Min, Max, true);
        const bool bMouseDownOnViewport =
            (bItemHovered || bRectHovered) &&
            (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
             ImGui::IsMouseDown(ImGuiMouseButton_Middle));
        const bool bViewportActive = bActiveFromOwner || ImGui::IsItemActive() || bMouseDownOnViewport;

        if (bItemHovered &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
        {
            ImGui::SetWindowFocus();
        }

        Client.SetHovered(bItemHovered || bRectHovered);
        Client.SetActive(bViewportActive);
        Client.SetViewportScreenRect(Rect);
        Client.SetViewportSize(Rect.Width, Rect.Height);
        Client.RenderViewportImage(Client.IsActive());
        Client.NotifyViewportPresented();

        ImGui::PopID();
        return Rect;
    }
};
