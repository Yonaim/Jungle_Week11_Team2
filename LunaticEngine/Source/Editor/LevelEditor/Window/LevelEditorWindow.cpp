#include "PCH/LunaticPCH.h"
#include "LevelEditor/Window/LevelEditorWindow.h"

#include "Component/CameraComponent.h"
#include "EditorEngine.h"
#include "AssetEditor/Window/AssetEditorWindow.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "LevelEditor/Subsystem/OverlayStatSystem.h"
#include "LevelEditor/Viewport/LevelEditorViewportClient.h"
#include "Object/Object.h"
#include "LevelEditor/Settings/LevelEditorSettings.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_internal.h"

#include "Engine/Input/InputManager.h"
#include "Render/Pipeline/Renderer.h"

#include "Common/File/EditorFileUtils.h"
#include "Common/UI/ImGui/EditorImGuiSystem.h"
#include "Common/UI/Style/AccentColor.h"
#include "Common/UI/Style/EditorUIStyle.h"
#include "Common/UI/Docking/DockLayoutUtils.h"
#include "Common/UI/Panels/Panel.h"
#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Common/UI/Tabs/EditorDocumentTabBar.h"
#include "Common/UI/Notifications/NotificationToast.h"
#include "Core/Notification.h"
#include "Core/ProjectSettings.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Level.h"
#include "Common/UI/ImGui/EditorImGuiStyleSettings.h"
#include "Object/UClass.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"

#include <cfloat>
#include <filesystem>
#include <imm.h>
#include <windows.h>

namespace
{
    constexpr ImVec4           UnrealPanelSurface = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
    constexpr ImVec4           UnrealPanelSurfaceHover = ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f);
    constexpr ImVec4           UnrealPanelSurfaceActive = ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f);
    constexpr ImVec4           UnrealDockEmpty = ImVec4(5.0f / 255.0f, 5.0f / 255.0f, 5.0f / 255.0f, 1.0f);
    constexpr ImVec4           UnrealPopupSurface = ImVec4(42.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 0.98f);
    constexpr ImVec4           UnrealBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
    constexpr ImVec4           PopupSectionHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
    constexpr EOverlayStatType SupportedOverlayStats[] = {
        EOverlayStatType::FPS,
        EOverlayStatType::PickingTime,
        EOverlayStatType::Memory,
        EOverlayStatType::Shadow,
    };
    constexpr const char *CreditsDevelopers[] = {
        "Hojin Lee",
        "HyoBeom Kim",
        "Hyungjun Kim",
        "JunHyeop3631",
        "keonwookang0914",
        "kimhojun",
        "kwonhyeonsoo-goo",
        "LEE SangHoon",
        "lin-ion",
        "Park SangHyeok",
        "Seyoung Park",
        "ShimWoojin",
        "wwonnn",
        "Yonaim",
        "\xEA\xB0\x95\xEA\xB1\xB4\xEC\x9A\xB0",
        "\xEA\xB9\x80\xED\x83\x9C\xED\x98\x84",
        "\xEB\x82\xA8\xEC\x9C\xA4\xEC\xA7\x80",
        "\xEC\xA1\xB0\xED\x98\x84\xEC\x84\x9D",
    };

    void DrawPopupSectionHeader(const char *Label)
    {
        FEditorUIStyle::DrawPopupSectionHeader(Label);
    }

    void SetNextPopupWindowPosition(ImGuiCond Condition = ImGuiCond_Appearing)
    {
        if (const ImGuiViewport *MainViewport = ImGui::GetMainViewport())
        {
            const ImVec2 PopupAnchor(MainViewport->Pos.x + MainViewport->Size.x * 0.5f, MainViewport->Pos.y + MainViewport->Size.y * 0.42f);
            ImGui::SetNextWindowPos(PopupAnchor, Condition, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowViewport(MainViewport->ID);
        }
    }

    bool BeginUtilityPopupWindow(const char *Title, bool *bOpen, const ImVec2 &InitialSize, ImGuiCond SizeCondition,
                                 ImGuiWindowFlags Flags = 0)
    {
        SetNextPopupWindowPosition(ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(InitialSize, SizeCondition);
        ImGui::SetNextWindowSizeConstraints(ImVec2((std::max)(InitialSize.x, 360.0f), (std::max)(InitialSize.y, 180.0f)), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_Border, UnrealBorder);
        const bool bVisible = ImGui::Begin(Title, bOpen, Flags);
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);
        return bVisible;
    }

    bool ConfirmNewScene(HWND OwnerWindowHandle)
    {
        const int32 Result = MessageBoxW(OwnerWindowHandle, L"Create a new scene?\nUnsaved changes may be lost.", L"New Scene",
                                         MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        return Result == IDYES;
    }

    std::wstring MakeLevelClosePromptMessage(const std::filesystem::path& ScenePath, const std::string& Title)
    {
        std::wstring SceneName = L"This scene";
        if (!ScenePath.empty())
        {
            SceneName = ScenePath.filename().wstring();
        }
        else if (!Title.empty())
        {
            SceneName = FPaths::ToWide(Title);
        }

        return SceneName + L" has unsaved changes.\n\nClose without saving?";
    }

    void ApplyEditorTabStyle()
    {
        ImGuiStyle &Style = ImGui::GetStyle();
        Style.TabRounding = (std::max)(Style.TabRounding, 7.0f);
        Style.TabBorderSize = (std::max)(Style.TabBorderSize, 1.0f);
        Style.TabBarBorderSize = 0.0f;
        Style.TabBarOverlineSize = 0.0f;
        Style.DockingSeparatorSize = 2.0f;

        Style.Colors[ImGuiCol_Tab] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TabHovered] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TabSelected] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TabDimmed] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TabDimmedSelected] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TabSelectedOverline] = UnrealDockEmpty;
        Style.Colors[ImGuiCol_TabDimmedSelectedOverline] = UnrealDockEmpty;
    }

    void ApplyEditorColorTheme()
    {
        ImGuiStyle &Style = ImGui::GetStyle();
        Style.Colors[ImGuiCol_WindowBg] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_ChildBg] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_PopupBg] = UnrealPopupSurface;
        Style.Colors[ImGuiCol_TitleBg] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TitleBgActive] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TitleBgCollapsed] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_MenuBarBg] = UnrealDockEmpty;
        Style.Colors[ImGuiCol_FrameBg] = UnrealDockEmpty;
        Style.Colors[ImGuiCol_FrameBgHovered] = UnrealPanelSurfaceHover;
        Style.Colors[ImGuiCol_FrameBgActive] = UnrealPanelSurfaceActive;
        Style.Colors[ImGuiCol_CheckMark] = UIAccentColor::Value;
        Style.Colors[ImGuiCol_Button] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_ButtonHovered] = UnrealPanelSurfaceHover;
        Style.Colors[ImGuiCol_ButtonActive] = UnrealPanelSurfaceActive;
        Style.Colors[ImGuiCol_Header] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_HeaderHovered] = UnrealPanelSurfaceHover;
        Style.Colors[ImGuiCol_HeaderActive] = UnrealPanelSurfaceActive;
        Style.Colors[ImGuiCol_Separator] = UnrealDockEmpty;
        Style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(12.0f / 255.0f, 12.0f / 255.0f, 12.0f / 255.0f, 1.0f);
        Style.Colors[ImGuiCol_SeparatorActive] = ImVec4(18.0f / 255.0f, 18.0f / 255.0f, 18.0f / 255.0f, 1.0f);
        Style.Colors[ImGuiCol_Border] = UnrealBorder;
        Style.Colors[ImGuiCol_DockingEmptyBg] = UnrealDockEmpty;
        Style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
        Style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.36f, 0.36f, 0.39f, 1.0f);
        Style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.48f, 0.48f, 0.52f, 1.0f);
        Style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.58f, 0.58f, 0.62f, 1.0f);
    }

    FString GetSceneTitleLabel(UEditorEngine *EditorEngine)
    {
        if (!EditorEngine || !EditorEngine->HasCurrentLevelFilePath())
        {
            return "Untitled.umap";
        }

        const std::filesystem::path ScenePath(FPaths::ToWide(EditorEngine->GetCurrentLevelFilePath()));
        const std::wstring          FileName = ScenePath.filename().wstring();
        return FileName.empty() ? FString("Untitled.umap") : FPaths::ToUtf8(FileName);
    }

    float GetCustomTitleBarHeight() { return 42.0f; }

    float GetDocumentTabBarHeight() { return FEditorDocumentTabBar::GetHeight(); }

    float GetWindowOuterPadding() { return 6.0f; }

    float GetWindowCornerRadius() { return 12.0f; }

    float GetWindowTopContentInset(FWindowsWindow *Window)
    {
        (void)Window;
        return 0.0f;
    }

    const char *GetWindowControlIconMinimize() { return "\xEE\xA4\xA1"; }

    const char *GetWindowControlIconMaximize() { return "\xEE\xA4\xA2"; }

    const char *GetWindowControlIconRestore() { return "\xEE\xA4\xA3"; }

    const char *GetWindowControlIconClose() { return "\xEE\xA2\xBB"; }

    std::string MakeLevelPanelTitle(const char *DisplayName, const char *StableId, const char *IconKey = nullptr)
    {
        FPanelDesc Desc;
        Desc.DisplayName = DisplayName;
        Desc.StableId = StableId;
        Desc.IconKey = IconKey;
        return FPanel::MakeTitle(Desc);
    }

} // namespace

void FLevelEditorWindow::Create(FWindowsWindow *InWindow, FRenderer &InRenderer, UEditorEngine *InEditorEngine, FLevelEditor *InLevelEditor)
{
    Window = InWindow;
    Renderer = &InRenderer;
    EditorEngine = InEditorEngine;
    LevelEditor = InLevelEditor;
    ImGuiSystem = InEditorEngine ? &InEditorEngine->GetImGuiSystem() : nullptr;

    // Panel instances are created per document tab.
    // Create the initial Level document immediately, not during RenderDocumentTabBar().
    // The render pipeline collects viewport clients before UI is drawn, so delaying this
    // until the tab bar pass makes the first frame have no active viewport/render request.
    bPendingDefaultDockLayout = true;
    OpenLevelDocumentTabFromCurrentScene();
}

void FLevelEditorWindow::Release()
{
    for (FLevelDocumentTab &Tab : LevelDocumentTabs)
    {
        ShutdownLevelDocumentPanels(Tab);
    }
    LevelDocumentTabs.clear();
}

