#pragma once

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <string>

/**
 * 여러 에디터에서 재사용할 수 있는 DockBuilder layout 유틸.
 *
 * 주의:
 * - Asset Editor 집중 모드에서는 Level Editor 패널이 숨겨진 상태이므로,
 *   dockspace child node를 비우고 에셋 에디터 기본 레이아웃을 다시 구성한다.
 * - 이렇게 해야 FBX를 열었을 때 Preview Viewport가 floating window가 아니라
 *   중앙 dock node에 안정적으로 들어간다.
 */
struct FFourPanelDockLayoutDesc
{
    std::string ToolbarWindow;
    std::string LeftWindow;
    std::string CenterWindow;
    std::string RightWindow;

    float ToolbarRatio = 0.10f;
    float LeftRatio = 0.22f;
    float RightRatio = 0.28f;
};

/**
 * Unreal 계열 Asset Editor 기본 배치.
 *
 * ┌──────────┬────────────────────┬─────────────────┐
 * │ Toolbar  │ Preview Viewport   │ Skeleton Tree   │
 * │          │                    ├─────────────────┤
 * │          │                    │ Asset Details   │
 * └──────────┴────────────────────┴─────────────────┘
 */
struct FAssetPreviewDockLayoutDesc
{
    std::string ToolbarWindow;
    std::string CenterWindow;
    std::string RightTopWindow;
    std::string RightBottomWindow;
    std::string RightBottomSecondWindow;
    std::string RightBottomSideWindow;

    float LeftToolbarRatio = 0.12f;
    float RightColumnRatio = 0.36f;
    float RightBottomRatio = 0.48f;
    float RightBottomSideRatio = 0.42f;
};

/**
 * Level Editor 기본 배치.
 *
 * 좌측 Place Actors / 중앙 Viewport / 우측 상단 Outliner / 우측 하단 Details / 하단 Content Browser.
 */
struct FLevelEditorDockLayoutDesc
{
    std::string LeftWindow;
    std::string CenterWindow;
    std::string RightTopWindow;
    std::string RightBottomWindow;
    std::string RightBottomSideWindow;
    std::string BottomWindow;
    std::string BottomRightWindow;

    float BottomRatio = 0.24f;
    float LeftRatio = 0.20f;
    float RightRatio = 0.24f;
    float RightBottomRatio = 0.52f;
    float RightBottomSideRatio = 0.42f;
    float BottomRightRatio = 0.42f;
};

class FDockLayoutUtils
{
  public:
    static void ClearDockspaceForAssetEditor(ImGuiID DockspaceId)
    {
        if (DockspaceId == 0)
        {
            return;
        }

        ImGui::DockBuilderRemoveNodeChildNodes(DockspaceId);
    }

