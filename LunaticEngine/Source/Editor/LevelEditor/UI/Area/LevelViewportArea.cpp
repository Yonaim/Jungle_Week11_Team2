#include "PCH/LunaticPCH.h"
#include "LevelEditor/UI/Area/LevelViewportArea.h"

#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Common/UI/Panels/Panel.h"
#include "Common/UI/Viewport/ViewportToolbar.h"
#include "EditorEngine.h"
#include "LevelEditor/Viewport/LevelEditorViewportClient.h"
#include "ImGui/imgui.h"
#include "Viewport/Viewport.h"


void FLevelViewportArea::SetIndex(int32 InIndex)
{
    Index = InIndex;
    if (Index == 0)
    {
        WindowName = "Viewport";
    }
    else
    {
        WindowName = "Viewport " + std::to_string(Index + 1);
    }
}

void FLevelViewportArea::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + 1.0f));
    constexpr const char *PanelIconKey = "Editor.Icon.Panel.Viewport";
    const std::string StableId = std::string("LevelViewportArea_") + std::to_string(Index);
    FPanelDesc PanelDesc;
    PanelDesc.DisplayName = WindowName.c_str();
    PanelDesc.StableId = StableId.c_str();
    PanelDesc.IconKey = PanelIconKey;
    PanelDesc.WindowFlags = ImGuiWindowFlags_None;
    PanelDesc.bApplySideInset = false;
    PanelDesc.bApplyBottomInset = false;
    const bool bIsOpen = FPanel::Begin(PanelDesc);
    if (!bIsOpen)
    {
        FPanel::End();
        ImGui::PopStyleVar(2);
        return;
    }

    if (FViewportToolbar::Begin("##ViewportToolbar"))
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.0f);
        float ButtonWidth = ImGui::CalcTextSize("Split").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - ButtonWidth);

        bool bIsSplit = EditorEngine ? EditorEngine->IsSplitViewport() : false;
        const char *ButtonLabel = bIsSplit ? "Merge" : "Split";

        if (ImGui::Button(ButtonLabel))
        {
            if (EditorEngine)
            {
                EditorEngine->ToggleViewportSplit();
            }
        }
    }
    FViewportToolbar::End();

    // 뷰포트 패널 위에서 마우스 클릭 시 활성 뷰포트 전환
    if (ViewportClient && EditorEngine)
    {
        ViewportClient->SetHovered(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows));

        if (ImGui::IsWindowHovered() && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
        {
            if (EditorEngine->GetActiveViewport() != ViewportClient)
            {
                EditorEngine->SetActiveViewport(ViewportClient);
            }
        }
    }

    ImVec2 Size = ImGui::GetContentRegionAvail();

    if (ViewportClient)
    {
        FViewport *VP = ViewportClient->GetViewport();
        if (VP && VP->GetSRV())
        {
            // 패널 리사이즈 감지 → 지연 요청 (다음 프레임 RenderViewport 직전에 적용)
            if (Size.x > 0 && Size.y > 0)
            {
                uint32 NewWidth = static_cast<uint32>(Size.x);
                uint32 NewHeight = static_cast<uint32>(Size.y);

                if (NewWidth != VP->GetWidth() || NewHeight != VP->GetHeight())
                {
                    VP->RequestResize(NewWidth, NewHeight);
                }
            }

            // 현재 RT의 SRV를 표시 (리사이즈는 다음 프레임에 적용됨)
            ImGui::Image((ImTextureID)VP->GetSRV(), Size);
        }
    }

    FPanel::End();
    ImGui::PopStyleVar(2);
}