void FLevelEditorWindow::SaveToSettings() const
{
    if (const FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels())
    {
        if (Panels->ContentBrowser)
        {
            Panels->ContentBrowser->SaveToSettings();
        }
        if (Panels->ViewportLayout)
        {
            const_cast<FLevelViewportLayout *>(Panels->ViewportLayout.get())->SaveToSettings();
        }
    }
}

void FLevelEditorWindow::InitializeLevelDocumentPanels(FLevelDocumentTab &Tab)
{
    if (!EditorEngine || !Renderer || !Window || !LevelEditor)
    {
        return;
    }

    if (!Tab.Panels)
    {
        Tab.Panels = std::make_unique<FLevelDocumentPanels>();
    }

    FLevelDocumentPanels &Panels = *Tab.Panels;
    if (!Panels.ViewportLayout)
    {
        Panels.ViewportLayout = std::make_unique<FLevelViewportLayout>();
        Panels.ViewportLayout->Init(EditorEngine, Window, *Renderer, &LevelEditor->GetSelectionManager());
        Panels.ViewportLayout->LoadFromSettings();
    }
    if (!Panels.ConsolePanel)
    {
        Panels.ConsolePanel = std::make_unique<FLevelConsolePanel>();
        Panels.ConsolePanel->Init(EditorEngine);
    }
    if (!Panels.DetailsPanel)
    {
        Panels.DetailsPanel = std::make_unique<FLevelDetailsPanel>();
        Panels.DetailsPanel->Init(EditorEngine);
    }
    if (!Panels.OutlinerPanel)
    {
        Panels.OutlinerPanel = std::make_unique<FLevelOutlinerPanel>();
        Panels.OutlinerPanel->Init(EditorEngine);
    }
    if (!Panels.PlaceActorsPanel)
    {
        Panels.PlaceActorsPanel = std::make_unique<FLevelPlaceActorsPanel>();
        Panels.PlaceActorsPanel->Init(EditorEngine);
    }
    if (!Panels.StatPanel)
    {
        Panels.StatPanel = std::make_unique<FLevelStatPanel>();
        Panels.StatPanel->Init(EditorEngine);
    }
    if (!Panels.ContentBrowser)
    {
        Panels.ContentBrowser = std::make_unique<FContentBrowser>();
        Panels.ContentBrowser->Init(EditorEngine, Renderer->GetFD3DDevice().GetDevice());
    }
    if (!Panels.ShadowMapDebugPanel)
    {
        Panels.ShadowMapDebugPanel = std::make_unique<FShadowMapDebugPanel>();
        Panels.ShadowMapDebugPanel->Init(EditorEngine);
    }
}

void FLevelEditorWindow::ShutdownLevelDocumentPanels(FLevelDocumentTab &Tab)
{
    if (!Tab.Panels)
    {
        return;
    }

    if (Tab.Panels->ConsolePanel)
    {
        Tab.Panels->ConsolePanel->Shutdown();
    }
    if (Tab.Panels->ViewportLayout)
    {
        Tab.Panels->ViewportLayout->Release();
    }
    Tab.Panels.reset();
}

FLevelEditorWindow::FLevelDocumentPanels *FLevelEditorWindow::GetActiveLevelDocumentPanels()
{
    FLevelDocumentTab *Tab = GetActiveLevelDocumentTab();
    if (!Tab)
    {
        return nullptr;
    }
    if (!Tab->Panels)
    {
        InitializeLevelDocumentPanels(*Tab);
    }
    return Tab->Panels.get();
}

const FLevelEditorWindow::FLevelDocumentPanels *FLevelEditorWindow::GetActiveLevelDocumentPanels() const
{
    const FLevelDocumentTab *Tab = GetActiveLevelDocumentTab();
    return Tab ? Tab->Panels.get() : nullptr;
}

void FLevelEditorWindow::SetShowEditorOnlyComponents(bool bEnable)
{
    if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels())
    {
        if (Panels->DetailsPanel)
        {
            Panels->DetailsPanel->SetShowEditorOnlyComponents(bEnable);
        }
    }
}

bool FLevelEditorWindow::IsShowingEditorOnlyComponents() const
{
    if (const FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels())
    {
        return Panels->DetailsPanel && Panels->DetailsPanel->IsShowingEditorOnlyComponents();
    }
    return false;
}

void FLevelEditorWindow::RefreshContentBrowser()
{
    if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels())
    {
        if (Panels->ContentBrowser)
        {
            Panels->ContentBrowser->Refresh();
        }
    }
}

void FLevelEditorWindow::SelectContentBrowserPath(const std::filesystem::path& Path)
{
    if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels())
    {
        if (Panels->ContentBrowser)
        {
            Panels->ContentBrowser->RevealAndSelect(Path);
        }
    }
}

void FLevelEditorWindow::SetContentBrowserIconSize(float Size)
{
    if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels())
    {
        if (Panels->ContentBrowser)
        {
            Panels->ContentBrowser->SetIconSize(Size);
        }
    }
}

float FLevelEditorWindow::GetContentBrowserIconSize() const
{
    if (const FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels())
    {
        if (Panels->ContentBrowser)
        {
            return Panels->ContentBrowser->GetIconSize();
        }
    }
    return 96.0f;
}

FLevelViewportLayout *FLevelEditorWindow::GetActiveLevelViewportLayout()
{
    FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels();
    return Panels ? Panels->ViewportLayout.get() : nullptr;
}

const FLevelViewportLayout *FLevelEditorWindow::GetActiveLevelViewportLayout() const
{
    const FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels();
    return Panels ? Panels->ViewportLayout.get() : nullptr;
}

void FLevelEditorWindow::RequestDefaultDockLayout()
{
    if (IsActiveDocumentAssetEditor())
    {
        EditorEngine->GetAssetEditorManager().GetAssetEditorWindow().RequestDefaultLayoutForActiveTab();
        bPendingDefaultDockLayout = true;
        return;
    }

    if (FDockPanelLayoutState *LayoutState = GetActiveDocumentLayoutState())
    {
        LayoutState->bRequestDefaultLayout = true;
        LayoutState->bDefaultLayoutBuilt = false;
        FPanel::ClearCapturedLayoutRestore(LayoutState);
        LayoutState->PanelDockIds.clear();
    }
    bPendingDefaultDockLayout = true;
}

FLevelEditorWindow::FLevelDocumentTab *FLevelEditorWindow::GetActiveLevelDocumentTab()
{
    if (ActiveLevelDocumentTabIndex < 0 || ActiveLevelDocumentTabIndex >= static_cast<int32>(LevelDocumentTabs.size()))
    {
        return nullptr;
    }
    return &LevelDocumentTabs[ActiveLevelDocumentTabIndex];
}

const FLevelEditorWindow::FLevelDocumentTab *FLevelEditorWindow::GetActiveLevelDocumentTab() const
{
    if (ActiveLevelDocumentTabIndex < 0 || ActiveLevelDocumentTabIndex >= static_cast<int32>(LevelDocumentTabs.size()))
    {
        return nullptr;
    }
    return &LevelDocumentTabs[ActiveLevelDocumentTabIndex];
}

bool FLevelEditorWindow::IsActiveDocumentAssetEditor() const
{
    return EditorEngine && EditorEngine->IsAssetEditorContextActive() &&
           EditorEngine->GetAssetEditorManager().GetAssetEditorWindow().HasOpenTabs();
}

std::string FLevelEditorWindow::GetActiveDocumentLayoutId() const
{
    if (IsActiveDocumentAssetEditor())
    {
        return EditorEngine->GetAssetEditorManager().GetAssetEditorWindow().GetActiveLayoutId();
    }
    if (const FLevelDocumentTab *Tab = GetActiveLevelDocumentTab())
    {
        return Tab->LayoutId;
    }
    return "LevelEditorDockSpace_Empty";
}

FDockPanelLayoutState *FLevelEditorWindow::GetActiveDocumentLayoutState()
{
    if (IsActiveDocumentAssetEditor())
    {
        return EditorEngine->GetAssetEditorManager().GetAssetEditorWindow().GetActiveLayoutState();
    }
    if (FLevelDocumentTab *Tab = GetActiveLevelDocumentTab())
    {
        return &Tab->LayoutState;
    }
    return nullptr;
}

void FLevelEditorWindow::RequestRestoreForActiveDocument()
{
    if (IsActiveDocumentAssetEditor())
    {
        RequestDefaultDockLayout();
        return;
    }
    if (FLevelDocumentTab *Tab = GetActiveLevelDocumentTab())
    {
        FPanel::RequestCapturedLayoutRestore(&Tab->LayoutState);
    }
}


void FLevelEditorWindow::OpenLevelDocumentTabFromCurrentScene()
{
    bSuppressAutoLevelDocumentTab = false;
    FLevelDocumentTab NewTab;
    NewTab.LayoutId = std::string("LevelEditorDockSpace_") + std::to_string(NextLevelLayoutId++);
    NewTab.LayoutState.bRequestDefaultLayout = true;
    if (EditorEngine && EditorEngine->HasCurrentLevelFilePath())
    {
        NewTab.ScenePath = std::filesystem::path(FPaths::ToWide(EditorEngine->GetCurrentLevelFilePath())).lexically_normal();
        NewTab.Title = FPaths::ToUtf8(NewTab.ScenePath.filename().wstring());
    }
    else
    {
        NewTab.Title = std::string("Untitled ") + std::to_string(NextUntitledSceneIndex++);
    }

    InitializeLevelDocumentPanels(NewTab);
    LevelDocumentTabs.push_back(std::move(NewTab));
    ActiveLevelDocumentTabIndex = static_cast<int32>(LevelDocumentTabs.size()) - 1;
}

void FLevelEditorWindow::ReplaceActiveLevelDocumentTabFromCurrentScene()
{
    bSuppressAutoLevelDocumentTab = false;
    FLevelDocumentTab NewTab;
    NewTab.LayoutId = std::string("LevelEditorDockSpace_") + std::to_string(NextLevelLayoutId++);
    NewTab.LayoutState.bRequestDefaultLayout = true;
    if (EditorEngine && EditorEngine->HasCurrentLevelFilePath())
    {
        NewTab.ScenePath = std::filesystem::path(FPaths::ToWide(EditorEngine->GetCurrentLevelFilePath())).lexically_normal();
        NewTab.Title = FPaths::ToUtf8(NewTab.ScenePath.filename().wstring());
    }
    else
    {
        NewTab.Title = std::string("Untitled ") + std::to_string(NextUntitledSceneIndex++);
    }

    NewTab.bDirty = false;

    InitializeLevelDocumentPanels(NewTab);

    if (ActiveLevelDocumentTabIndex >= 0 && ActiveLevelDocumentTabIndex < static_cast<int32>(LevelDocumentTabs.size()))
    {
        ShutdownLevelDocumentPanels(LevelDocumentTabs[ActiveLevelDocumentTabIndex]);
        LevelDocumentTabs[ActiveLevelDocumentTabIndex] = std::move(NewTab);
        return;
    }

    LevelDocumentTabs.push_back(std::move(NewTab));
    ActiveLevelDocumentTabIndex = static_cast<int32>(LevelDocumentTabs.size()) - 1;
}

