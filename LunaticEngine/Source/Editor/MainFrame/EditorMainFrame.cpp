#include "PCH/LunaticPCH.h"
#include "MainFrame/EditorMainFrame.h"

#include "Common/UI/Style/AccentColor.h"
#include "Common/UI/Style/EditorUIStyle.h"
#include "Core/ProjectSettings.h"
#include "EditorEngine.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "Object/UClass.h"
#include "Resource/ResourceManager.h"
#include "Render/Pipeline/Renderer.h"

#include "ImGui/imgui.h"
#include <imm.h>
#include <windows.h>

namespace
{
    constexpr ImVec4 UnrealPanelSurface = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
    constexpr ImVec4 UnrealPanelSurfaceHover = ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f);
    constexpr ImVec4 UnrealPanelSurfaceActive = ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f);
    constexpr ImVec4 UnrealDockEmpty = ImVec4(5.0f / 255.0f, 5.0f / 255.0f, 5.0f / 255.0f, 1.0f);
    constexpr ImVec4 UnrealPopupSurface = ImVec4(42.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 0.98f);
    constexpr ImVec4 UnrealBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
    constexpr ImVec4 PopupSectionHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
    constexpr const char *CreditsDevelopers[] = {
        "Hojin Lee", "HyoBeom Kim", "Hyungjun Kim", "JunHyeop3631", "keonwookang0914", "kimhojun", "kwonhyeonsoo-goo",
        "LEE SangHoon", "lin-ion", "Park SangHyeok", "Seyoung Park", "ShimWoojin", "wwonnn", "Yonaim",
        "\xEA\xB0\x95\xEA\xB1\xB4\xEC\x9A\xB0", "\xEA\xB9\x80\xED\x83\x9C\xED\x98\x84", "\xEB\x82\xA8\xEC\x9C\xA4\xEC\xA7\x80",
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_Border, UnrealBorder);
        const bool bVisible = ImGui::Begin(Title, bOpen, Flags);
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
        return bVisible;
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
    }
} // namespace

void FEditorMainFrame::Create(FWindowsWindow *InWindow, FRenderer &InRenderer, UEditorEngine *InEditorEngine)
{
    // FEditorMainFrame은 최상위 UI 루트 역할만 한다.
    // ImGui Context / Backend / Frame 생명주기는 FEditorImGuiSystem이 단독으로 관리한다.
    (void)InRenderer;
    Window = InWindow;
    EditorEngine = InEditorEngine;
}

void FEditorMainFrame::Release()
{
    Window = nullptr;
    EditorEngine = nullptr;
}

void FEditorMainFrame::BeginFrame()
{
    MakeCurrentContext();
}

void FEditorMainFrame::RenderMainMenuBar(IEditorMenuProvider *MenuProvider)
{
    FEditorMainMenuBarContext Context{};
    Context.Window = Window;
    Context.EditorEngine = EditorEngine;
    Context.MenuProvider = MenuProvider;
    Context.TitleBarFont = EditorEngine ? EditorEngine->GetImGuiSystem().GetTitleBarFont() : nullptr;
    Context.WindowControlIconFont = EditorEngine ? EditorEngine->GetImGuiSystem().GetWindowControlIconFont() : nullptr;
    Context.bShowProjectSettings = &bShowProjectSettings;
    Context.bShowShortcutOverlay = &bShowShortcutOverlay;
    Context.bShowCreditsOverlay = &bShowCreditsOverlay;
    MainMenuBar.Render(Context);
}

void FEditorMainFrame::RenderCommonOverlays()
{
    RenderProjectSettingsWindow();
    RenderShortcutOverlay();
    RenderCreditsOverlay();
}

void FEditorMainFrame::EndFrame()
{
    // ImGui::Render()와 backend draw 제출은 FEditorImGuiSystem이 프레임당 한 번만 수행한다.
}

void FEditorMainFrame::UpdateInputState(bool bMouseOverViewport, bool bAssetEditorCapturingInput, bool bPIEPopupOpen)
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

bool FEditorMainFrame::HasBlockingOverlayOpen() const
{
    return bShowProjectSettings || bShowShortcutOverlay || bShowCreditsOverlay;
}

void FEditorMainFrame::MakeCurrentContext() const
{
    if (EditorEngine)
    {
        EditorEngine->GetImGuiSystem().MakeCurrentContext();
    }
}

void FEditorMainFrame::RenderProjectSettingsWindow()
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

void FEditorMainFrame::RenderShortcutOverlay()
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

void FEditorMainFrame::RenderCreditsOverlay()
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
