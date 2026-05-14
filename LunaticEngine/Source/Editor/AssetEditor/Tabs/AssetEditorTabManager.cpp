#include "PCH/LunaticPCH.h"
#include "AssetEditor/Tabs/AssetEditorTabManager.h"
#include "Common/UI/Panels/Panel.h"
#include "Common/UI/Tabs/EditorDocumentTabBar.h"
#include "Common/Viewport/EditorViewportClient.h"
#include "Common/Gizmo/GizmoManager.h"

#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/Tabs/AssetEditorTab.h"

#include "ImGui/imgui.h"
#include "Platform/Paths.h"

#include <algorithm>
#include <string>
#include <vector>
#include <windows.h>

namespace
{
    std::filesystem::path NormalizeAssetPath(const std::filesystem::path &Path)
    {
        return Path.empty() ? Path : Path.lexically_normal();
    }

    std::wstring MakeAssetClosePromptMessage(const FAssetEditorTab *Tab)
    {
        std::wstring AssetName = L"This asset file";
        if (Tab)
        {
            const std::filesystem::path &AssetPath = Tab->GetAssetPath();
            if (!AssetPath.empty())
            {
                AssetName = L"\"" + AssetPath.filename().wstring() + L"\"";
            }
        }

        return AssetName +
               L" has unsaved changes.\n\n"
               L"Do you want to save your changes before closing this editor?\n\n"
               L"Yes: Save and close\n"
               L"No: Close without saving\n"
               L"Cancel: Keep editing";
    }

    std::wstring MakeAssetCloseAllPromptMessage()
    {
        return L"One or more asset editor files have unsaved changes.\n\n"
               L"Do you want to save your changes before closing the Asset Editor workspace?\n\n"
               L"Yes: Save all and close\n"
               L"No: Close without saving\n"
               L"Cancel: Keep editing";
    }
    void DeactivateAssetEditorViewport(IAssetEditor* Editor)
    {
        if (!Editor)
        {
            return;
        }

        // Deactivate every viewport owned by this editor, not just the active one.
        // An asset editor can have preview/detail viewports later, and hidden clients
        // must not keep live gizmo/input targets.
        TArray<FEditorViewportClient*> ViewportClients;
        Editor->CollectViewportClients(ViewportClients);
        for (FEditorViewportClient* ViewportClient : ViewportClients)
        {
            if (ViewportClient)
            {
                ViewportClient->DeactivateEditorContext();
            }
        }

        if (FEditorViewportClient* ViewportClient = Editor->GetActiveViewportClient())
        {
            if (std::find(ViewportClients.begin(), ViewportClients.end(), ViewportClient) == ViewportClients.end())
            {
                ViewportClient->DeactivateEditorContext();
            }
        }

        Editor->OnDeactivated();
    }

    void ActivateAssetEditorViewport(IAssetEditor* Editor)
    {
        if (!Editor)
        {
            return;
        }

        Editor->OnActivated();
        if (FEditorViewportClient* ViewportClient = Editor->GetActiveViewportClient())
        {
            ViewportClient->ActivateEditorContext();
        }
    }

}

bool FAssetEditorTabManager::OpenTab(std::unique_ptr<IAssetEditor> Editor)
{
    if (!Editor)
    {
        return false;
    }

    const std::filesystem::path NewAssetPath = NormalizeAssetPath(Editor->GetAssetPath());
    if (!NewAssetPath.empty() && ActivateTabByAssetPath(NewAssetPath))
    {
        return true;
    }

    Tabs.push_back(std::make_unique<FAssetEditorTab>(std::move(Editor)));
    SetActiveTabIndex(static_cast<int32>(Tabs.size()) - 1);
    return true;
}

bool FAssetEditorTabManager::ActivateTabByAssetPath(const std::filesystem::path &AssetPath)
{
    const std::filesystem::path NormalizedPath = NormalizeAssetPath(AssetPath);
    for (int32 Index = 0; Index < static_cast<int32>(Tabs.size()); ++Index)
    {
        if (!Tabs[Index])
        {
            continue;
        }

        if (NormalizeAssetPath(Tabs[Index]->GetAssetPath()) == NormalizedPath)
        {
            SetActiveTabIndex(Index);
            return true;
        }
    }

    return false;
}