void FLevelEditorWindow::MarkActiveLevelDocumentDirty()
{
    SyncCurrentLevelDocumentTab();
    if (ActiveLevelDocumentTabIndex >= 0 && ActiveLevelDocumentTabIndex < static_cast<int32>(LevelDocumentTabs.size()))
    {
        LevelDocumentTabs[ActiveLevelDocumentTabIndex].bDirty = true;
    }
}

void FLevelEditorWindow::MarkActiveLevelDocumentClean()
{
    SyncCurrentLevelDocumentTab();
    if (ActiveLevelDocumentTabIndex >= 0 && ActiveLevelDocumentTabIndex < static_cast<int32>(LevelDocumentTabs.size()))
    {
        FLevelDocumentTab &ActiveTab = LevelDocumentTabs[ActiveLevelDocumentTabIndex];
        if (EditorEngine && EditorEngine->HasCurrentLevelFilePath())
        {
            ActiveTab.ScenePath = std::filesystem::path(FPaths::ToWide(EditorEngine->GetCurrentLevelFilePath())).lexically_normal();
            ActiveTab.Title = FPaths::ToUtf8(ActiveTab.ScenePath.filename().wstring());
        }
        ActiveTab.bDirty = false;
    }
}

bool FLevelEditorWindow::HasDirtyLevelDocument() const
{
    for (const FLevelDocumentTab &Tab : LevelDocumentTabs)
    {
        if (Tab.bDirty)
        {
            return true;
        }
    }
    return false;
}

