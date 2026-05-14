#pragma once

#include "Common/UI/Style/AccentColor.h"
#include "Object/FName.h"
#include "Resource/ResourceManager.h"


#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace PanelTitleUtils
{
struct FPendingPanelDecoration
{
    ImGuiWindow *Window = nullptr;
    const char *IconKey = nullptr;
    bool *VisibleFlag = nullptr;
    ImRect TitleRect{};
    bool bHasTitleRect = false;
    bool bFocused = false;
};

struct FFocusedPanelOverlay
{
    ImDrawList *DrawList = nullptr;
    ImRect TitleRect{};
    float TabRounding = 0.0f;
};

inline std::unordered_map<ImGuiContext *, ImFont *> &GetPanelChromeIconFontStorage()
{
    static std::unordered_map<ImGuiContext *, ImFont *> Fonts;
    return Fonts;
}

inline std::vector<FPendingPanelDecoration> &GetPendingDecorations()
{
    static std::vector<FPendingPanelDecoration> Decorations;
    return Decorations;
}

inline std::vector<FFocusedPanelOverlay> &GetFocusedPanelOverlays()
{
    static std::vector<FFocusedPanelOverlay> Overlays;
    return Overlays;
}

inline void EnsurePanelChromeIconFontLoaded()
{
    ImGuiContext *Context = ImGui::GetCurrentContext();
    if (!Context)
    {
        return;
    }

    auto &Fonts = GetPanelChromeIconFontStorage();
    if (auto Found = Fonts.find(Context); Found != Fonts.end() && Found->second)
    {
        return;
    }

    const char *FontPath = "C:/Windows/Fonts/segmdl2.ttf";
    if (!std::filesystem::exists(FontPath))
    {
        return;
    }

    ImFontConfig FontConfig{};
    FontConfig.PixelSnapH = true;
    Fonts[Context] = ImGui::GetIO().Fonts->AddFontFromFileTTF(FontPath, 12.0f, &FontConfig);
}

inline ImFont *GetPanelChromeIconFont()
{
    ImGuiContext *Context = ImGui::GetCurrentContext();
    if (!Context)
    {
        return nullptr;
    }

    auto &Fonts = GetPanelChromeIconFontStorage();
    if (auto Found = Fonts.find(Context); Found != Fonts.end())
    {
        return Found->second;
    }

    return nullptr;
}

inline void ReleasePanelChromeIconFontForCurrentContext()
{
    ImGuiContext *Context = ImGui::GetCurrentContext();
    if (!Context)
    {
        return;
    }

    GetPanelChromeIconFontStorage().erase(Context);
}

inline ImU32 GetDockTabBarGapColor()
{
    return IM_COL32(5, 5, 5, 255);
}

inline ImU32 GetDockPanelSideGapColor()
{
    // Dock splitters/host background can leak through next to a rounded tab.
    // Keep the side gutters the same black as the editor frame instead of ImGui's grey dock background.
    return GetDockTabBarGapColor();
}

inline ImVec4 GetPanelSurfaceColor()
{
    return ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
}

inline ImVec4 GetPanelSurfaceHoverColor()
{
    return ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f);
}

inline ImVec4 GetPanelSurfaceActiveColor()
{
    return ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f);
}

inline ImVec4 GetPanelBorderColor()
{
    return ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
}

inline void PushPanelStyle()
{
    // Level Editor와 Asset Editor 패널이 같은 회색 surface를 사용하도록 여기서 통일한다.
    // Asset Editor 쪽에서 별도로 WindowBg를 덮어쓰면 탭/패널 body 색이 어긋나므로,
    // 모든 dockable editor panel은 이 공통 스타일을 먼저 탄다.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, GetPanelSurfaceColor());
    ImGui::PushStyleColor(ImGuiCol_ChildBg, GetPanelSurfaceColor());
    ImGui::PushStyleColor(ImGuiCol_Border, GetPanelBorderColor());
    ImGui::PushStyleColor(ImGuiCol_Header, GetPanelSurfaceColor());
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, GetPanelSurfaceHoverColor());
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, GetPanelSurfaceActiveColor());
}

inline void PopPanelStyle()
{
    ImGui::PopStyleColor(6);
}

inline float GetPanelTitleTopGapHeight()
{
    return 5.0f;
}