bool FAssetEditorTabManager::CloseTab(int32 TabIndex, bool bPromptForDirty)
{
    if (TabIndex < 0 || TabIndex >= static_cast<int32>(Tabs.size()))
    {
        return false;
    }

    FAssetEditorTab *Tab = Tabs[TabIndex].get();
    if (bPromptForDirty && !ConfirmCloseTab(Tab))
    {
        return false;
    }

    const bool bClosingActiveTab = ActiveTabIndex == TabIndex;
    if (bClosingActiveTab && Tab)
    {
        DeactivateAssetEditorViewport(Tab->GetEditor());
    }

    if (Tab && Tab->GetEditor())
    {
        Tab->GetEditor()->Close();
    }

    Tabs.erase(Tabs.begin() + TabIndex);

    if (Tabs.empty())
    {
        ActiveTabIndex = -1;
        return true;
    }

    if (bClosingActiveTab)
    {
        ActiveTabIndex = -1;
        SetActiveTabIndex((std::min)(TabIndex, static_cast<int32>(Tabs.size()) - 1));
    }
    else if (TabIndex < ActiveTabIndex)
    {
        ActiveTabIndex = (std::max)(0, ActiveTabIndex - 1);
    }
    else if (ActiveTabIndex >= static_cast<int32>(Tabs.size()))
    {
        ActiveTabIndex = -1;
        SetActiveTabIndex(static_cast<int32>(Tabs.size()) - 1);
    }

    return true;
}

bool FAssetEditorTabManager::CloseActiveTab(bool bPromptForDirty)
{
    return CloseTab(ActiveTabIndex, bPromptForDirty);
}

bool FAssetEditorTabManager::CloseAllTabs(bool bPromptForDirty)
{
    if (bPromptForDirty && !ConfirmCloseAllTabs())
    {
        return false;
    }

    while (!Tabs.empty())
    {
        CloseTab(static_cast<int32>(Tabs.size()) - 1, false);
    }
    return true;
}

bool FAssetEditorTabManager::ConfirmCloseAllTabs() const
{
    bool bHasDirtyTab = false;
    for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
    {
        if (Tab && Tab->GetEditor() && Tab->GetEditor()->IsDirty())
        {
            bHasDirtyTab = true;
            break;
        }
    }

    if (!bHasDirtyTab)
    {
        return true;
    }

    const int32 Result = MessageBoxW(static_cast<HWND>(ClosePromptOwnerWindowHandle),
                                    MakeAssetCloseAllPromptMessage().c_str(),
                                    L"Unsaved Asset Editor Files",
                                    MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON3);
    if (Result == IDCANCEL)
    {
        return false;
    }
    if (Result == IDNO)
    {
        return true;
    }
    if (Result == IDYES)
    {
        for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
        {
            if (Tab && Tab->GetEditor() && Tab->GetEditor()->IsDirty())
            {
                if (!Tab->GetEditor()->Save())
                {
                    MessageBoxW(static_cast<HWND>(ClosePromptOwnerWindowHandle),
                                L"Failed to save one or more asset editor files. The workspace will remain open.",
                                L"Save Failed",
                                MB_OK | MB_ICONERROR);
                    return false;
                }
            }
        }
        return true;
    }

    return false;
}

bool FAssetEditorTabManager::HasDirtyTabs() const
{
    for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
    {
        if (Tab && Tab->GetEditor() && Tab->GetEditor()->IsDirty())
        {
            return true;
        }
    }
    return false;
}

void FAssetEditorTabManager::SetClosePromptOwnerWindowHandle(void *OwnerWindowHandle)
{
    ClosePromptOwnerWindowHandle = OwnerWindowHandle;
}

bool FAssetEditorTabManager::SaveActiveTab()
{
    IAssetEditor *Editor = GetActiveEditor();
    return Editor ? Editor->Save() : false;
}