bool FLevelEditorWindow::ConfirmCloseLevelDocumentTab(int32 TabIndex) const
{
    if (TabIndex < 0 || TabIndex >= static_cast<int32>(LevelDocumentTabs.size()))
    {
        return true;
    }

    const FLevelDocumentTab &Tab = LevelDocumentTabs[TabIndex];
    if (!Tab.bDirty)
    {
        return true;
    }

    const std::wstring Message = MakeLevelClosePromptMessage(Tab.ScenePath, Tab.Title);
    const int32 Result = MessageBoxW(Window ? Window->GetHWND() : nullptr,
                                    Message.c_str(),
                                    L"Unsaved Scene",
                                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    return Result == IDYES;
}

bool FLevelEditorWindow::ConfirmCloseActiveLevelDocument() const
{
    return ConfirmCloseLevelDocumentTab(ActiveLevelDocumentTabIndex);
}

bool FLevelEditorWindow::CanCloseEditorWindowWithPrompt() const
{
    if (!ConfirmCloseActiveLevelDocument())
    {
        return false;
    }

    if (EditorEngine && !EditorEngine->GetAssetEditorManager().ConfirmCloseAllEditors(Window ? Window->GetHWND() : nullptr))
    {
        return false;
    }

    return true;
}

void FLevelEditorWindow::SyncCurrentLevelDocumentTab()
{
    if (!EditorEngine)
    {
        return;
    }

    if (LevelDocumentTabs.empty())
    {
        if (!bSuppressAutoLevelDocumentTab)
        {
            OpenLevelDocumentTabFromCurrentScene();
        }
        return;
    }

    const bool bHasScenePath = EditorEngine->HasCurrentLevelFilePath();
    const std::filesystem::path CurrentPath = bHasScenePath
        ? std::filesystem::path(FPaths::ToWide(EditorEngine->GetCurrentLevelFilePath())).lexically_normal()
        : std::filesystem::path{};

    if (!CurrentPath.empty())
    {
        for (int32 Index = 0; Index < static_cast<int32>(LevelDocumentTabs.size()); ++Index)
        {
            if (LevelDocumentTabs[Index].ScenePath == CurrentPath)
            {
                ActiveLevelDocumentTabIndex = Index;
                LevelDocumentTabs[Index].Title = FPaths::ToUtf8(CurrentPath.filename().wstring());
                return;
            }
        }

        if (ActiveLevelDocumentTabIndex >= 0 && ActiveLevelDocumentTabIndex < static_cast<int32>(LevelDocumentTabs.size()) &&
            LevelDocumentTabs[ActiveLevelDocumentTabIndex].ScenePath.empty())
        {
            FLevelDocumentTab &ActiveTab = LevelDocumentTabs[ActiveLevelDocumentTabIndex];
            ActiveTab.ScenePath = CurrentPath;
            ActiveTab.Title = FPaths::ToUtf8(CurrentPath.filename().wstring());
            return;
        }

        FLevelDocumentTab NewTab;
        NewTab.LayoutId = std::string("LevelEditorDockSpace_") + std::to_string(NextLevelLayoutId++);
        NewTab.LayoutState.bRequestDefaultLayout = true;
        NewTab.ScenePath = CurrentPath;
        NewTab.Title = FPaths::ToUtf8(CurrentPath.filename().wstring());
        InitializeLevelDocumentPanels(NewTab);
        LevelDocumentTabs.push_back(std::move(NewTab));
        ActiveLevelDocumentTabIndex = static_cast<int32>(LevelDocumentTabs.size()) - 1;
        return;
    }

    if (ActiveLevelDocumentTabIndex < 0 || ActiveLevelDocumentTabIndex >= static_cast<int32>(LevelDocumentTabs.size()))
    {
        ActiveLevelDocumentTabIndex = 0;
    }

    FLevelDocumentTab &ActiveTab = LevelDocumentTabs[ActiveLevelDocumentTabIndex];
    if (ActiveTab.ScenePath.empty() && ActiveTab.Title.empty())
    {
        ActiveTab.Title = std::string("Untitled ") + std::to_string(NextUntitledSceneIndex++);
    }
}

bool FLevelEditorWindow::SetActiveLevelDocumentTab(int32 NewIndex)
{
    if (NewIndex < 0 || NewIndex >= static_cast<int32>(LevelDocumentTabs.size()))
    {
        return false;
    }

    if (ActiveLevelDocumentTabIndex == NewIndex)
    {
        return true;
    }

    const bool bLevelContextActive = EditorEngine && EditorEngine->IsLevelEditorContextActive();

    // The tab state/panels stay alive, but the outgoing Level document must lose
    // live input/gizmo ownership immediately. Otherwise a stale Level gizmo target
    // can receive input while an Asset tab is active or after returning later.
    if (bLevelContextActive)
    {
        if (FLevelViewportLayout* OldLayout = GetActiveLevelViewportLayout())
        {
            for (FLevelEditorViewportClient* Client : OldLayout->GetLevelViewportClients())
            {
                if (Client)
                {
                    Client->DeactivateEditorContext();
                }
            }
        }
    }

    ActiveLevelDocumentTabIndex = NewIndex;
    bSuppressAutoLevelDocumentTab = false;

    if (!EditorEngine)
    {
        return true;
    }

    FPanel::RequestCapturedLayoutRestore(&LevelDocumentTabs[NewIndex].LayoutState);

    // Tab switching must not rebuild/load the world by default. The document tab owns
    // its UI/panel/live context; scene load/new-scene is an explicit File action.
    if (bLevelContextActive)
    {
        if (FLevelViewportLayout* NewLayout = GetActiveLevelViewportLayout())
        {
            if (FLevelEditorViewportClient* ActiveClient = NewLayout->GetActiveViewport())
            {
                ActiveClient->ActivateEditorContext();
            }
        }
    }

    return true;
}

void FLevelEditorWindow::CloseLevelDocumentTab(int32 TabIndex)
{
    if (TabIndex < 0 || TabIndex >= static_cast<int32>(LevelDocumentTabs.size()))
    {
        return;
    }

    if (!ConfirmCloseLevelDocumentTab(TabIndex))
    {
        return;
    }

    ShutdownLevelDocumentPanels(LevelDocumentTabs[TabIndex]);
    LevelDocumentTabs.erase(LevelDocumentTabs.begin() + TabIndex);

    if (LevelDocumentTabs.empty())
    {
        ActiveLevelDocumentTabIndex = -1;
        bSuppressAutoLevelDocumentTab = true;
        return;
    }

    if (TabIndex == ActiveLevelDocumentTabIndex)
    {
        const int32 NewIndex = (std::min)(TabIndex, static_cast<int32>(LevelDocumentTabs.size()) - 1);
        ActiveLevelDocumentTabIndex = -1;
        SetActiveLevelDocumentTab(NewIndex);
    }
    else if (TabIndex < ActiveLevelDocumentTabIndex)
    {
        ActiveLevelDocumentTabIndex = (std::max)(0, ActiveLevelDocumentTabIndex - 1);
    }
}

void FLevelEditorWindow::RenderDocumentTabBar()
{
    if (!EditorEngine)
    {
        return;
    }

    SyncCurrentLevelDocumentTab();

    FAssetEditorWindow &AssetWindow = EditorEngine->GetAssetEditorManager().GetAssetEditorWindow();
    const int32 LevelTabCount = static_cast<int32>(LevelDocumentTabs.size());
    const int32 AssetTabCount = AssetWindow.GetDocumentTabCount();
    if (LevelTabCount <= 0 && AssetTabCount <= 0)
    {
        return;
    }

    const ImGuiViewport *MainViewport = ImGui::GetMainViewport();
    if (!MainViewport)
    {
        return;
    }

    const float OuterPadding = GetWindowOuterPadding();
    const float TitleBarHeight = GetCustomTitleBarHeight();
    const ImVec2 BarPos(MainViewport->Pos.x + OuterPadding,
                        MainViewport->Pos.y + OuterPadding + TitleBarHeight);
    const ImVec2 BarSize((std::max)(0.0f, MainViewport->Size.x - OuterPadding * 2.0f), GetDocumentTabBarHeight());
    if (BarSize.x <= 1.0f || BarSize.y <= 1.0f)
    {
        return;
    }

    ImGui::SetNextWindowPos(BarPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(BarSize, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(MainViewport->ID);

    const ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.035f, 0.040f, 1.0f));
    if (ImGui::Begin("##EditorDocumentTabBar", nullptr, Flags))
    {
        std::vector<FEditorDocumentTabBar::FTabDesc> TabDescs;
        TabDescs.reserve(static_cast<size_t>(LevelTabCount + AssetTabCount));

        for (const FLevelDocumentTab &Tab : LevelDocumentTabs)
        {
            FEditorDocumentTabBar::FTabDesc Desc;
            Desc.Label = Tab.Title.empty() ? "Untitled" : Tab.Title;
            Desc.Tooltip = Tab.ScenePath.empty() ? std::string("Unsaved Scene") : FPaths::ToUtf8(Tab.ScenePath.wstring());
            Desc.IconKey = "Editor.Icon.LevelDocument.Scene";
            Desc.IconTint = ImVec4(1.0f, 0.53f, 0.16f, 1.0f);
            Desc.bDirty = Tab.bDirty;
            TabDescs.push_back(std::move(Desc));
        }

        AssetWindow.AppendDocumentTabDescs(TabDescs);

        int32 ActiveUnifiedIndex = -1;
        if (EditorEngine->IsAssetEditorContextActive() && AssetWindow.GetActiveDocumentTabIndex() >= 0)
        {
            ActiveUnifiedIndex = LevelTabCount + AssetWindow.GetActiveDocumentTabIndex();
        }
        else
        {
            ActiveUnifiedIndex = ActiveLevelDocumentTabIndex;
        }

        FEditorDocumentTabBar::FRenderResult Result =
            FEditorDocumentTabBar::Render("EditorDocumentTabBar", TabDescs, ActiveUnifiedIndex);

        const char *ResetLayoutLabel = "Reset Layout";
        const float ResetButtonWidth = 104.0f;
        const float AvailableRight = ImGui::GetWindowContentRegionMax().x;
        const float CursorY = ImGui::GetCursorPosY();
        const float ResetButtonX = (std::max)(ImGui::GetCursorPosX(), AvailableRight - ResetButtonWidth);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ResetButtonX);
        ImGui::SetCursorPosY((std::max)(0.0f, (BarSize.y - 24.0f) * 0.5f));
        if (ImGui::Button(ResetLayoutLabel, ImVec2(ResetButtonWidth, 24.0f)))
        {
            RequestDefaultDockLayout();
        }
        ImGui::SetCursorPosY(CursorY);

        if (Result.SelectedIndex >= 0 && Result.SelectedIndex != ActiveUnifiedIndex)
        {
            if (Result.SelectedIndex < LevelTabCount)
            {
                // Select the target document first while Level context may still be inactive.
                // Then entering Level context activates only the selected tab's active viewport.
                SetActiveLevelDocumentTab(Result.SelectedIndex);
                if (EditorEngine->IsAssetEditorContextActive())
                {
                    EditorEngine->SetActiveEditorContext(EEditorContextType::LevelEditor);
                }
                RequestRestoreForActiveDocument();
            }
            else
            {
                const int32 AssetIndex = Result.SelectedIndex - LevelTabCount;
                const bool bSwitchedTab = AssetWindow.ActivateDocumentTab(AssetIndex);
                if (bSwitchedTab)
                {
                    // Pick the destination asset tab before entering the Asset context.
                    // Otherwise EnterEditorContext() briefly reactivates whichever asset tab
                    // used to be active, which can resurrect stale gizmo/input state and
                    // disturb the returning dock layout.
                    if (!EditorEngine->IsAssetEditorContextActive())
                    {
                        EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
                    }
                    RequestRestoreForActiveDocument();
                }
            }
        }

        if (Result.CloseRequestedIndex >= 0)
        {
            if (Result.CloseRequestedIndex < LevelTabCount)
            {
                CloseLevelDocumentTab(Result.CloseRequestedIndex);
            }
            else
            {
                const int32 AssetIndex = Result.CloseRequestedIndex - LevelTabCount;
                AssetWindow.CloseDocumentTab(AssetIndex, true);
                if (!AssetWindow.HasOpenTabs() && EditorEngine->IsAssetEditorContextActive())
                {
                    EditorEngine->SetActiveEditorContext(EEditorContextType::LevelEditor);
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

void FLevelEditorWindow::RenderLevelFrameToolbar()
{
    if (!EditorEngine || EditorEngine->IsAssetEditorContextActive())
    {
        return;
    }

    const ImGuiViewport *MainViewport = ImGui::GetMainViewport();
    if (!MainViewport)
    {
        return;
    }

    const float OuterPadding = GetWindowOuterPadding();
    const float TitleBarHeight = GetCustomTitleBarHeight();
    const float DocumentTabBarHeight = GetDocumentTabBarHeight();
    FLevelDocumentPanels *ActivePanels = GetActiveLevelDocumentPanels();
    FLevelViewportLayout *ActiveViewportLayout = ActivePanels ? ActivePanels->ViewportLayout.get() : nullptr;
    if (!ActiveViewportLayout)
    {
        return;
    }

    const float ToolbarHeight = ActiveViewportLayout->GetFrameToolbarHeight();

    const ImVec2 ToolbarPos(MainViewport->Pos.x + OuterPadding,
                            MainViewport->Pos.y + OuterPadding + TitleBarHeight + DocumentTabBarHeight);
    const ImVec2 ToolbarSize((std::max)(0.0f, MainViewport->Size.x - OuterPadding * 2.0f), ToolbarHeight);
    if (ToolbarSize.x <= 1.0f || ToolbarSize.y <= 1.0f)
    {
        return;
    }

    ImGui::SetNextWindowPos(ToolbarPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ToolbarSize, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(MainViewport->ID);

    const ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.050f, 0.050f, 0.055f, 1.0f));
    if (ImGui::Begin("##LevelEditorFrameToolbar", nullptr, Flags))
    {
        ActiveViewportLayout->RenderFrameToolbar(ToolbarSize.x);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

void FLevelEditorWindow::ApplyPendingDefaultDockLayout()
{
    if (IsActiveDocumentAssetEditor() || MainDockspaceId == 0)
    {
        return;
    }

    FLevelDocumentTab *ActiveTab = GetActiveLevelDocumentTab();
    if (!ActiveTab)
    {
        return;
    }

    FDockPanelLayoutState &LayoutState = ActiveTab->LayoutState;
    if (!bPendingDefaultDockLayout && !LayoutState.bRequestDefaultLayout && LayoutState.bDefaultLayoutBuilt)
    {
        return;
    }

    if (LayoutState.bDefaultLayoutBuilt && !LayoutState.bRequestDefaultLayout)
    {
        bPendingDefaultDockLayout = false;
        return;
    }

    ImGuiDockNode *DockNode = ImGui::DockBuilderGetNode(MainDockspaceId);
    if (!DockNode)
    {
        return;
    }

    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
    Settings.Panels.bViewport = true;
    Settings.Panels.bDetails = true;
    Settings.Panels.bOutliner = true;
    Settings.Panels.bPlaceActors = true;
    Settings.Panels.bContentBrowser = true;
    Settings.Panels.bConsole = true;

    FPanel::SetCurrentStableIdPrefix(ActiveTab->LayoutId);
    FLevelEditorDockLayoutDesc LayoutDesc;
    LayoutDesc.LeftWindow = MakeLevelPanelTitle("Place Actors", "LevelPlaceActorsPanel", "Editor.Icon.Panel.PlaceActors");
    LayoutDesc.CenterWindow = MakeLevelPanelTitle("Viewport", "LevelViewport", "Editor.Icon.Panel.Viewport");
    LayoutDesc.RightTopWindow = MakeLevelPanelTitle("Outliner", "LevelOutlinerPanel", "Editor.Icon.Panel.Outliner");
    LayoutDesc.RightBottomWindow = MakeLevelPanelTitle("Details", "LevelDetailsPanel", "Editor.Icon.Panel.Details");
    LayoutDesc.BottomWindow = MakeLevelPanelTitle("Content Browser", "LevelContentBrowser", "Editor.Icon.Panel.ContentBrowser");
    LayoutDesc.BottomRightWindow = MakeLevelPanelTitle("Console", "LevelConsolePanel", "Editor.Icon.Panel.Console");
    FPanel::ClearCurrentStableIdPrefix();

    FDockLayoutUtils::DockLevelEditorLayout(MainDockspaceId, LayoutDesc);
    LayoutState.bDefaultLayoutBuilt = true;
    LayoutState.bRequestDefaultLayout = false;
    FPanel::ClearCapturedLayoutRestore(&LayoutState);
    bPendingDefaultDockLayout = false;
}

void FLevelEditorWindow::FlushPendingMenuAction()
{
    const EPendingMenuAction Action = PendingMenuAction;
    PendingMenuAction = EPendingMenuAction::None;

    switch (Action)
    {
    case EPendingMenuAction::None:
        return;
    case EPendingMenuAction::NewScene:
        if (EditorEngine)
        {
            HWND OwnerWindowHandle = Window ? Window->GetHWND() : nullptr;
            if (ConfirmCloseActiveLevelDocument() && ConfirmNewScene(OwnerWindowHandle))
            {
                EditorEngine->NewScene();
                ReplaceActiveLevelDocumentTabFromCurrentScene();
            }
        }
        return;
    case EPendingMenuAction::OpenScene:
        if (EditorEngine)
        {
            if (ConfirmCloseActiveLevelDocument() && EditorEngine->LoadSceneWithDialog())
            {
                ReplaceActiveLevelDocumentTabFromCurrentScene();
            }
        }
        return;
    case EPendingMenuAction::SaveScene:
        if (EditorEngine)
        {
            if (EditorEngine->SaveScene())
            {
                MarkActiveLevelDocumentClean();
            }
        }
        return;
    case EPendingMenuAction::SaveSceneAs:
        if (EditorEngine)
        {
            EditorEngine->RequestSaveSceneAsDialog();
        }
        return;
    case EPendingMenuAction::NewUAsset:
        if (EditorEngine)
        {
            EditorEngine->GetAssetEditorManager().ShowAssetEditorWindow();
        }
        return;
    case EPendingMenuAction::OpenUAsset:
        if (EditorEngine)
        {
            EditorEngine->GetAssetEditorManager().OpenAssetWithDialog(Window ? Window->GetHWND() : nullptr);
        }
        return;
    case EPendingMenuAction::ImportAsset:
        if (EditorEngine)
        {
            EditorEngine->ImportAssetWithDialog();
        }
        return;
    case EPendingMenuAction::CookCurrentScene:
        CookCurrentScene();
        return;
    case EPendingMenuAction::CookAllScenes:
    {
        const int32 Count = FSceneSaveManager::CookAllScenes();
        FNotificationManager::Get().AddNotification(std::string("Cooked ") + std::to_string(Count) + " scenes",
                                                    Count > 0 ? ENotificationType::Success : ENotificationType::Error);
        return;
    }
    case EPendingMenuAction::PackageRelease:
        PackageGameBuild("ReleaseBuild.bat");
        return;
    case EPendingMenuAction::PackageShipping:
        PackageGameBuild("ShippingBuild.bat");
        return;
    case EPendingMenuAction::PackageDemo:
        PackageGameBuild("DemoBuild.bat");
        return;
    }
}

void FLevelEditorWindow::RenderContent(float DeltaTime)
{
    PanelTitleUtils::BeginPanelDecorationFrame();

    FEditorMenuBarContext MenuContext{};
    MenuContext.Id = "##LevelEditorMenuBar";
    MenuContext.Window = Window;
    MenuContext.EditorEngine = EditorEngine;
    // 메뉴 구성은 Level Editor 기준으로 통일한다.
    // 활성 document가 Asset Editor여도 File/Edit/Window 메뉴 항목의 구조는 바뀌지 않는다.
    MenuContext.MenuProvider = static_cast<IEditorMenuProvider *>(this);
    MenuContext.TitleBarFont = ImGuiSystem ? ImGuiSystem->GetTitleBarFont() : nullptr;
    MenuContext.WindowControlIconFont = ImGuiSystem ? ImGuiSystem->GetWindowControlIconFont() : nullptr;
    MenuContext.bShowProjectSettingsMenu = true;
    MenuContext.OnMinimizeWindow = [this]()
    {
        if (Window)
        {
            Window->Minimize();
        }
    };
    MenuContext.OnToggleMaximizeWindow = [this]()
    {
        if (Window)
        {
            Window->ToggleMaximize();
        }
    };
    MenuContext.OnCloseWindow = [this]()
    {
        if (Window)
        {
            Window->Close();
        }
    };
    MenuContext.OnOpenProjectSettings = [this]() { bShowProjectSettings = true; };
    MenuContext.OnToggleShortcutOverlay = [this]() { bShowShortcutOverlay = !bShowShortcutOverlay; };
    MenuContext.OnOpenCredits = [this]() { bShowCreditsOverlay = !bShowCreditsOverlay; };
    const bool bAssetEditorContextActiveEarly = EditorEngine && EditorEngine->IsAssetEditorContextActive();
    MenuBar.Render(MenuContext);
    RenderDocumentTabBar();
    RenderLevelFrameToolbar();

    const ImGuiViewport *MainViewport = ImGui::GetMainViewport();
    const float          TitleBarHeight = GetCustomTitleBarHeight();
    const float          TopFrameInset = GetWindowTopContentInset(Window);
    const float          OuterPadding = GetWindowOuterPadding();
    const float          DocumentTabBarHeight = GetDocumentTabBarHeight();
    const bool           bAssetEditorContextActive = bAssetEditorContextActiveEarly;
    const bool           bLevelFrameToolbarVisible = !bAssetEditorContextActive;
    FLevelDocumentPanels *ActiveLevelPanelsForFrame = (!bAssetEditorContextActive && EditorEngine) ? GetActiveLevelDocumentPanels() : nullptr;
    FLevelViewportLayout *ActiveLevelViewportLayoutForFrame = ActiveLevelPanelsForFrame ? ActiveLevelPanelsForFrame->ViewportLayout.get() : nullptr;
    const float          LevelFrameToolbarHeight = (bLevelFrameToolbarVisible && ActiveLevelViewportLayoutForFrame)
                                                     ? ActiveLevelViewportLayoutForFrame->GetFrameToolbarHeight()
                                                     : 0.0f;
    const bool           bAssetFrameToolbarVisible = bAssetEditorContextActive &&
                                                     EditorEngine->GetAssetEditorManager().GetAssetEditorWindow().IsOpen();
    const float          AssetFrameToolbarHeight = bAssetFrameToolbarVisible
                                                     ? FAssetEditorWindow::GetFrameToolbarHeight()
                                                     : 0.0f;
    const float          TopEditorStripHeight = DocumentTabBarHeight + LevelFrameToolbarHeight + AssetFrameToolbarHeight;
    const float          CornerRadius = GetWindowCornerRadius();
    const ImVec2         ViewportMin = MainViewport->Pos;
    const ImVec2         ViewportMax(MainViewport->Pos.x + MainViewport->Size.x, MainViewport->Pos.y + MainViewport->Size.y);
    const ImVec2         FrameMin(MainViewport->Pos.x + OuterPadding, MainViewport->Pos.y + TopFrameInset + OuterPadding);
    const ImVec2         FrameMax(MainViewport->Pos.x + MainViewport->Size.x - OuterPadding,
                                  MainViewport->Pos.y + MainViewport->Size.y - OuterPadding);
    ImDrawList          *BackgroundDrawList = ImGui::GetBackgroundDrawList(const_cast<ImGuiViewport *>(MainViewport));
    BackgroundDrawList->AddRectFilled(ViewportMin, ViewportMax, IM_COL32(5, 5, 5, 255));
    BackgroundDrawList->AddRectFilled(FrameMin, FrameMax, IM_COL32(5, 5, 5, 255), CornerRadius);

    ImGuiWindowClass DockspaceWindowClass{};
    // Dock node 오른쪽 끝에 뜨는 전역 X 버튼은 패널별 tab close와 별개라서
    // Asset Editor 패널을 다시 켤 때 불필요한 닫기 버튼처럼 보인다.
    // 패널 닫기는 각 panel window의 p_open / Window 메뉴에서만 처리한다.
    DockspaceWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;
    ImGui::SetNextWindowPos(ImVec2(MainViewport->Pos.x + OuterPadding,
                                    MainViewport->Pos.y + TopFrameInset + TitleBarHeight + OuterPadding + TopEditorStripHeight),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(MainViewport->Size.x - OuterPadding * 2.0f,
               (std::max)(1.0f, MainViewport->Size.y - TopFrameInset - TitleBarHeight - TopEditorStripHeight - OuterPadding * 2.0f)),
        ImGuiCond_Always);
    ImGui::SetNextWindowViewport(MainViewport->ID);
    ImGuiWindowFlags DockspaceWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + 6.0f));
    if (ImGui::Begin("##EditorDockSpaceHost", nullptr, DockspaceWindowFlags))
    {
        const std::string ActiveLayoutId = GetActiveDocumentLayoutId();
        MainDockspaceId = ImGui::GetID(ActiveLayoutId.c_str());
        ImGui::DockSpace(MainDockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &DockspaceWindowClass);

        // Keep inactive document DockSpaces alive so ImGui does not discard their dock node tree while the tab is hidden.
        for (const FLevelDocumentTab &Tab : LevelDocumentTabs)
        {
            if (!Tab.LayoutId.empty() && Tab.LayoutId != ActiveLayoutId)
            {
                ImGui::DockSpace(ImGui::GetID(Tab.LayoutId.c_str()), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_KeepAliveOnly, &DockspaceWindowClass);
            }
        }
        if (EditorEngine)
        {
            std::vector<std::string> AssetLayoutIds;
            EditorEngine->GetAssetEditorManager().GetAssetEditorWindow().CollectLayoutIds(AssetLayoutIds);
            for (const std::string &LayoutId : AssetLayoutIds)
            {
                if (!LayoutId.empty() && LayoutId != ActiveLayoutId)
                {
                    ImGui::DockSpace(ImGui::GetID(LayoutId.c_str()), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_KeepAliveOnly, &DockspaceWindowClass);
                }
            }
        }

        // Keep the saved panel-window ↔ dock-node relationship alive for inactive tabs.
        // Keeping only DockSpace nodes alive is not enough: when the corresponding panel
        // windows are not submitted for several frames, ImGui can bring them back as
        // floating/undocked windows. These invisible no-content windows use the same ###ID
        // and the last captured DockId, but only for inactive tabs.
        for (FLevelDocumentTab &Tab : LevelDocumentTabs)
        {
            if (!Tab.LayoutId.empty() && Tab.LayoutId != ActiveLayoutId)
            {
                FPanel::RenderDockKeepAliveWindows(&Tab.LayoutState);
            }
        }
        if (EditorEngine)
        {
            EditorEngine->GetAssetEditorManager().GetAssetEditorWindow().RenderInactiveDockKeepAliveWindows(ActiveLayoutId);
        }

        ApplyPendingDefaultDockLayout();
    }
    ImGui::End();
    ImGui::PopStyleVar(4);

    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
    FDockPanelLayoutState *ActiveLayoutState = GetActiveDocumentLayoutState();
    if (!bAssetEditorContextActive && ActiveLayoutState)
    {
        FPanel::SetCurrentStableIdPrefix(GetActiveDocumentLayoutId());
        FPanel::SetCurrentDockspaceId(MainDockspaceId);
        FPanel::SetCurrentLayoutState(ActiveLayoutState);
    }

    // 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)
    if (!bAssetEditorContextActive && ActiveLayoutState && EditorEngine && Settings.Panels.bViewport)
    {
        FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels();
        FLevelViewportLayout *ViewportLayout = Panels ? Panels->ViewportLayout.get() : nullptr;
        if (ViewportLayout)
        {
            SCOPE_STAT_CAT("ActiveLevelTabViewportLayout.RenderViewportUI", "5_UI");
            ViewportLayout->RenderViewportUI(DeltaTime);

            if (FLevelEditorViewportClient *ActiveViewport = ViewportLayout->GetActiveViewport())
            {
                EditorEngine->GetOverlayStatSystem().RenderImGui(*EditorEngine, ActiveViewport->GetViewportScreenRect());
            }
        }
    }

    if (!bAssetEditorContextActive && ActiveLayoutState && !bHideEditorWindows && Settings.Panels.bImGuiSettings)
    {
        FEditorImGuiStyleSettings::ShowPanel(&Settings.Panels.bImGuiSettings);
    }

    if (!bAssetEditorContextActive && ActiveLayoutState && !bHideEditorWindows && Settings.Panels.bConsole)
    {
        SCOPE_STAT_CAT("ConsolePanel.Render", "5_UI");
        if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels()) { if (Panels->ConsolePanel) { Panels->ConsolePanel->Render(DeltaTime); } }
    }

    if (!bAssetEditorContextActive && ActiveLayoutState && !bHideEditorWindows && Settings.Panels.bDetails)
    {
        SCOPE_STAT_CAT("DetailsPanel.Render", "5_UI");
        if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels()) { if (Panels->DetailsPanel) { Panels->DetailsPanel->Render(DeltaTime); } }
    }

    if (!bAssetEditorContextActive && ActiveLayoutState && !bHideEditorWindows && Settings.Panels.bOutliner)
    {
        SCOPE_STAT_CAT("OutlinerPanel.Render", "5_UI");
        if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels()) { if (Panels->OutlinerPanel) { Panels->OutlinerPanel->Render(DeltaTime); } }
    }

    if (!bAssetEditorContextActive && ActiveLayoutState && !bHideEditorWindows && Settings.Panels.bPlaceActors)
    {
        SCOPE_STAT_CAT("PlaceActorsPanel.Render", "5_UI");
        if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels()) { if (Panels->PlaceActorsPanel) { Panels->PlaceActorsPanel->Render(DeltaTime); } }
    }

    if (!bAssetEditorContextActive && ActiveLayoutState && !bHideEditorWindows && Settings.Panels.bStats)
    {
        SCOPE_STAT_CAT("StatPanel.Render", "5_UI");
        if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels()) { if (Panels->StatPanel) { Panels->StatPanel->Render(DeltaTime); } }
    }

    if (!bAssetEditorContextActive && ActiveLayoutState && !bHideEditorWindows && Settings.Panels.bContentBrowser)
    {
        SCOPE_STAT_CAT("ContentBrowser.Render", "5_UI");
        if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels()) { if (Panels->ContentBrowser) { Panels->ContentBrowser->Render(DeltaTime); } }
    }

    if (!bAssetEditorContextActive && ActiveLayoutState && !bHideEditorWindows && Settings.Panels.bShadowMapDebug)
    {
        if (FLevelDocumentPanels *Panels = GetActiveLevelDocumentPanels()) { if (Panels->ShadowMapDebugPanel) { Panels->ShadowMapDebugPanel->Render(DeltaTime); } }
    }

    if (!bAssetEditorContextActive && ActiveLayoutState)
    {
        FPanel::ConsumeCapturedLayoutRestoreFrame(ActiveLayoutState);
        FPanel::ClearCurrentLayoutState();
        FPanel::ClearCurrentDockspaceId();
        FPanel::ClearCurrentStableIdPrefix();
    }

    if (EditorEngine)
    {
        EditorEngine->RenderPIEOverlayPopups();
    }

    // FlushPanelDecorations()는 UEditorEngine::RenderUI()에서 Level/Asset 패널을 모두 렌더링한 뒤 한 번만 호출한다.
    // 그래야 FBX/SkeletalMesh 패널도 Level Editor 패널과 같은 dock tab fill / selected accent line을 적용받는다.
    RenderCommonOverlays();

    // 토스트 알림은 UEditorEngine::RenderUI()에서 Level/Asset Editor 렌더 이후 한 번만 그린다.

}