inline float GetPanelFrameGapThickness()
{
    return 5.0f;
}

inline float GetPanelContentTopInset()
{
    return 8.0f;
}

inline float GetPanelContentSideInset()
{
    return 12.0f;
}

inline float GetPanelContentBottomInset()
{
    return 10.0f;
}

inline float GetSelectedPanelTopBorderThickness()
{
    return 2.0f;
}

inline ImU32 GetSelectedPanelTopBorderColor()
{
    return UIAccentColor::ToU32();
}

inline float GetSelectedPanelTopBorderInset()
{
    return 1.5f;
}

inline const char *GetChromeCloseGlyph()
{
    return "\xEE\xA2\xBB";
}

inline void BeginPanelDecorationFrame()
{
    GetPendingDecorations().clear();
    GetFocusedPanelOverlays().clear();
}

inline FString GetIconResourcePath(const char *Key)
{
    return FResourceManager::Get().ResolvePath(FName(Key));
}

inline ID3D11ShaderResourceView *GetIcon(const char *Key)
{
    if (!Key || Key[0] == '\0')
    {
        return nullptr;
    }

    if (FTextureResource *Texture = FResourceManager::Get().FindTexture(FName(Key)))
    {
        return Texture->SRV;
    }

    return FResourceManager::Get().FindLoadedTexture(GetIconResourcePath(Key)).Get();
}

inline std::string MakeClosablePanelTitle(const char *Title, const char *IconKey = nullptr)
{
    const char *Prefix = (IconKey && IconKey[0] != '\0') ? "      " : "";
    return std::string(Prefix) + Title + "          ###" + Title;
}

inline ImRect GetPanelTitleRect()
{
    ImGuiWindow *Window = ImGui::GetCurrentWindow();
    if (Window && Window->DockIsActive && Window->DockNode && Window->DockNode->TabBar)
    {
        ImGuiTabBar *TabBar = Window->DockNode->TabBar;
        for (int TabIndex = 0; TabIndex < TabBar->Tabs.Size; ++TabIndex)
        {
            const ImGuiTabItem &Tab = TabBar->Tabs[TabIndex];
            if (Tab.Window != Window)
            {
                continue;
            }

            const float TabMinX = TabBar->BarRect.Min.x + Tab.Offset;
            const float TabMaxX = TabMinX + Tab.Width;
            return ImRect(ImVec2(TabMinX, TabBar->BarRect.Min.y), ImVec2(TabMaxX, TabBar->BarRect.Max.y));
        }
    }
    if (Window && Window->DockTabIsVisible)
    {
        return Window->DC.DockTabItemRect;
    }
    return Window ? Window->TitleBarRect() : ImRect();
}

inline ImRect GetPanelTitleRect(const ImGuiWindow *Window)
{
    if (Window && Window->DockIsActive && Window->DockNode && Window->DockNode->TabBar)
    {
        ImGuiTabBar *TabBar = Window->DockNode->TabBar;
        for (int TabIndex = 0; TabIndex < TabBar->Tabs.Size; ++TabIndex)
        {
            const ImGuiTabItem &Tab = TabBar->Tabs[TabIndex];
            if (Tab.Window != Window)
            {
                continue;
            }

            const float TabMinX = TabBar->BarRect.Min.x + Tab.Offset;
            const float TabMaxX = TabMinX + Tab.Width;
            return ImRect(ImVec2(TabMinX, TabBar->BarRect.Min.y), ImVec2(TabMaxX, TabBar->BarRect.Max.y));
        }
    }
    return Window ? Window->TitleBarRect() : ImRect();
}

inline ImDrawList *GetPanelTitleDrawList(ImGuiWindow *Window)
{
    if (Window && Window->DockIsActive && Window->DockNode && Window->DockNode->HostWindow)
    {
        return Window->DockNode->HostWindow->DrawList;
    }
    return Window ? Window->DrawList : ImGui::GetForegroundDrawList();
}

