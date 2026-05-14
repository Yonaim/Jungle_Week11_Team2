#include "PCH/LunaticPCH.h"
#include "AssetEditor/Tabs/AssetEditorTab.h"

#include "AssetEditor/IAssetEditor.h"
#include "Platform/Paths.h"

#include <atomic>

namespace
{
    std::atomic<uint32> GNextAssetEditorTabLayoutId{1};
}

FAssetEditorTab::FAssetEditorTab(std::unique_ptr<IAssetEditor> InEditor) : Editor(std::move(InEditor))
{
    LayoutId = std::string("AssetEditorDockSpace_") + std::to_string(GNextAssetEditorTabLayoutId.fetch_add(1));
    LayoutState.bRequestDefaultLayout = true;
}


IAssetEditor *FAssetEditorTab::GetEditor() const { return Editor.get(); }

std::string FAssetEditorTab::GetTitle() const
{
    if (!Editor)
    {
        return "Asset";
    }

    const std::filesystem::path &AssetPath = Editor->GetAssetPath();
    if (!AssetPath.empty())
    {
        return FPaths::ToUtf8(AssetPath.filename().wstring());
    }

    return Editor->GetEditorName();
}

const std::filesystem::path &FAssetEditorTab::GetAssetPath() const
{
    static const std::filesystem::path EmptyPath;
    return Editor ? Editor->GetAssetPath() : EmptyPath;
}

void FAssetEditorTab::Tick(float DeltaTime)
{
    if (Editor)
    {
        Editor->Tick(DeltaTime);
    }
}

void FAssetEditorTab::Render(float DeltaTime)
{
    if (Editor)
    {
        Editor->RenderContent(DeltaTime);
    }
}

void FAssetEditorTab::RequestDefaultLayout()
{
    LayoutState.bRequestDefaultLayout = true;
    LayoutState.bDefaultLayoutBuilt = false;
    FPanel::ClearCapturedLayoutRestore(&LayoutState);
    LayoutState.PanelDockIds.clear();
    if (Editor)
    {
        Editor->InvalidateDockLayout();
    }
}

void FAssetEditorTab::RequestRestoreCapturedLayout()
{
    LayoutState.bRequestDefaultLayout = false;
    if (FPanel::HasCapturedDockLayout(&LayoutState))
    {
        FPanel::RequestCapturedLayoutRestore(&LayoutState);
        return;
    }

    RequestDefaultLayout();
}

void FAssetEditorTab::MarkDefaultLayoutBuilt()
{
    LayoutState.bDefaultLayoutBuilt = true;
    LayoutState.bRequestDefaultLayout = false;
}