void FLevelEditorWindow::BuildFileMenu()
{
    DrawPopupSectionHeader("SCENE");
    if (ImGui::MenuItem("New Scene", "Ctrl+N") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::NewScene;
        return;
    }
    if (ImGui::MenuItem("Open Scene...", "Ctrl+O") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::OpenScene;
        return;
    }
    if (ImGui::MenuItem("Save Scene", "Ctrl+S") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::SaveScene;
        return;
    }
    if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::SaveSceneAs;
        return;
    }

    DrawPopupSectionHeader("ASSET");
    if (ImGui::MenuItem("Open Asset Editor") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::NewUAsset;
        return;
    }
    if (ImGui::MenuItem("Open UAsset...", "Ctrl+Alt+O") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::OpenUAsset;
        return;
    }

    DrawPopupSectionHeader("IMPORT");
    if (ImGui::MenuItem("Import Asset...", "Ctrl+Alt+I") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::ImportAsset;
        return;
    }

    DrawPopupSectionHeader("PACKAGE");
    if (ImGui::MenuItem("Package: Release..."))
    {
        PendingMenuAction = EPendingMenuAction::PackageRelease;
        return;
    }
    if (ImGui::MenuItem("Package: Shipping..."))
    {
        PendingMenuAction = EPendingMenuAction::PackageShipping;
        return;
    }
    if (ImGui::MenuItem("Package: Demo..."))
    {
        PendingMenuAction = EPendingMenuAction::PackageDemo;
        return;
    }
}