inline void PaintDockTabBarEmptyRegions(ImGuiWindow *Window)
{
    if (!Window || !Window->DockIsActive || !Window->DockNode || !Window->DockNode->TabBar)
    {
        return;
    }

    ImGuiTabBar *TabBar = Window->DockNode->TabBar;
    if (TabBar->Tabs.Size <= 0)
    {
        return;
    }

    float TabsMinX = FLT_MAX;
    float TabsMaxX = -FLT_MAX;
    for (int TabIndex = 0; TabIndex < TabBar->Tabs.Size; ++TabIndex)
    {
        const ImGuiTabItem &Tab = TabBar->Tabs[TabIndex];
        const float TabMinX = TabBar->BarRect.Min.x + Tab.Offset;
        const float TabMaxX = TabMinX + Tab.Width;
        TabsMinX = (std::min)(TabsMinX, TabMinX);
        TabsMaxX = (std::max)(TabsMaxX, TabMaxX);
    }

    if (TabsMinX > TabsMaxX)
    {
        return;
    }

    ImDrawList *DrawList = GetPanelTitleDrawList(Window);
    DrawList->PushClipRect(TabBar->BarRect.Min, TabBar->BarRect.Max, true);

    if (TabsMinX > TabBar->BarRect.Min.x)
    {
        DrawList->AddRectFilled(TabBar->BarRect.Min, ImVec2(TabsMinX, TabBar->BarRect.Max.y), GetDockTabBarGapColor());
    }

    if (TabsMaxX < TabBar->BarRect.Max.x)
    {
        DrawList->AddRectFilled(ImVec2(TabsMaxX, TabBar->BarRect.Min.y), TabBar->BarRect.Max, GetDockTabBarGapColor());
    }

    for (int TabIndex = 0; TabIndex + 1 < TabBar->Tabs.Size; ++TabIndex)
    {
        const ImGuiTabItem &CurrentTab = TabBar->Tabs[TabIndex];
        const ImGuiTabItem &NextTab = TabBar->Tabs[TabIndex + 1];
        const float CurrentTabMaxX = TabBar->BarRect.Min.x + CurrentTab.Offset + CurrentTab.Width;
        const float NextTabMinX = TabBar->BarRect.Min.x + NextTab.Offset;
        if (NextTabMinX <= CurrentTabMaxX)
        {
            continue;
        }

        DrawList->AddRectFilled(ImVec2(CurrentTabMaxX, TabBar->BarRect.Min.y),
                                ImVec2(NextTabMinX, TabBar->BarRect.Max.y), GetDockTabBarGapColor());
    }

    DrawList->PopClipRect();
}

inline void QueuePanelDecoration(const char *IconKey, bool *VisibleFlag)
{
    ImGuiWindow *Window = ImGui::GetCurrentWindow();
    if (!Window)
    {
        return;
    }

    const ImRect TitleRect = GetPanelTitleRect();
    const bool bHasTitleRect = TitleRect.GetWidth() > 0.0f && TitleRect.GetHeight() > 0.0f;
    const bool bFocused = Window->DockTabIsVisible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    std::vector<FPendingPanelDecoration> &Decorations = GetPendingDecorations();
    for (FPendingPanelDecoration &Decoration : Decorations)
    {
        if (Decoration.Window != Window)
        {
            continue;
        }

        if (IconKey)
        {
            Decoration.IconKey = IconKey;
        }
        if (VisibleFlag)
        {
            Decoration.VisibleFlag = VisibleFlag;
        }
        Decoration.TitleRect = TitleRect;
        Decoration.bHasTitleRect = bHasTitleRect;
        Decoration.bFocused = bFocused;
        return;
    }

    FPendingPanelDecoration Decoration;
    Decoration.Window = Window;
    Decoration.IconKey = IconKey;
    Decoration.VisibleFlag = VisibleFlag;
    Decoration.TitleRect = TitleRect;
    Decoration.bHasTitleRect = bHasTitleRect;
    Decoration.bFocused = bFocused;
    Decorations.push_back(Decoration);
}

inline void DrawPanelTitleIcon(const char *IconKey, float IconSize = 16.0f)
{
    (void)IconSize;
    QueuePanelDecoration(IconKey, nullptr);
}

inline bool DrawSmallPanelCloseButton(const char *DisplayTitle, bool &bVisible, const char *Id)
{
    // 도킹된 에디터 패널은 이제 ImGui::Begin(title, &bOpen, flags)가 제공하는
    // 기본 닫기 버튼에 의존한다.
    // 이 함수는 기존 호출부와의 호환을 위해 아무 동작도 하지 않는 형태로 유지한다.
    (void)DisplayTitle;
    (void)bVisible;
    (void)Id;
    return false;
}

