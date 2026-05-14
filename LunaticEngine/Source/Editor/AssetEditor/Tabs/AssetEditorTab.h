#pragma once

#include "AssetEditor/IAssetEditor.h"
#include "Common/UI/Panels/Panel.h"

#include <filesystem>
#include <memory>
#include <string>

/**
 * Asset Editor 안에서 열린 문서 하나를 나타내는 래퍼.
 *
 * 현재는 별도 Asset Editor Window를 쓰지 않고 Level Editor DockSpace 안에 패널을 붙이지만,
 * 개념적으로는 여전히 "열린 에셋 문서 하나"를 의미한다.
 *
 * 역할:
 * - IAssetEditor 인스턴스 하나를 소유한다.
 * - 탭 제목 / 에셋 경로 / 렌더링을 에디터에 위임한다.
 */
class FAssetEditorTab
{
  public:
    explicit FAssetEditorTab(std::unique_ptr<IAssetEditor> InEditor);

    IAssetEditor *GetEditor() const;
    std::string GetTitle() const;
    const std::filesystem::path &GetAssetPath() const;
    const std::string &GetLayoutId() const { return LayoutId; }
    FDockPanelLayoutState &GetLayoutState() { return LayoutState; }
    const FDockPanelLayoutState &GetLayoutState() const { return LayoutState; }
    void RequestDefaultLayout();
    void RequestRestoreCapturedLayout();
    void MarkDefaultLayoutBuilt();

    void Tick(float DeltaTime);
    void Render(float DeltaTime);

  private:
    std::unique_ptr<IAssetEditor> Editor;
    std::string LayoutId;
    FDockPanelLayoutState LayoutState;
};