void FLevelEditorWindow::BuildEditMenu()
{
    const bool bCanUndo = EditorEngine && EditorEngine->CanUndoTransformChange();
    const bool bCanRedo = EditorEngine && EditorEngine->CanRedoTransformChange();
    if (!bCanUndo)
        ImGui::BeginDisabled();
    if (ImGui::MenuItem("Undo", "Ctrl+Z") && EditorEngine)
        EditorEngine->UndoTrackedTransformChange();
    if (!bCanUndo)
        ImGui::EndDisabled();
    if (!bCanRedo)
        ImGui::BeginDisabled();
    if (ImGui::MenuItem("Redo", "Ctrl+Y") && EditorEngine)
        EditorEngine->RedoTrackedTransformChange();
    if (!bCanRedo)
        ImGui::EndDisabled();
}

void FLevelEditorWindow::BuildWindowMenu()
{
    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();

    if (bHideEditorWindows)
    {
        if (ImGui::MenuItem("Restore Level Editor Panels"))
        {
            ShowEditorWindows();
        }
        ImGui::Separator();
    }

    if (ImGui::MenuItem("Viewport", nullptr, Settings.Panels.bViewport))
        Settings.Panels.bViewport = !Settings.Panels.bViewport;
    ImGui::Separator();
    if (ImGui::MenuItem("Console", nullptr, Settings.Panels.bConsole))
        Settings.Panels.bConsole = !Settings.Panels.bConsole;
    if (ImGui::MenuItem("Details", nullptr, Settings.Panels.bDetails))
        Settings.Panels.bDetails = !Settings.Panels.bDetails;
    if (ImGui::MenuItem("Outliner", nullptr, Settings.Panels.bOutliner))
        Settings.Panels.bOutliner = !Settings.Panels.bOutliner;
    if (ImGui::MenuItem("Place Actors", nullptr, Settings.Panels.bPlaceActors))
        Settings.Panels.bPlaceActors = !Settings.Panels.bPlaceActors;
    if (ImGui::MenuItem("Stat Profiler", nullptr, Settings.Panels.bStats))
        Settings.Panels.bStats = !Settings.Panels.bStats;
    if (ImGui::MenuItem("Content Browser", nullptr, Settings.Panels.bContentBrowser))
        Settings.Panels.bContentBrowser = !Settings.Panels.bContentBrowser;
    if (ImGui::MenuItem("Shadow Map Debug", nullptr, Settings.Panels.bShadowMapDebug))
        Settings.Panels.bShadowMapDebug = !Settings.Panels.bShadowMapDebug;
    ImGui::Separator();
    if (ImGui::MenuItem("ImGui Style Settings", nullptr, Settings.Panels.bImGuiSettings))
        Settings.Panels.bImGuiSettings = !Settings.Panels.bImGuiSettings;
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Default Layout"))
    {
        RequestDefaultDockLayout();
    }
}

