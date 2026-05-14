#pragma once

#include "Common/Menu/EditorMenuBar.h"
#include "Common/Menu/EditorMenuProvider.h"
#include "Common/UI/Panels/Panel.h"
#include "LevelEditor/Settings/LevelEditorSettings.h"
#include "ImGui/imgui.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "LevelEditor/UI/Debug/ShadowMapDebugPanel.h"
#include "LevelEditor/UI/Panels/ContentBrowser/ContentBrowser.h"
#include "LevelEditor/UI/Panels/LevelConsolePanel.h"
#include "LevelEditor/UI/Panels/LevelDetailsPanel.h"
#include "LevelEditor/UI/Panels/LevelOutlinerPanel.h"
#include "LevelEditor/UI/Panels/LevelPlaceActorsPanel.h"
#include "LevelEditor/UI/Panels/LevelStatPanel.h"
#include "LevelEditor/Viewport/LevelViewportLayout.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;
class FLevelEditor;
class FEditorImGuiSystem;

/**
 * 메인 Level Editor 창의 UI frame.
 *
 * 역할:
 * - 이미 생성된 메인 FWindowsWindow를 참조한다. 새 OS 창을 만들지 않는다.
 * - 공통 EditorMenuBar를 사용해 메뉴바를 그린다.
 * - Level Editor DockSpace와 Level 전용 패널을 렌더링한다.
 * - AssetEditorManager가 사용할 MainDockspaceId를 제공한다.
 *
 * 주의:
 * - 실제 Level 편집 로직은 FLevelEditor가 소유한다.
 * - 이 클래스는 Window/UI frame 역할만 담당한다.
 */
class FLevelEditorWindow : public IEditorMenuProvider
{
  public:
    void  Create(FWindowsWindow *InWindow, FRenderer &InRenderer, UEditorEngine *InEditorEngine, FLevelEditor *InLevelEditor);
    void  Release();
    void  RenderContent(float DeltaTime);
    void  Update();
    void  UpdateInputState(bool bMouseOverViewport, bool bAssetEditorCapturingInput, bool bPIEPopupOpen);
    void  SaveToSettings() const;
    void  HideEditorWindows();
    void  ShowEditorWindows();

    /**
     * FBX / SkeletalMesh Viewer 작업용 임시 집중 모드.
     *
     * 역할:
     * - 기존 Level Editor 패널과 Level Viewport를 모두 숨긴다.
     * - Asset Editor 관련 패널만 메인 DockSpace에 남기기 위한 진입점이다.
     * - 저장된 패널 표시 상태는 ShowEditorWindows()로 복구한다.
     *
     * 주의:
     * - 별도 Asset Editor Window 구조를 포기한 현재 단계에서만 쓰는 임시 UX이다.
     * - 나중에 Asset Editor 전용 Window/Workspace를 다시 만들면 이 함수는 제거하거나 Workspace 전환 함수로 대체한다.
     */
    void  HideLevelEditorUIForAssetEditor();
    void  RestoreLevelEditorUIAfterAssetEditor();
    void  SetShowEditorOnlyComponents(bool bEnable);
    bool  IsShowingEditorOnlyComponents() const;
    void  HideEditorWindowsForPIE();
    void  RestoreEditorWindowsAfterPIE();
    void  RefreshContentBrowser();
    void  SelectContentBrowserPath(const std::filesystem::path& Path);
    ImGuiID GetMainDockspaceId() const { return MainDockspaceId; }
    FLevelViewportLayout *GetActiveLevelViewportLayout();
    const FLevelViewportLayout *GetActiveLevelViewportLayout() const;
    bool IsActiveDocumentAssetEditor() const;
    void  SetContentBrowserIconSize(float Size);
    float GetContentBrowserIconSize() const;
    void  RequestDefaultDockLayout();
    void  FlushPendingMenuAction();
    void  MarkActiveLevelDocumentDirty();
    void  MarkActiveLevelDocumentClean();
    bool  HasDirtyLevelDocument() const;
    bool  CanCloseEditorWindowWithPrompt() const;

    // IEditorMenuProvider: 공통 메뉴바에 Level Editor 전용 메뉴를 제공한다.
    void BuildFileMenu() override;
    void BuildEditMenu() override;
    void BuildWindowMenu() override;
    void BuildCustomMenus() override;
    FString GetFrameTitle() const override;
    FString GetFrameTitleTooltip() const override;