inline void ApplyPanelContentTopInset(bool bApplySideInset = true, bool bApplyBottomInset = true)
{
    ImGuiWindow *Window = ImGui::GetCurrentWindow();
    if (!Window)
    {
        return;
    }

    const ImGuiID InsetStateId = ImHashStr("##PanelContentInsetApplied");
    int *const LastAppliedFrame = Window->StateStorage.GetIntRef(InsetStateId, -1);
    if (*LastAppliedFrame != ImGui::GetFrameCount())
    {
        *LastAppliedFrame = ImGui::GetFrameCount();

        const float SideInset =
            bApplySideInset ? (std::max)(GetPanelContentSideInset(), GetPanelFrameGapThickness()) : 0.0f;
        const float BottomInset =
            bApplyBottomInset ? (std::max)(GetPanelContentBottomInset(), GetPanelFrameGapThickness()) : 0.0f;
        Window->WorkRect.Min.x = (std::min)(Window->WorkRect.Max.x, Window->WorkRect.Min.x + SideInset);
        Window->WorkRect.Max.x = (std::max)(Window->WorkRect.Min.x, Window->WorkRect.Max.x - SideInset);
        Window->WorkRect.Max.y = (std::max)(Window->WorkRect.Min.y, Window->WorkRect.Max.y - BottomInset);

        Window->ContentRegionRect.Min.x =
            (std::min)(Window->ContentRegionRect.Max.x, Window->ContentRegionRect.Min.x + SideInset);
        Window->ContentRegionRect.Max.x =
            (std::max)(Window->ContentRegionRect.Min.x, Window->ContentRegionRect.Max.x - SideInset);
        Window->ContentRegionRect.Max.y =
            (std::max)(Window->ContentRegionRect.Min.y, Window->ContentRegionRect.Max.y - BottomInset);

        Window->DC.Indent.x += SideInset;
        Window->DC.CursorPos.x += SideInset;
        Window->DC.CursorStartPos.x += SideInset;
        Window->DC.CursorMaxPos.x = (std::max)(Window->DC.CursorMaxPos.x, Window->DC.CursorPos.x);
    }

    ImGui::Dummy(ImVec2(0.0f, GetPanelContentTopInset()));
}

