#include "PCH/LunaticPCH.h"
#include "Common/Menu/EditorMenuBar.h"

#include "Common/UI/Style/AccentColor.h"
#include "Common/UI/Style/EditorUIStyle.h"
#include "Common/Menu/EditorMenuProvider.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Resource/ResourceManager.h"

#include "ImGui/imgui.h"

namespace
{
    constexpr ImVec4 UnrealPanelSurface = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);

    float GetCustomTitleBarHeight() { return 42.0f; }
    float GetWindowOuterPadding() { return 6.0f; }
    float GetWindowTopContentInset(FWindowsWindow *Window)
    {
        (void)Window;
        return 0.0f;
    }

    const char *GetWindowControlIconMinimize() { return "\xEE\xA4\xA1"; }
    const char *GetWindowControlIconMaximize() { return "\xEE\xA4\xA2"; }
    const char *GetWindowControlIconRestore() { return "\xEE\xA4\xA3"; }
    const char *GetWindowControlIconClose() { return "\xEE\xA2\xBB"; }
} // namespace

void FEditorMenuBar::Render(const FEditorMenuBarContext &Context)
{
    const ImGuiViewport *MainViewport = ImGui::GetMainViewport();
    const float TitleBarHeight = GetCustomTitleBarHeight();
    const float TopFrameInset = GetWindowTopContentInset(Context.Window);
    const float OuterPadding = GetWindowOuterPadding();
    const float LogoSize = 36.0f;
    const float ButtonWidth = 38.0f;
    const float WindowControlHeight = 24.0f;
    const float ButtonSpacing = 2.0f;
    const float RightControlsWidth = ButtonWidth * 3.0f + ButtonSpacing * 2.0f;
    const float TitleBarPaddingY = 2.0f;
    const float LeftContentInset = 8.0f;

    ImGui::SetNextWindowPos(ImVec2(MainViewport->Pos.x + OuterPadding, MainViewport->Pos.y + TopFrameInset + OuterPadding), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(MainViewport->Size.x - OuterPadding * 2.0f, TitleBarHeight), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(MainViewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f + LeftContentInset, TitleBarPaddingY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
    const ImGuiWindowFlags TitleBarFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    if (!ImGui::Begin(Context.Id, nullptr, TitleBarFlags))
    {
        ImGui::End();
        ImGui::PopStyleVar(4);
        return;
    }

    ID3D11ShaderResourceView *LogoTexture =
        FResourceManager::Get().FindLoadedTexture(FResourceManager::Get().ResolvePath(FName("Editor.Icon.AppLogo"))).Get();

    float MenuEndX = 54.0f;
    if (ImGui::BeginMenuBar())
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24.0f, 7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(22.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
        if (Context.TitleBarFont)
        {
            ImGui::PushFont(Context.TitleBarFont);
        }

        const FString FrameTitle = Context.MenuProvider ? Context.MenuProvider->GetFrameTitle() : FString("Editor");
        const FString FrameTitleTooltip = Context.MenuProvider ? Context.MenuProvider->GetFrameTitleTooltip() : FrameTitle;
        const float FrameTabWidth = ImGui::CalcTextSize(FrameTitle.c_str()).x + 34.0f;
        const float MenuFrameHeight = ImGui::GetFrameHeight();
        const float FrameTabHeight = MenuFrameHeight;
        const float MaxContentHeight = (std::max)((std::max)(MenuFrameHeight, FrameTabHeight), (std::max)(WindowControlHeight, LogoSize));
        const float ContentStartY = (std::max)(0.0f, floorf((TitleBarHeight - MaxContentHeight) * 0.5f));
        const float RightControlsStartX = ImGui::GetWindowWidth() - RightControlsWidth;
        const float FrameTabX = RightControlsStartX - FrameTabWidth - 12.0f;
        float MenuStartX = ImGui::GetStyle().WindowPadding.x;

        if (LogoTexture)
        {
            const float LogoX = 8.0f;
            const float LogoY = ContentStartY;
            ImDrawList *DrawList = ImGui::GetForegroundDrawList(const_cast<ImGuiViewport *>(MainViewport));
            const ImVec2 WindowPos = ImGui::GetWindowPos();
            DrawList->AddImage(LogoTexture, ImVec2(WindowPos.x + LogoX, WindowPos.y + LogoY),
                               ImVec2(WindowPos.x + LogoX + LogoSize, WindowPos.y + LogoY + LogoSize));
            MenuStartX = LogoX + LogoSize + 10.0f;
        }

        ImGui::SetCursorPos(ImVec2(MenuStartX, ContentStartY));
        FEditorUIStyle::PushPopupWindowStyle();

        if (ImGui::BeginMenu("File"))
        {
            if (Context.MenuProvider)
            {
                Context.MenuProvider->BuildFileMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (Context.MenuProvider)
            {
                Context.MenuProvider->BuildEditMenu();
            }

            if (Context.bShowProjectSettingsMenu)
            {
                if (ImGui::GetCursorPosY() > 0.0f)
                {
                    FEditorUIStyle::DrawPopupSeparator(4.0f, 6.0f);
                }
                if (ImGui::MenuItem("Project Settings...") && Context.OnOpenProjectSettings)
                {
                    Context.OnOpenProjectSettings();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            if (Context.MenuProvider)
            {
                Context.MenuProvider->BuildWindowMenu();
            }
            ImGui::EndMenu();
        }

        if (Context.MenuProvider)
        {
            Context.MenuProvider->BuildCustomMenus();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (Context.OnToggleShortcutOverlay && ImGui::MenuItem("Shortcut Help"))
            {
                Context.OnToggleShortcutOverlay();
            }
            if (Context.OnOpenCredits && ImGui::MenuItem("Credits"))
            {
                Context.OnOpenCredits();
            }
            ImGui::EndMenu();
        }

        FEditorUIStyle::PopPopupWindowStyle();

        MenuEndX = ImGui::GetCursorPosX();

        ImGui::SetCursorPos(ImVec2(FrameTabX, ContentStartY));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.74f, 1.0f));
        ImGui::Button(FrameTitle.c_str(), ImVec2(FrameTabWidth, FrameTabHeight));
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(520.0f);
            ImGui::Text("Current: %s", FrameTitleTooltip.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);

        if (Context.Window)
        {
            ImGui::SetCursorPos(ImVec2(RightControlsStartX, ContentStartY));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.24f, 0.26f, 1.0f));
            if (Context.WindowControlIconFont)
            {
                ImGui::PushFont(Context.WindowControlIconFont);
            }
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.58f));
            if (ImGui::Button(GetWindowControlIconMinimize(), ImVec2(ButtonWidth, WindowControlHeight)))
            {
                if (Context.OnMinimizeWindow)
                {
                    Context.OnMinimizeWindow();
                }
                else
                {
                    Context.Window->Minimize();
                }
            }
            ImGui::PopStyleVar();
            ImGui::SameLine(0.0f, ButtonSpacing);
            if (ImGui::Button(Context.Window->IsWindowMaximized() ? GetWindowControlIconRestore() : GetWindowControlIconMaximize(),
                              ImVec2(ButtonWidth, WindowControlHeight)))
            {
                if (Context.OnToggleMaximizeWindow)
                {
                    Context.OnToggleMaximizeWindow();
                }
                else
                {
                    Context.Window->ToggleMaximize();
                }
            }
            ImGui::SameLine(0.0f, ButtonSpacing);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.16f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.58f, 0.10f, 0.10f, 1.0f));
            if (ImGui::Button(GetWindowControlIconClose(), ImVec2(ButtonWidth, WindowControlHeight)))
            {
                if (Context.OnCloseWindow)
                {
                    Context.OnCloseWindow();
                }
                else
                {
                    Context.Window->Close();
                }
            }
            if (Context.WindowControlIconFont)
            {
                ImGui::PopFont();
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
        }

        if (Context.TitleBarFont)
        {
            ImGui::PopFont();
        }
        ImGui::PopStyleVar(4);
        ImGui::EndMenuBar();

        const float DragRegionStartX = MenuEndX + 8.0f;
        const float DragRegionEndX = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - RightControlsWidth - FrameTabWidth - 20.0f;
        const float DragRegionWidth = DragRegionEndX - DragRegionStartX;
        const float TitleBarClientOriginX = OuterPadding;
        const float TitleBarClientOriginY = TopFrameInset + OuterPadding;
        const float TitleBarControlRegionX = TitleBarClientOriginX + (ImGui::GetWindowWidth() - RightControlsWidth);
        const float TitleBarControlRegionY = TitleBarClientOriginY + floorf((TitleBarHeight - WindowControlHeight) * 0.5f);

        if (Context.Window && DragRegionWidth > 24.0f)
        {
            Context.Window->SetTitleBarDragRegion(TitleBarClientOriginX + DragRegionStartX, TitleBarClientOriginY, DragRegionWidth,
                                                  TitleBarHeight);
            Context.Window->SetTitleBarControlRegion(TitleBarControlRegionX, TitleBarControlRegionY, RightControlsWidth,
                                                     WindowControlHeight);
        }
        else if (Context.Window)
        {
            Context.Window->ClearTitleBarDragRegion();
            Context.Window->ClearTitleBarControlRegion();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(4);
}
