#pragma once
#include "ContentBrowserContext.h"
#include "ContentBrowserElement.h"
#include "ContentItem.h"
#include "Common/UI/Base/UIElement.h"
#include <d3d11.h>
#include <memory>
#include <wrl/client.h>


class FContentBrowser final : public FUIElement
{
    struct FDirNode
    {
        FContentItem Self;
        TArray<FDirNode> Children;
    };

  public:
    void Init(UEditorEngine *InEditor, ID3D11Device *InDevice);
    void Render(float DeltaTime) override;
    void Refresh();
    void RevealAndSelect(const std::filesystem::path& Path);
    void SaveToSettings() const;
    void SetIconSize(float Size);
    float GetIconSize() const
    {
        return BrowserContext.ContentSize.x;
    }

  private:
    void LoadFromSettings();
    void RefreshContent();
    void DrawDirNode(FDirNode InNode);
    void DrawContents();
    void ProcessPendingActions();

    TArray<FContentItem> ReadDirectory(std::wstring Path);
    FDirNode BuildDirectoryTree(const std::filesystem::path &DirPath);

  private:
    ContentBrowserContext BrowserContext;

    FDirNode RootNode;
    TArray<std::shared_ptr<ContentBrowserElement>> CachedBrowserElements;
    TMap<FString, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> ICons;
};