inline void FlushPanelDecorations()
{
    std::vector<ImGuiTabBar *> PaintedTabBars;
    for (FPendingPanelDecoration &Decoration : GetPendingDecorations())
    {
        ImGuiWindow *Window = Decoration.Window;
        if (!Window)
        {
            continue;
        }

        if (Window->DockIsActive && Window->DockNode && Window->DockNode->TabBar)
        {
            ImGuiTabBar *TabBar = Window->DockNode->TabBar;
            if (std::find(PaintedTabBars.begin(), PaintedTabBars.end(), TabBar) == PaintedTabBars.end())
            {
                PaintDockTabBarEmptyRegions(Window);
                PaintedTabBars.push_back(TabBar);
            }
        }

        const ImRect TitleRect = Decoration.bHasTitleRect ? Decoration.TitleRect : GetPanelTitleRect(Window);
        if (TitleRect.GetWidth() <= 0.0f || TitleRect.GetHeight() <= 0.0f)
        {
            continue;
        }

        ImDrawList *DrawList = GetPanelTitleDrawList(Window);
        const float FrameGapThickness = GetPanelFrameGapThickness();
        const ImRect WindowRect = Window->Rect();
        const ImRect ExpandedWindowRect(ImVec2(WindowRect.Min.x - FrameGapThickness, WindowRect.Min.y),
                                        ImVec2(WindowRect.Max.x + FrameGapThickness, WindowRect.Max.y));
        const float TabRounding = (std::min)(ImGui::GetStyle().TabRounding, TitleRect.GetHeight() * 0.5f);
        const float TitleGapHeight = (std::min)(GetPanelTitleTopGapHeight(), TitleRect.GetHeight());
        const float BodyTopY = (std::max)(WindowRect.Min.y + FrameGapThickness, TitleRect.Max.y);
        DrawList->PushClipRect(ExpandedWindowRect.Min, ExpandedWindowRect.Max, true);
        DrawList->AddRectFilled(
            ExpandedWindowRect.Min,
            ImVec2(ExpandedWindowRect.Max.x,
                   (std::min)(ExpandedWindowRect.Min.y + FrameGapThickness, ExpandedWindowRect.Max.y)),
            GetDockTabBarGapColor(), TabRounding, ImDrawFlags_RoundCornersTop);
        if (BodyTopY < ExpandedWindowRect.Max.y)
        {
            DrawList->AddRectFilled(ImVec2(ExpandedWindowRect.Min.x, BodyTopY),
                                    ImVec2((std::min)(WindowRect.Min.x + FrameGapThickness, ExpandedWindowRect.Max.x),
                                           ExpandedWindowRect.Max.y),
                                    GetDockPanelSideGapColor());
            DrawList->AddRectFilled(
                ImVec2((std::max)(WindowRect.Max.x - FrameGapThickness, ExpandedWindowRect.Min.x), BodyTopY),
                ExpandedWindowRect.Max, GetDockPanelSideGapColor());
        }
        DrawList->PopClipRect();

        DrawList->PushClipRect(TitleRect.Min, TitleRect.Max, true);
        DrawList->AddRectFilled(TitleRect.Min, ImVec2(TitleRect.Max.x, TitleRect.Min.y + TitleGapHeight),
                                GetDockTabBarGapColor(), TabRounding, ImDrawFlags_RoundCornersTop);
        if (Decoration.bFocused)
        {
            FFocusedPanelOverlay Overlay;
            Overlay.DrawList = DrawList;
            Overlay.TitleRect = TitleRect;
            Overlay.TabRounding = TabRounding;
            GetFocusedPanelOverlays().push_back(Overlay);
        }

        if (ID3D11ShaderResourceView *Icon = GetIcon(Decoration.IconKey))
        {
            const float IconSize = 16.0f;
            const float IconX = TitleRect.Min.x + 8.0f;
            const float IconY = TitleRect.Min.y + (TitleRect.GetHeight() - IconSize) * 0.5f;
            DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon), ImVec2(IconX, IconY),
                               ImVec2(IconX + IconSize, IconY + IconSize));
        }

        // 닫기 버튼은 여기서 의도적으로 그리지 않는다.
        // ImGui dock 탭은 ImGui::Begin()에 p_open을 넘기면 자체적으로 닫기 버튼을 그린다.
        // 장식 단계에서 버튼을 하나 더 그리면 X 버튼이 중복되거나 겹쳐 보이게 된다.

        DrawList->PopClipRect();
    }

    // Focused/selected docked panel: draw the accent line inside the tab, not above the tab.
    // This avoids changing dock node height while still matching the document-tab selected indicator.
    for (const FFocusedPanelOverlay &Overlay : GetFocusedPanelOverlays())
    {
        if (!Overlay.DrawList || Overlay.TitleRect.GetWidth() <= 0.0f || Overlay.TitleRect.GetHeight() <= 0.0f)
        {
            continue;
        }

        const float BorderThickness =
            (std::min)(GetSelectedPanelTopBorderThickness(), Overlay.TitleRect.GetHeight() * 0.5f);
        const float BorderInset = GetSelectedPanelTopBorderInset();
        const float TopGapHeight = (std::min)(GetPanelTitleTopGapHeight(), Overlay.TitleRect.GetHeight());
        const ImRect InnerTopBorderRect(
            ImVec2(Overlay.TitleRect.Min.x + BorderInset, Overlay.TitleRect.Min.y + TopGapHeight + BorderInset),
            ImVec2(Overlay.TitleRect.Max.x - BorderInset,
                   Overlay.TitleRect.Min.y + TopGapHeight + BorderInset + BorderThickness));
        if (InnerTopBorderRect.GetWidth() <= 0.0f || InnerTopBorderRect.GetHeight() <= 0.0f)
        {
            continue;
        }

        Overlay.DrawList->PushClipRect(Overlay.TitleRect.Min, Overlay.TitleRect.Max, true);
        Overlay.DrawList->AddRectFilled(
            InnerTopBorderRect.Min, InnerTopBorderRect.Max, GetSelectedPanelTopBorderColor(),
            (std::max)(Overlay.TabRounding - BorderInset, 0.0f), ImDrawFlags_RoundCornersTop);
        Overlay.DrawList->PopClipRect();
    }
}
} // namespace PanelTitleUtils