void FAssetEditorTabManager::SetEditorContextActive(bool bActive)
{
    if (bEditorContextActive == bActive)
    {
        // Same-context re-entry is not a no-op in a multi document workspace.
        // It is used as a hard ownership refresh after Level/Asset tab switches.
        if (bActive)
        {
            ActivateActiveTabContext();
        }
        else
        {
            DeactivateActiveTabContext();
        }
        return;
    }

    bEditorContextActive = bActive;
    if (bEditorContextActive)
    {
        ActivateActiveTabContext();
    }
    else
    {
        DeactivateActiveTabContext();
    }
}

void FAssetEditorTabManager::ActivateActiveTabContext()
{
    bEditorContextActive = true;
    if (FAssetEditorTab* ActiveTab = GetActiveTab())
    {
        // Never surface a stale or broken captured dock layout when users return to this workspace.
        // Rebuild the editor's canonical layout before the tab becomes visible again.
        ActiveTab->RequestDefaultLayout();
        ActivateAssetEditorViewport(ActiveTab->GetEditor());
    }
}

void FAssetEditorTabManager::DeactivateActiveTabContext()
{
    if (FAssetEditorTab* ActiveTab = GetActiveTab())
    {
        DeactivateAssetEditorViewport(ActiveTab->GetEditor());
    }
    bEditorContextActive = false;
}

void FAssetEditorTabManager::Tick(float DeltaTime)
{
    // 화면에 보이는 ActiveTab 하나만 tick한다.
    // 비활성/hidden Asset Editor 탭까지 tick하면 preview scene, skeletal skinning, gizmo/debug draw가
    // 백그라운드에서 계속 돌아 Level Editor 복귀 후 프레임 저하가 발생할 수 있다.
    FAssetEditorTab *ActiveTab = GetActiveTab();
    if (ActiveTab)
    {
        ActiveTab->Tick(DeltaTime);
    }
}

void FAssetEditorTabManager::RequestDefaultLayoutForActiveTab()
{
    if (FAssetEditorTab *Tab = GetActiveTab())
    {
        Tab->RequestDefaultLayout();
    }
}

void FAssetEditorTabManager::InvalidateEditorLayouts()
{
    // Legacy name kept for existing menu code. Reset only the active tab.
    RequestDefaultLayoutForActiveTab();
}

void FAssetEditorTabManager::AppendDocumentTabDescs(std::vector<FEditorDocumentTabBar::FTabDesc> &OutTabs) const
{
    for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
    {
        if (!Tab || !Tab->GetEditor())
        {
            continue;
        }

        FEditorDocumentTabBar::FTabDesc Desc;
        Desc.Label = Tab->GetTitle();
        Desc.Tooltip = FPaths::ToUtf8(Tab->GetAssetPath().wstring());
        Desc.bDirty = Tab->GetEditor()->IsDirty();
        Desc.IconKey = Tab->GetEditor()->GetDocumentTabIconKey();
        Desc.IconTint = Tab->GetEditor()->GetDocumentTabIconTint();
        OutTabs.push_back(std::move(Desc));
    }
}

const std::string &FAssetEditorTabManager::GetActiveLayoutId() const
{
    static const std::string EmptyLayoutId;
    FAssetEditorTab *Tab = GetActiveTab();
    return Tab ? Tab->GetLayoutId() : EmptyLayoutId;
}

void FAssetEditorTabManager::CollectLayoutIds(std::vector<std::string> &OutLayoutIds) const
{
    for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
    {
        if (Tab)
        {
            OutLayoutIds.push_back(Tab->GetLayoutId());
        }
    }
}

void FAssetEditorTabManager::RenderInactiveDockKeepAliveWindows(const std::string &ActiveLayoutId)
{
    for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
    {
        if (!Tab || Tab->GetLayoutId() == ActiveLayoutId)
        {
            continue;
        }

        FPanel::RenderDockKeepAliveWindows(&Tab->GetLayoutState());
    }
}

FDockPanelLayoutState *FAssetEditorTabManager::GetActiveLayoutState()
{
    FAssetEditorTab *Tab = GetActiveTab();
    return Tab ? &Tab->GetLayoutState() : nullptr;
}

void FAssetEditorTabManager::RequestRestoreForActiveTab()
{
    if (FAssetEditorTab *Tab = GetActiveTab())
    {
        Tab->RequestRestoreCapturedLayout();
    }
}