    static void DockFourPanelLayout(ImGuiID DockspaceId, const FFourPanelDockLayoutDesc &Desc)
    {
        if (DockspaceId == 0)
        {
            return;
        }

        ClearDockspaceForAssetEditor(DockspaceId);

        ImGuiID MainId = DockspaceId;
        ImGuiID ToolbarId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Up, Desc.ToolbarRatio, nullptr, &MainId);
        ImGuiID LeftId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Left, Desc.LeftRatio, nullptr, &MainId);
        ImGuiID RightId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Right, Desc.RightRatio, nullptr, &MainId);
        ImGuiID CenterId = MainId;

        if (!Desc.ToolbarWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.ToolbarWindow.c_str(), ToolbarId);
        }
        if (!Desc.LeftWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.LeftWindow.c_str(), LeftId);
        }
        if (!Desc.CenterWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.CenterWindow.c_str(), CenterId);
        }
        if (!Desc.RightWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.RightWindow.c_str(), RightId);
        }

        ImGui::DockBuilderFinish(DockspaceId);
    }

    static void DockAssetPreviewLayout(ImGuiID DockspaceId, const FAssetPreviewDockLayoutDesc &Desc)
    {
        if (DockspaceId == 0)
        {
            return;
        }

        ClearDockspaceForAssetEditor(DockspaceId);

        ImGuiID MainId = DockspaceId;
        ImGuiID ToolbarId = 0;
        if (!Desc.ToolbarWindow.empty())
        {
            ToolbarId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Left, Desc.LeftToolbarRatio, nullptr, &MainId);
        }
        ImGuiID RightColumnId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Right, Desc.RightColumnRatio, nullptr, &MainId);
        ImGuiID RightBottomId = ImGui::DockBuilderSplitNode(RightColumnId, ImGuiDir_Down, Desc.RightBottomRatio, nullptr, &RightColumnId);
        ImGuiID RightBottomSideId = 0;
        if (!Desc.RightBottomSideWindow.empty())
        {
            RightBottomSideId = ImGui::DockBuilderSplitNode(RightBottomId, ImGuiDir_Right, Desc.RightBottomSideRatio, nullptr, &RightBottomId);
        }
        ImGuiID CenterId = MainId;
        ImGuiID RightTopId = RightColumnId;

        if (ToolbarId != 0 && !Desc.ToolbarWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.ToolbarWindow.c_str(), ToolbarId);
        }
        if (!Desc.CenterWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.CenterWindow.c_str(), CenterId);
        }
        if (!Desc.RightTopWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.RightTopWindow.c_str(), RightTopId);
        }
        if (!Desc.RightBottomWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.RightBottomWindow.c_str(), RightBottomId);
        }
        if (!Desc.RightBottomSecondWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.RightBottomSecondWindow.c_str(), RightBottomId);
        }
        if (RightBottomSideId != 0 && !Desc.RightBottomSideWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.RightBottomSideWindow.c_str(), RightBottomSideId);
        }

        ImGui::DockBuilderFinish(DockspaceId);
    }

    static void DockLevelEditorLayout(ImGuiID DockspaceId, const FLevelEditorDockLayoutDesc &Desc)
    {
        if (DockspaceId == 0)
        {
            return;
        }

        ClearDockspaceForAssetEditor(DockspaceId);

        ImGuiID MainId = DockspaceId;
        ImGuiID BottomId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Down, Desc.BottomRatio, nullptr, &MainId);
        ImGuiID BottomRightId = 0;
        if (!Desc.BottomRightWindow.empty())
        {
            BottomRightId = ImGui::DockBuilderSplitNode(BottomId, ImGuiDir_Right, Desc.BottomRightRatio, nullptr, &BottomId);
        }
        ImGuiID LeftId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Left, Desc.LeftRatio, nullptr, &MainId);
        ImGuiID RightColumnId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Right, Desc.RightRatio, nullptr, &MainId);
        ImGuiID RightBottomId = ImGui::DockBuilderSplitNode(RightColumnId, ImGuiDir_Down, Desc.RightBottomRatio, nullptr, &RightColumnId);
        ImGuiID RightBottomSideId = 0;
        if (!Desc.RightBottomSideWindow.empty())
        {
            RightBottomSideId = ImGui::DockBuilderSplitNode(RightBottomId, ImGuiDir_Right, Desc.RightBottomSideRatio, nullptr, &RightBottomId);
        }
        ImGuiID CenterId = MainId;
        ImGuiID RightTopId = RightColumnId;

        if (!Desc.LeftWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.LeftWindow.c_str(), LeftId);
        }
        if (!Desc.CenterWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.CenterWindow.c_str(), CenterId);
        }
        if (!Desc.RightTopWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.RightTopWindow.c_str(), RightTopId);
        }
        if (!Desc.RightBottomWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.RightBottomWindow.c_str(), RightBottomId);
        }
        if (!Desc.BottomWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.BottomWindow.c_str(), BottomId);
        }
        if (RightBottomSideId != 0 && !Desc.RightBottomSideWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.RightBottomSideWindow.c_str(), RightBottomSideId);
        }
        if (BottomRightId != 0 && !Desc.BottomRightWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.BottomRightWindow.c_str(), BottomRightId);
        }

        ImGui::DockBuilderFinish(DockspaceId);
    }
};
