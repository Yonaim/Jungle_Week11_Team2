#pragma once

#include "AssetEditor/Tabs/AssetEditorTab.h"
#include "Core/CoreTypes.h"
#include "ImGui/imgui.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class IAssetEditor;
class FEditorViewportClient;

namespace FEditorDocumentTabBar { struct FTabDesc; }

/**
 * 열린 Asset Editor 문서들을 관리하는 관리자.
 *
 * 하나의 Asset Editor Window 안에서 여러 에셋 문서 탭을 관리한다.
 * 탭 UI는 Main Menu Bar 바로 아래의 별도 Document Tab Bar로 그리되,
 * 실제 에디터 패널은 항상 ActiveTab 하나만 렌더링한다.
 *
 * 역할:
 * - 열린 에디터 목록 관리
 * - 같은 에셋 중복 열기 방지 및 기존 탭 활성화
 * - 활성 탭 저장/닫기
 * - title-bar document tab 렌더링
 * - 활성 에디터 패널 렌더링
 */
class FAssetEditorTabManager
{
  public:
    bool OpenTab(std::unique_ptr<IAssetEditor> Editor);
    bool ActivateTabByAssetPath(const std::filesystem::path &AssetPath);

    bool CloseTab(int32 TabIndex, bool bPromptForDirty = true);
    bool CloseActiveTab(bool bPromptForDirty = true);
    bool CloseAllTabs(bool bPromptForDirty = true);
    bool ConfirmCloseAllTabs() const;
    bool HasDirtyTabs() const;
    void SetClosePromptOwnerWindowHandle(void *OwnerWindowHandle);
    bool SaveActiveTab();

    void SetEditorContextActive(bool bActive);
    void ActivateActiveTabContext();
    void DeactivateActiveTabContext();
    void ActivateActiveTab() { ActivateActiveTabContext(); }
    void DeactivateActiveTab() { DeactivateActiveTabContext(); }

    void Tick(float DeltaTime);
    void Render(float DeltaTime, ImGuiID DockspaceId = 0);
    void RequestDefaultLayoutForActiveTab();
    void InvalidateEditorLayouts();

    /** Unified main-frame document tab bar support. */
    void AppendDocumentTabDescs(std::vector<FEditorDocumentTabBar::FTabDesc> &OutTabs) const;
    bool SetActiveTabIndex(int32 NewIndex);
    int32 GetActiveTabIndex() const { return ActiveTabIndex; }
    const std::string &GetActiveLayoutId() const;
    void CollectLayoutIds(std::vector<std::string> &OutLayoutIds) const;
    void RenderInactiveDockKeepAliveWindows(const std::string &ActiveLayoutId);
    FDockPanelLayoutState *GetActiveLayoutState();
    void RequestRestoreForActiveTab();

    /** 현재 ImGui window 안에 Unreal식 asset document tab bar를 그린다. Legacy fallback only. */
    bool RenderDocumentTabBar();

    bool HasOpenTabs() const;
    int32 GetTabCount() const;
    bool IsCapturingInput() const;
    IAssetEditor *GetActiveEditor() const;
    FEditorViewportClient *GetActiveViewportClient() const;
    void CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) const;
    void ForceDeactivateAllViewportClients();

  private:
    FAssetEditorTab *GetActiveTab() const;
    void RenderActiveTab(float DeltaTime, ImGuiID DockspaceId);
    void CompactInvalidTabs();
    bool ConfirmCloseTab(FAssetEditorTab *Tab);

  private:
    TArray<std::unique_ptr<FAssetEditorTab>> Tabs;
    int32 ActiveTabIndex = -1;
    bool bEditorContextActive = false;
    void *ClosePromptOwnerWindowHandle = nullptr;
};