bool FAssetEditorTabManager::RenderDocumentTabBar()
{
    CompactInvalidTabs();
    if (Tabs.empty())
    {
        return false;
    }

    std::vector<FEditorDocumentTabBar::FTabDesc> TabDescs;
    TabDescs.reserve(Tabs.size());
    for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
    {
        if (!Tab || !Tab->GetEditor())
        {
            continue;
        }

        FEditorDocumentTabBar::FTabDesc Desc;
        Desc.Label = Tab->GetTitle();
        Desc.Tooltip = FPaths::ToUtf8(Tab->GetAssetPath().wstring());
        Desc.bDirty = Tab->GetEditor()->IsDirty();
        Desc.IconKey = Tab->GetEditor()->GetDocumentTabIconKey();
        Desc.IconTint = Tab->GetEditor()->GetDocumentTabIconTint();
        TabDescs.push_back(std::move(Desc));
    }

    FEditorDocumentTabBar::FRenderResult Result =
        FEditorDocumentTabBar::Render("AssetEditorDocumentTabBar", TabDescs, ActiveTabIndex);

    if (Result.SelectedIndex != ActiveTabIndex)
    {
        SetActiveTabIndex(Result.SelectedIndex);
    }

    if (Result.CloseRequestedIndex >= 0)
    {
        CloseTab(Result.CloseRequestedIndex);
    }

    return true;
}

void FAssetEditorTabManager::Render(float DeltaTime, ImGuiID DockspaceId)
{
    CompactInvalidTabs();
    RenderActiveTab(DeltaTime, DockspaceId);
}

void FAssetEditorTabManager::RenderActiveTab(float DeltaTime, ImGuiID DockspaceId)
{
    FAssetEditorTab *Tab = GetActiveTab();
    if (!Tab || !Tab->GetEditor())
    {
        return;
    }

    IAssetEditor *Editor = Tab->GetEditor();
    FDockPanelLayoutState &LayoutState = Tab->GetLayoutState();
    FPanel::SetCurrentStableIdPrefix(Tab->GetLayoutId());
    FPanel::SetCurrentDockspaceId(DockspaceId);
    FPanel::SetCurrentLayoutState(&LayoutState);

    if (Editor->UsesExternalPanels())
    {
        // SkeletalMeshEditor처럼 에디터 내부 영역이 실제 패널 단위로 나뉘는 경우.
        // 다중 문서 탭 구조에서는 반드시 ActiveTab 하나만 외부 패널을 렌더링해야 한다.
        Editor->RenderPanels(DeltaTime, DockspaceId);
        FPanel::ConsumeCapturedLayoutRestoreFrame(&LayoutState);
        FPanel::ClearCurrentLayoutState();
        FPanel::ClearCurrentDockspaceId();
        FPanel::ClearCurrentStableIdPrefix();
        return;
    }

    bool bOpen = true;
    const std::string StableId = std::string("AssetEditorTab_") + std::to_string(ActiveTabIndex);
    const std::string TabTitle = Tab->GetTitle();
    FPanelDesc PanelDesc;
    PanelDesc.DisplayName = TabTitle.c_str();
    PanelDesc.StableId = StableId.c_str();
    PanelDesc.IconKey = "Editor.Icon.Panel.Asset";
    PanelDesc.DockspaceId = DockspaceId;
    PanelDesc.bClosable = true;
    PanelDesc.bOpen = &bOpen;
    PanelDesc.WindowFlags = ImGuiWindowFlags_NoCollapse;

    if (FPanel::Begin(PanelDesc))
    {
        Tab->Render(DeltaTime);
    }
    FPanel::End();

    FPanel::ConsumeCapturedLayoutRestoreFrame(&LayoutState);
    FPanel::ClearCurrentLayoutState();
    FPanel::ClearCurrentDockspaceId();
    FPanel::ClearCurrentStableIdPrefix();

    if (!bOpen)
    {
        CloseActiveTab();
    }
}