  private:
    struct FLevelDocumentPanels;
    struct FLevelDocumentTab;

    enum class EPendingMenuAction
    {
        None,
        NewScene,
        OpenScene,
        SaveScene,
        SaveSceneAs,
        NewUAsset,
        OpenUAsset,
        ImportAsset,
        CookCurrentScene,
        CookAllScenes,
        PackageRelease,
        PackageShipping,
        PackageDemo,
    };

    void MakeCurrentContext() const;
    void ApplyPendingDefaultDockLayout();
    FLevelDocumentTab *GetActiveLevelDocumentTab();
    const FLevelDocumentTab *GetActiveLevelDocumentTab() const;
    FLevelDocumentPanels *GetActiveLevelDocumentPanels();
    const FLevelDocumentPanels *GetActiveLevelDocumentPanels() const;
    std::string GetActiveDocumentLayoutId() const;
    FDockPanelLayoutState *GetActiveDocumentLayoutState();
    void RequestRestoreForActiveDocument();
    bool HasBlockingOverlayOpen() const;
    void HandleGlobalShortcuts();
    void InitializeLevelDocumentPanels(FLevelDocumentTab &Tab);
    void ShutdownLevelDocumentPanels(FLevelDocumentTab &Tab);
    void OpenLevelDocumentTabFromCurrentScene();
    void ReplaceActiveLevelDocumentTabFromCurrentScene();
    void SyncCurrentLevelDocumentTab();
    bool SetActiveLevelDocumentTab(int32 NewIndex);
    bool ConfirmCloseLevelDocumentTab(int32 TabIndex) const;
    bool ConfirmCloseActiveLevelDocument() const;
    void CloseLevelDocumentTab(int32 TabIndex);
    void PackageGameBuild(const char *BatFileName);
    void CookCurrentScene();
    void RenderDocumentTabBar();
    void RenderLevelFrameToolbar();
    void RenderCommonOverlays();
    void RenderProjectSettingsWindow();
    void RenderShortcutOverlay();
    void RenderCreditsOverlay();

    FWindowsWindow *Window = nullptr;
    FRenderer      *Renderer = nullptr;
    UEditorEngine  *EditorEngine = nullptr;
    FLevelEditor   *LevelEditor = nullptr;
    FEditorImGuiSystem *ImGuiSystem = nullptr;

    FEditorMenuBar MenuBar;

    struct FLevelDocumentPanels
    {
        // Each document tab owns its own panel instances.
        // Layout state alone is not enough: ImGui windows such as Viewport/Details/Outliner
        // need stable per-tab owner instances so tab switching does not collapse everything
        // into one shared viewport/panel set.
        std::unique_ptr<FLevelViewportLayout> ViewportLayout;
        std::unique_ptr<FLevelConsolePanel> ConsolePanel;
        std::unique_ptr<FLevelDetailsPanel> DetailsPanel;
        std::unique_ptr<FLevelOutlinerPanel> OutlinerPanel;
        std::unique_ptr<FLevelPlaceActorsPanel> PlaceActorsPanel;
        std::unique_ptr<FLevelStatPanel> StatPanel;
        std::unique_ptr<FContentBrowser> ContentBrowser;
        std::unique_ptr<FShadowMapDebugPanel> ShadowMapDebugPanel;
    };

    struct FLevelDocumentTab
    {
        std::filesystem::path ScenePath;
        std::string Title;
        std::string LayoutId;
        FDockPanelLayoutState LayoutState;
        std::unique_ptr<FLevelDocumentPanels> Panels;
        bool bDirty = false;
    };

    std::vector<FLevelDocumentTab> LevelDocumentTabs;
    int32 ActiveLevelDocumentTabIndex = -1;
    uint32 NextUntitledSceneIndex = 1;
    uint32 NextLevelLayoutId = 1;
    bool bSuppressAutoLevelDocumentTab = false;

    bool bShowPanelList = false;
    bool bHideEditorWindows = false;
    bool bHasSavedPanelVisibility = false;
    bool bSavedShowPanelList = false;
    bool bShowProjectSettings = false;
    bool bShowShortcutOverlay = false;
    bool bShowCreditsOverlay = false;
    bool bPendingDefaultDockLayout = true;

    ImGuiID MainDockspaceId = 0;

    EPendingMenuAction PendingMenuAction = EPendingMenuAction::None;
    FLevelEditorSettings::FLevelEditorPanelVisibility SavedPanelVisibility{};
};
