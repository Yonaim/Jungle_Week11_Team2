#pragma once

#include "AssetEditor/Tabs/AssetEditorTabManager.h"
#include "Common/Menu/EditorMenuProvider.h"
#include "Common/UI/Panels/Panel.h"
#include "Common/UI/Tabs/EditorDocumentTabBar.h"
#include "ImGui/imgui.h"

#include <string>
#include <vector>

class UEditorEngine;
class FAssetEditorManager;
class IAssetEditor;
class FEditorViewportClient;

/**
 * Asset Editor 패널 컨트롤러.
 *
 * 주의:
 * - 이름은 아직 Window지만, 현재 구현에서는 별도 OS Window / 별도 ImGui Context를 만들지 않는다.
 * - 이전에 시도했던 Native Asset Editor Window 구조는 ImGui multi-context / backend 문제로 잠시 보류한다.
 * - 현재는 Level Editor의 메인 DockSpace 안에 Asset Editor 관련 패널을 추가하는 역할만 한다.
 *
 * 나중에 다시 별도 FWindowsWindow 기반 Asset Editor를 구현할 경우:
 * - 이 클래스 이름을 FAssetEditorPanelManager로 바꾸거나,
 * - 별도 Native Window용 FAssetEditorWindow 클래스를 새로 분리하는 것이 좋다.
 *
 * 현재 책임:
 * - Asset Editor를 열지/숨길지 상태 관리
 * - 에셋 에디터 탭/패널 목록을 FAssetEditorTabManager에 위임
 * - 아무 에셋도 열리지 않았을 때 빈 안내 패널 표시
 * - File/Edit/Window 메뉴 내용을 제공
 */
class FAssetEditorWindow : public IEditorMenuProvider
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FAssetEditorManager *InOwnerManager);
    void Shutdown();

    void Show();
    void Hide();
    void EnterEditorContext();
    void ExitEditorContext();

    bool IsOpen() const;
    bool HasOpenTabs() const;

    bool OpenEditorTab(std::unique_ptr<IAssetEditor> Editor);
    bool ActivateTabByAssetPath(const std::filesystem::path &AssetPath);
    bool SaveActiveTab();
    bool UndoActiveTab();
    bool RedoActiveTab();
    bool CanUndoActiveTab() const;
    bool CanRedoActiveTab() const;
    bool CloseActiveTab(bool bPromptForDirty = true);
    bool CloseAllTabs(bool bPromptForDirty = true, void *OwnerWindowHandle = nullptr);
    bool CloseDocumentTab(int32 TabIndex, bool bPromptForDirty = true);
    bool ActivateDocumentTab(int32 TabIndex);
    int32 GetDocumentTabCount() const;
    int32 GetActiveDocumentTabIndex() const;
    void AppendDocumentTabDescs(std::vector<FEditorDocumentTabBar::FTabDesc> &OutTabs) const;
    const std::string &GetActiveLayoutId() const;
    void CollectLayoutIds(std::vector<std::string> &OutLayoutIds) const;
    void RenderInactiveDockKeepAliveWindows(const std::string &ActiveLayoutId);
    FDockPanelLayoutState *GetActiveLayoutState();
    void RequestRestoreForActiveTab();
    void RequestDefaultLayoutForActiveTab();
    bool HasDirtyTabs() const;
    bool ConfirmCloseAllTabs(void *OwnerWindowHandle = nullptr) const;

    void Tick(float DeltaTime);

    /**
     * Level Editor DockSpace 안에 Asset Editor 패널들을 렌더링한다.
     * DockspaceId가 0이면 일반 ImGui window로 렌더링하고,
     * 유효하면 해당 DockSpace에 도킹 가능한 패널로 붙인다.
     */
    void RenderContent(float DeltaTime, ImGuiID DockspaceId = 0);

    bool IsCapturingInput() const;
    FEditorViewportClient *GetActiveViewportClient() const;
    void CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) const;
    void ForceDeactivateAllViewportClients();

    // IEditorMenuProvider: 공통 EditorMenuBar가 호출하는 메뉴 구성 함수들.
    void BuildFileMenu() override;
    void BuildEditMenu() override;
    void BuildWindowMenu() override;
    void BuildCustomMenus() override;
    FString GetFrameTitle() const override;
    FString GetFrameTitleTooltip() const override;

    static float GetFrameToolbarHeight();

  private:
    void RenderDocumentTabBar();
    void RenderAssetFrameToolbar();
    void RenderEmptyState(ImGuiID DockspaceId);
    void HandleGlobalShortcuts();

  private:
    UEditorEngine       *EditorEngine = nullptr;
    FAssetEditorManager *OwnerManager = nullptr;

    // 열린 에셋 편집기들을 탭/도킹 패널 단위로 관리한다.
    FAssetEditorTabManager TabManager;

    bool bOpen = false;
    bool bVisible = false;
    bool bCapturingInput = false;
};