void FAssetEditorTabManager::CompactInvalidTabs()
{
    for (int32 Index = 0; Index < static_cast<int32>(Tabs.size());)
    {
        if (!Tabs[Index] || !Tabs[Index]->GetEditor())
        {
            CloseTab(Index, false);
            continue;
        }
        ++Index;
    }
}

bool FAssetEditorTabManager::ConfirmCloseTab(FAssetEditorTab *Tab)
{
    if (!Tab || !Tab->GetEditor() || !Tab->GetEditor()->IsDirty())
    {
        return true;
    }

    const std::wstring Message = MakeAssetClosePromptMessage(Tab);
    const int32 Result = MessageBoxW(static_cast<HWND>(ClosePromptOwnerWindowHandle),
                                    Message.c_str(),
                                    L"Unsaved Asset Editor File",
                                    MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON3);
    if (Result == IDCANCEL)
    {
        return false;
    }
    if (Result == IDNO)
    {
        return true;
    }
    if (Result == IDYES)
    {
        if (Tab->GetEditor()->Save())
        {
            return true;
        }

        MessageBoxW(static_cast<HWND>(ClosePromptOwnerWindowHandle),
                    L"Failed to save this asset editor file. The editor will remain open.",
                    L"Save Failed",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    return false;
}

FAssetEditorTab *FAssetEditorTabManager::GetActiveTab() const
{
    if (ActiveTabIndex < 0 || ActiveTabIndex >= static_cast<int32>(Tabs.size()) || !Tabs[ActiveTabIndex])
    {
        return nullptr;
    }
    return Tabs[ActiveTabIndex].get();
}

bool FAssetEditorTabManager::SetActiveTabIndex(int32 NewIndex)
{
    if (NewIndex < 0 || NewIndex >= static_cast<int32>(Tabs.size()))
    {
        return false;
    }

    if (ActiveTabIndex == NewIndex)
    {
        return true;
    }

    if (bEditorContextActive)
    {
        if (FAssetEditorTab *OldTab = GetActiveTab())
        {
            DeactivateAssetEditorViewport(OldTab->GetEditor());
        }
    }

    ActiveTabIndex = NewIndex;
    if (Tabs[ActiveTabIndex] && Tabs[ActiveTabIndex]->GetEditor())
    {
        // Switching back to a document tab should always present the default layout rather than
        // a previously captured, potentially corrupted dock arrangement.
        Tabs[ActiveTabIndex]->RequestDefaultLayout();
        if (bEditorContextActive)
        {
            ActivateAssetEditorViewport(Tabs[ActiveTabIndex]->GetEditor());
        }
    }
    return true;
}

bool FAssetEditorTabManager::HasOpenTabs() const
{
    return !Tabs.empty();
}

int32 FAssetEditorTabManager::GetTabCount() const
{
    return static_cast<int32>(Tabs.size());
}

bool FAssetEditorTabManager::IsCapturingInput() const
{
    if (!bEditorContextActive)
    {
        return false;
    }

    IAssetEditor *Editor = GetActiveEditor();
    return Editor && Editor->IsCapturingInput();
}

IAssetEditor *FAssetEditorTabManager::GetActiveEditor() const
{
    FAssetEditorTab *Tab = GetActiveTab();
    return Tab ? Tab->GetEditor() : nullptr;
}

FEditorViewportClient *FAssetEditorTabManager::GetActiveViewportClient() const
{
    if (!bEditorContextActive)
    {
        return nullptr;
    }

    IAssetEditor *Editor = GetActiveEditor();
    return Editor ? Editor->GetActiveViewportClient() : nullptr;
}

void FAssetEditorTabManager::CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) const
{
    if (!bEditorContextActive)
    {
        return;
    }

    // 비활성 document tab의 preview viewport는 화면에 보이지 않으므로 렌더/입력 대상에서 제외한다.
    IAssetEditor *Editor = GetActiveEditor();
    if (Editor)
    {
        Editor->CollectViewportClients(OutClients);
    }
}

void FAssetEditorTabManager::ForceDeactivateAllViewportClients()
{
    for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
    {
        if (Tab && Tab->GetEditor())
        {
            DeactivateAssetEditorViewport(Tab->GetEditor());
        }
    }
}