void FLevelEditorWindow::BuildCustomMenus()
{
    if (ImGui::BeginMenu("Stat"))
    {
        if (EditorEngine)
        {
            FOverlayStatSystem &OverlayStats = EditorEngine->GetOverlayStatSystem();
            for (EOverlayStatType StatType : SupportedOverlayStats)
            {
                bool bVisible = OverlayStats.IsStatVisible(StatType);
                if (ImGui::MenuItem(FOverlayStatSystem::GetStatDisplayName(StatType), nullptr, bVisible))
                {
                    OverlayStats.SetStatVisible(StatType, !bVisible);
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Hide All"))
            {
                OverlayStats.HideAll();
            }
        }
        else
        {
            ImGui::BeginDisabled();
            for (EOverlayStatType StatType : SupportedOverlayStats)
            {
                ImGui::MenuItem(FOverlayStatSystem::GetStatDisplayName(StatType), nullptr, false, false);
            }
            ImGui::Separator();
            ImGui::MenuItem("Hide All", nullptr, false, false);
            ImGui::EndDisabled();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Levels"))
    {
        UWorld *World = EditorEngine ? EditorEngine->GetWorld() : nullptr;
        if (World)
        {
            ULevel *Persistent = World->GetPersistentLevel();
            FString PersistentName = Persistent ? "Persistent Level" : "No Persistent Level";
            bool bIsPersistentCurrent = (World->GetCurrentLevel() == Persistent);
            if (ImGui::MenuItem(PersistentName.c_str(), nullptr, bIsPersistentCurrent))
            {
                World->SetCurrentLevel(Persistent);
            }

            ImGui::Separator();
            ImGui::TextDisabled("Streaming Levels");

            for (const auto &Info : World->GetStreamingLevels())
            {
                bool bIsCurrent = (World->GetCurrentLevel() == Info.LoadedLevel);
                FString DisplayName = Info.LevelName.ToString() + (Info.bIsLoaded ? "" : " (Unloaded)");

                if (ImGui::MenuItem(DisplayName.c_str(), nullptr, bIsCurrent))
                {
                    if (Info.LoadedLevel)
                    {
                        World->SetCurrentLevel(Info.LoadedLevel);
                    }
                }

                if (ImGui::BeginPopupContextItem())
                {
                    if (!Info.bIsLoaded)
                    {
                        if (ImGui::MenuItem("Load Level"))
                        {
                            World->LoadStreamingLevel(Info.LevelPath);
                        }
                    }
                    else if (ImGui::MenuItem("Unload Level"))
                    {
                        World->UnloadStreamingLevel(Info.LevelName);
                    }
                    ImGui::EndPopup();
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Add Existing Level..."))
            {
                const std::wstring InitialDir = FSceneSaveManager::GetSceneDirectory();
                const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
                    .Filter = L"Level Files (*.umap)\0*.umap\0",
                    .Title = L"Add Existing Level",
                    .InitialDirectory = InitialDir.c_str(),
                    .OwnerWindowHandle = Window ? Window->GetHWND() : nullptr,
                    .bFileMustExist = true,
                    .bPathMustExist = true,
                    .bPromptOverwrite = false,
                    .bReturnRelativeToProjectRoot = false,
                });
                if (!SelectedPath.empty())
                {
                    World->AddStreamingLevel(SelectedPath);
                }
            }

            if (Persistent && ImGui::BeginMenu("GameMode Override"))
            {
                const TArray<UClass *> Candidates = UClass::GetSubclassesOf(AGameModeBase::StaticClass());
                const FString CurrentName = Persistent->GetGameModeClassName();

                if (ImGui::MenuItem("(Use Project Default)", nullptr, CurrentName.empty()))
                {
                    Persistent->SetGameModeClassName("");
                }
                ImGui::Separator();
                for (UClass *C : Candidates)
                {
                    const bool bSelected = (CurrentName == C->GetName());
                    if (ImGui::MenuItem(C->GetName(), nullptr, bSelected))
                    {
                        Persistent->SetGameModeClassName(C->GetName());
                    }
                }
                ImGui::EndMenu();
            }
        }
        ImGui::EndMenu();
    }
}

FString FLevelEditorWindow::GetFrameTitle() const
{
    if (IsActiveDocumentAssetEditor())
    {
        return EditorEngine->GetAssetEditorManager().GetAssetEditorWindow().GetFrameTitle();
    }

    FString Title = FString("Level Editor | ") + GetSceneTitleLabel(EditorEngine);
    if (const FLevelDocumentTab *Tab = GetActiveLevelDocumentTab())
    {
        if (Tab->bDirty)
        {
            Title += "*";
        }
    }
    return Title;
}

FString FLevelEditorWindow::GetFrameTitleTooltip() const
{
    if (IsActiveDocumentAssetEditor())
    {
        return EditorEngine->GetAssetEditorManager().GetAssetEditorWindow().GetFrameTitleTooltip();
    }

    if (EditorEngine && EditorEngine->HasCurrentLevelFilePath())
    {
        return EditorEngine->GetCurrentLevelFilePath();
    }

    return "Level Editor | Unsaved Scene";
}

void FLevelEditorWindow::Update()
{
    HandleGlobalShortcuts();
}

void FLevelEditorWindow::HandleGlobalShortcuts()
{
    if (!EditorEngine)
    {
        return;
    }
    if (EditorEngine->IsPIEPossessedMode())
    {
        return;
    }

    ImGuiIO &IO = ImGui::GetIO();
    if (IO.WantTextInput)
    {
        return;
    }

    FInputManager   &Input = FInputManager::Get();
    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();

    if (Input.IsKeyPressed(VK_OEM_3))
    {
        Settings.Panels.bConsole = !Settings.Panels.bConsole;
        return;
    }

    if (!Input.IsKeyDown(VK_CONTROL))
    {
        return;
    }

    const bool bShift = Input.IsKeyDown(VK_SHIFT);
    if (Input.IsKeyPressed(VK_SPACE))
    {
        Settings.Panels.bContentBrowser = !Settings.Panels.bContentBrowser;
        return;
    }

    if (Input.IsKeyPressed('N'))
    {
        HWND OwnerWindowHandle = Window ? Window->GetHWND() : nullptr;
        if (ConfirmCloseActiveLevelDocument() && ConfirmNewScene(OwnerWindowHandle))
        {
            EditorEngine->NewScene();
            ReplaceActiveLevelDocumentTabFromCurrentScene();
        }
    }
    else if (Input.IsKeyPressed('O'))
    {
        if (ConfirmCloseActiveLevelDocument() && EditorEngine->LoadSceneWithDialog())
        {
            ReplaceActiveLevelDocumentTabFromCurrentScene();
        }
    }
    else if (Input.IsKeyPressed('S'))
    {
        if (bShift)
        {
            EditorEngine->RequestSaveSceneAsDialog();
        }
        else
        {
            if (EditorEngine->SaveScene())
            {
                MarkActiveLevelDocumentClean();
            }
        }
    }
    else if (Input.IsKeyPressed('Z'))
    {
        EditorEngine->UndoTrackedTransformChange();
    }
    else if (Input.IsKeyPressed('Y'))
    {
        EditorEngine->RedoTrackedTransformChange();
    }
}

void FLevelEditorWindow::HideEditorWindows()
{
    if (bHasSavedPanelVisibility)
    {
        bHideEditorWindows = true;
        bShowPanelList = false;
        return;
    }

    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
    SavedPanelVisibility = Settings.Panels;
    bSavedShowPanelList = bShowPanelList;
    bHasSavedPanelVisibility = true;
    bHideEditorWindows = true;
    bShowPanelList = false;

    Settings.Panels.bConsole = false;
    Settings.Panels.bDetails = false;
    Settings.Panels.bOutliner = false;
    Settings.Panels.bPlaceActors = false;
    Settings.Panels.bStats = false;
    Settings.Panels.bContentBrowser = false;
    Settings.Panels.bImGuiSettings = false;
    Settings.Panels.bShadowMapDebug = false;
}

void FLevelEditorWindow::HideLevelEditorUIForAssetEditor()
{
    // 다중 document tab 구조에서는 Asset 탭으로 전환해도 Level Editor 패널 표시 설정을 변경하지 않는다.
    // 실제 렌더링 여부는 active document context가 결정한다.
}

void FLevelEditorWindow::RestoreLevelEditorUIAfterAssetEditor()
{
    // Level 패널 표시 설정은 탭 전환 중에 건드리지 않는다.
    // 다만 Asset 탭에서 Level 탭으로 돌아오는 첫 프레임에는 마지막으로 캡처한
    // panel -> dock node 관계를 한 번 재적용해야 Viewport/Details가 floating으로
    // 떨어지거나 사라지지 않는다.
    if (FLevelDocumentTab *Tab = GetActiveLevelDocumentTab())
    {
        if (!Tab->Panels)
        {
            InitializeLevelDocumentPanels(*Tab);
        }
        if (!Tab->LayoutState.PanelDockIds.empty())
        {
            FPanel::RequestCapturedLayoutRestore(&Tab->LayoutState);
        }
    }
}

void FLevelEditorWindow::ShowEditorWindows()
{
    if (!bHasSavedPanelVisibility)
    {
        bHideEditorWindows = false;
        return;
    }

    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
    Settings.Panels = SavedPanelVisibility;
    bShowPanelList = bSavedShowPanelList;
    bHideEditorWindows = false;
    bHasSavedPanelVisibility = false;
}

void FLevelEditorWindow::HideEditorWindowsForPIE() { HideEditorWindows(); }

void FLevelEditorWindow::RestoreEditorWindowsAfterPIE() { ShowEditorWindows(); }

void FLevelEditorWindow::CookCurrentScene()
{
    if (!EditorEngine || !EditorEngine->HasCurrentLevelFilePath())
    {
        FNotificationManager::Get().AddNotification("Cook: save the current scene first.", ENotificationType::Error);
        return;
    }

    const FString        &InPath = EditorEngine->GetCurrentLevelFilePath();
    std::filesystem::path Out(FPaths::ToWide(InPath));
    Out.replace_extension(L".umap");
    const FString OutPath = FPaths::ToUtf8(Out.wstring());

    const bool bOk = FSceneSaveManager::CookSceneToBinary(InPath, OutPath);
    FNotificationManager::Get().AddNotification(bOk ? std::string("Cooked: ") + Out.filename().string()
                                                    : std::string("Cook failed: ") + Out.filename().string(),
                                                bOk ? ENotificationType::Success : ENotificationType::Error);
}

void FLevelEditorWindow::PackageGameBuild(const char *BatFileName)
{
    // 솔루션 루트(.bat 위치)를 찾는다 — 후보 경로를 차례대로 검사.
    // FPaths::RootDir()은 보통 LunaticEngine/ (개발) 또는 exe 디렉터리(배포)를 반환한다.
    // 트레일링 슬래시 때문에 parent_path()가 의도대로 안 나올 수 있으므로 lexically_normal로 정규화.
    std::filesystem::path RootDir = std::filesystem::path(FPaths::RootDir()).lexically_normal();

    std::filesystem::path       SolutionDir;
    std::filesystem::path       BatPath;
    const std::filesystem::path Candidates[] = {
        RootDir,                             // exe 디렉터리에 .bat이 있는 경우 (배포)
        RootDir.parent_path(),               // LunaticEngine/의 상위 = 솔루션 루트 (개발)
        RootDir.parent_path().parent_path(), // 한 단계 더 (혹시 모를 중첩)
        std::filesystem::current_path(),     // 마지막 폴백
    };
    for (const auto &Candidate : Candidates)
    {
        const std::filesystem::path Tentative = Candidate / BatFileName;
        if (std::filesystem::exists(Tentative))
        {
            SolutionDir = Candidate;
            BatPath = Tentative;
            break;
        }
    }

    if (BatPath.empty())
    {
        FNotificationManager::Get().AddNotification(std::string("Package script not found: ") + BatFileName + " (searched near " +
                                                        RootDir.string() + ")",
                                                    ENotificationType::Error);
        return;
    }

    // .bat은 별도 콘솔 창에서 실행 (편집 중인 에디터를 막지 않게).
    // "cmd /c start \"Title\" /D <SolutionDir> cmd /k \"<bat>\"" 형태로 cmd 콘솔에서 띄움.
    std::wstring SolutionDirW = SolutionDir.wstring();
    std::wstring BatPathW = BatPath.wstring();

    std::wstring CommandLine = L"/c start \"Package Game Build\" /D \"" + SolutionDirW + L"\" cmd /k \"\"" + BatPathW + L"\"\"";

    HINSTANCE Result =
        ShellExecuteW(Window ? Window->GetHWND() : nullptr, L"open", L"cmd.exe", CommandLine.c_str(), SolutionDirW.c_str(), SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(Result) <= 32)
    {
        FNotificationManager::Get().AddNotification(std::string("Failed to launch package script: ") + BatFileName,
                                                    ENotificationType::Error);
        return;
    }

    FNotificationManager::Get().AddNotification(std::string("Packaging started: ") + BatFileName, ENotificationType::Info);
}

void FLevelEditorWindow::UpdateInputState(bool bMouseOverViewport, bool bAssetEditorCapturingInput, bool bPIEPopupOpen)
{
    MakeCurrentContext();
    ImGuiIO &IO = ImGui::GetIO();

    bool bWantMouse = IO.WantCaptureMouse;
    bool bWantKeyboard = IO.WantCaptureKeyboard || HasBlockingOverlayOpen();
    if (bPIEPopupOpen)
    {
        bWantMouse = true;
        bWantKeyboard = true;
    }

    if (EditorEngine && bMouseOverViewport && !bAssetEditorCapturingInput)
    {
        if (!bPIEPopupOpen)
        {
            bWantMouse = false;
            if (!IO.WantTextInput && !HasBlockingOverlayOpen())
            {
                bWantKeyboard = false;
            }
        }
    }

    FInputManager::Get().SetGuiCaptureOverride(bWantMouse, bWantKeyboard, IO.WantTextInput);

    if (Window)
    {
        HWND hWnd = Window->GetHWND();
        if (IO.WantTextInput)
        {
            ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
        }
        else
        {
            ImmAssociateContext(hWnd, NULL);
        }
    }
}

bool FLevelEditorWindow::HasBlockingOverlayOpen() const
{
    return bShowProjectSettings || bShowShortcutOverlay || bShowCreditsOverlay;
}

void FLevelEditorWindow::MakeCurrentContext() const
{
    if (ImGuiSystem)
    {
        ImGuiSystem->MakeCurrentContext();
    }
}

void FLevelEditorWindow::RenderCommonOverlays()
{
    RenderProjectSettingsWindow();
    RenderShortcutOverlay();
    RenderCreditsOverlay();
}


void FLevelEditorWindow::RenderProjectSettingsWindow()
{
    if (!bShowProjectSettings)
    {
        return;
    }

    if (!BeginUtilityPopupWindow("Project Settings", &bShowProjectSettings, ImVec2(560.0f, 460.0f), ImGuiCond_Appearing))
    {
        ImGui::End();
        return;
    }

    FProjectSettings &ProjectSettings = FProjectSettings::Get();

    auto DrawClassDropdown = [](const char *Label, UClass *BaseClass, FString &InOutValue)
    {
        const TArray<UClass *> Candidates = UClass::GetSubclassesOf(BaseClass);
        const char *Preview = InOutValue.empty() ? "(none)" : InOutValue.c_str();
        if (ImGui::BeginCombo(Label, Preview))
        {
            for (UClass *C : Candidates)
            {
                const bool bSelected = (InOutValue == C->GetName());
                if (ImGui::Selectable(C->GetName(), bSelected))
                {
                    InOutValue = C->GetName();
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    };

    DrawPopupSectionHeader("WINDOW");
    ImGui::InputScalar("Window Width", ImGuiDataType_U32, &ProjectSettings.Game.WindowWidth);
    ImGui::InputScalar("Window Height", ImGuiDataType_U32, &ProjectSettings.Game.WindowHeight);
    ImGui::Checkbox("Lock Resolution", &ProjectSettings.Game.bLockWindowResolution);
    if (ProjectSettings.Game.WindowWidth < 320)
        ProjectSettings.Game.WindowWidth = 320;
    if (ProjectSettings.Game.WindowHeight < 240)
        ProjectSettings.Game.WindowHeight = 240;

    ImGui::PushStyleColor(ImGuiCol_Button, UIAccentColor::WithAlpha(0.92f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UIAccentColor::Value);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIAccentColor::Value);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.97f, 0.98f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 8.0f));
    if (ImGui::Button("Apply Resolution"))
    {
        ProjectSettings.SaveToFile(FProjectSettings::GetDefaultPath());
        if (Window)
        {
            Window->ResizeClientArea(ProjectSettings.Game.WindowWidth, ProjectSettings.Game.WindowHeight);
        }
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::SameLine();
    ImGui::TextDisabled(ProjectSettings.Game.bLockWindowResolution
                            ? "Packaged game resize is locked. The editor window stays resizable."
                            : "The editor stays resizable. Packaged game uses this size on next launch.");

    DrawPopupSectionHeader("PERFORMANCE");
    bool bPerformanceChanged = false;
    bPerformanceChanged |= ImGui::Checkbox("Limit FPS", &ProjectSettings.Performance.bLimitFPS);
    ImGui::BeginDisabled(!ProjectSettings.Performance.bLimitFPS);
    bPerformanceChanged |= ImGui::InputScalar("Max FPS", ImGuiDataType_U32, &ProjectSettings.Performance.MaxFPS);
    ImGui::EndDisabled();
    if (ProjectSettings.Performance.MaxFPS == 0)
    {
        ProjectSettings.Performance.MaxFPS = 1;
    }
    else if (ProjectSettings.Performance.MaxFPS > 1000)
    {
        ProjectSettings.Performance.MaxFPS = 1000;
    }
    if (bPerformanceChanged && GEngine && GEngine->GetTimer())
    {
        GEngine->GetTimer()->SetMaxFPS(ProjectSettings.Performance.bLimitFPS ? static_cast<float>(ProjectSettings.Performance.MaxFPS)
                                                                             : 0.0f);
    }

    DrawPopupSectionHeader("SHADOW");
    ImGui::Checkbox("Enable Shadows", &ProjectSettings.Shadow.bEnabled);
    ImGui::InputScalar("CSM Resolution", ImGuiDataType_U32, &ProjectSettings.Shadow.CSMResolution);
    ImGui::InputScalar("Spot Atlas Resolution", ImGuiDataType_U32, &ProjectSettings.Shadow.SpotAtlasResolution);
    ImGui::InputScalar("Point Atlas Resolution", ImGuiDataType_U32, &ProjectSettings.Shadow.PointAtlasResolution);
    ImGui::InputScalar("Max Spot Atlas Pages", ImGuiDataType_U32, &ProjectSettings.Shadow.MaxSpotAtlasPages);
    ImGui::InputScalar("Max Point Atlas Pages", ImGuiDataType_U32, &ProjectSettings.Shadow.MaxPointAtlasPages);

    DrawPopupSectionHeader("LIGHT CULLING");
    int32 LightCullingMode = static_cast<int32>(ProjectSettings.LightCulling.Mode);
    ImGui::RadioButton("Off", &LightCullingMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Tile", &LightCullingMode, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Cluster", &LightCullingMode, 2);
    ProjectSettings.LightCulling.Mode = static_cast<uint32>(LightCullingMode);
    ImGui::SliderFloat("Heat Map Max", &ProjectSettings.LightCulling.HeatMapMax, 1.0f, 100.0f, "%.0f");
    ImGui::Checkbox("Enable 2.5D Culling", &ProjectSettings.LightCulling.bEnable25DCulling);

    DrawPopupSectionHeader("SCENE DEPTH");
    int32 SceneDepthMode = static_cast<int32>(ProjectSettings.SceneDepth.Mode);
    ImGui::Combo("Mode", &SceneDepthMode, "Power\0Linear\0");
    ProjectSettings.SceneDepth.Mode = static_cast<uint32>(SceneDepthMode);
    ImGui::SliderFloat("Exponent", &ProjectSettings.SceneDepth.Exponent, 1.0f, 512.0f, "%.0f");

    DrawPopupSectionHeader("GAME");
    DrawClassDropdown("GameInstance Class", UGameInstance::StaticClass(), ProjectSettings.Game.GameInstanceClass);
    DrawClassDropdown("Default GameMode Class", AGameModeBase::StaticClass(), ProjectSettings.Game.DefaultGameModeClass);

    const TArray<FString> Scenes = FSceneSaveManager::GetSceneFileList();
    const char *Preview = ProjectSettings.Game.DefaultScene.empty() ? "(none)" : ProjectSettings.Game.DefaultScene.c_str();
    if (ImGui::BeginCombo("Default Map", Preview))
    {
        for (const FString &Stem : Scenes)
        {
            const bool bSelected = (ProjectSettings.Game.DefaultScene == Stem);
            if (ImGui::Selectable(Stem.c_str(), bSelected))
            {
                ProjectSettings.Game.DefaultScene = Stem;
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::TextDisabled("(GameInstance class change requires restart)");

    if (ImGui::Button("Save"))
    {
        ProjectSettings.SaveToFile(FProjectSettings::GetDefaultPath());
    }
    ImGui::SameLine();
    if (ImGui::Button("Close"))
    {
        bShowProjectSettings = false;
    }

    ImGui::End();
}

void FLevelEditorWindow::RenderShortcutOverlay()
{
    if (!bShowShortcutOverlay)
    {
        return;
    }

    if (!BeginUtilityPopupWindow("Shortcut Help", &bShowShortcutOverlay, ImVec2(320.0f, 150.0f), ImGuiCond_Appearing,
                                 ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("File");
    ImGui::Separator();
    ImGui::TextUnformatted("Ctrl+N : New Scene");
    ImGui::TextUnformatted("Ctrl+O : Open Scene");
    ImGui::TextUnformatted("Ctrl+S : Save Scene");
    ImGui::TextUnformatted("Ctrl+Shift+S : Save Scene As");
    ImGui::TextUnformatted("Ctrl+Z : Undo Scene Change");
    ImGui::TextUnformatted("Ctrl+Y : Redo Scene Change");
    ImGui::TextUnformatted("` : Toggle Console");
    ImGui::TextUnformatted("Ctrl+Space : Toggle Content Browser");
    ImGui::Separator();
    ImGui::TextUnformatted("F : Focus on selection");
    ImGui::TextUnformatted("Ctrl + LMB : Multi Picking (Toggle)");
    ImGui::TextUnformatted("Ctrl + Alt + LMB Drag : Area Selection");

    ImGui::End();
}

void FLevelEditorWindow::RenderCreditsOverlay()
{
    if (!bShowCreditsOverlay)
    {
        return;
    }

    if (!BeginUtilityPopupWindow("Credits", &bShowCreditsOverlay, ImVec2(420.0f, 560.0f), ImGuiCond_Appearing,
                                 ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ID3D11ShaderResourceView *CreditsTexture = FResourceManager::Get().FindLoadedTexture("Asset/Source/Editor/Icons/App/lunatic_icon.png").Get();
    if (!CreditsTexture)
    {
        CreditsTexture = FResourceManager::Get().FindLoadedTexture(FResourceManager::Get().ResolvePath(FName("Editor.Icon.AppLogo"))).Get();
    }

    if (CreditsTexture)
    {
        constexpr float ImageSize = 180.0f;
        const float CursorX = (ImGui::GetContentRegionAvail().x - ImageSize) * 0.5f;
        ImGui::SetCursorPosX((std::max)(CursorX, 0.0f));
        ImGui::Image(CreditsTexture, ImVec2(ImageSize, ImageSize));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float TitleWidth = ImGui::CalcTextSize("Developers").x;
    ImGui::SetCursorPosX((std::max)((ImGui::GetContentRegionAvail().x - TitleWidth) * 0.5f, 0.0f));
    ImGui::TextUnformatted("Developers");
    ImGui::Spacing();

    for (const char *Developer : CreditsDevelopers)
    {
        const float NameWidth = ImGui::CalcTextSize(Developer).x;
        ImGui::SetCursorPosX((std::max)((ImGui::GetContentRegionAvail().x - NameWidth) * 0.5f, 0.0f));
        ImGui::TextUnformatted(Developer);
    }

    ImGui::End();
}
