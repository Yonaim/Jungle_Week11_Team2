#pragma once

#include "AssetEditor/Window/AssetEditorWindow.h"
#include "ImGui/imgui.h"

#include <filesystem>
#include <memory>

class UObject;
class UEditorEngine;
class FRenderer;
class IAssetEditor;
class USkeletalMesh;
class FEditorViewportClient;

/**
 * Asset Editor의 진입점이자 라우터.
 *
 * 역할:
 * - Content Browser / 메뉴에서 들어온 파일 열기 요청을 받는다.
 * - Asset Editor는 .uasset만 열고 저장한다. Source 파일 import는 FAssetImportManager가 담당한다.
 * - UObject 타입에 맞는 IAssetEditor 인스턴스를 생성한다.
 * - 생성된 에디터를 AssetEditorWindow(현재는 패널 컨트롤러)에 등록한다.
 *
 * 주의:
 * - SkeletalMeshEditor는 FBX 파일을 직접 파싱하지 않는다. .fbx는 Import 버튼으로 .uasset을 만든 뒤 연다.
 */
class FAssetEditorManager
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer);
    void Shutdown();

    void Tick(float DeltaTime);
    void RenderContent(float DeltaTime, ImGuiID DockspaceId = 0);

    bool OpenAssetFromPath(const std::filesystem::path &AssetPath);
    bool OpenOwnedWorkingCopy(UObject *Asset, const std::filesystem::path &AssetPath);
    bool OpenLoadedAsset(UObject *Asset, const std::filesystem::path &AssetPath) { return OpenOwnedWorkingCopy(Asset, AssetPath); }
    bool OpenAssetWithDialog(void *OwnerWindowHandle = nullptr);

    /**
     * 빈 Asset Editor 패널을 표시한다.
     */
    bool ShowAssetEditorWindow();
    bool CreateCameraModifierStackAsset();

    bool SaveActiveEditor();
    void CloseActiveEditor();
    bool CloseAllEditors(bool bPromptForDirty = true, void *OwnerWindowHandle = nullptr);
    bool HasDirtyEditors() const;
    bool ConfirmCloseAllEditors(void *OwnerWindowHandle = nullptr) const;

    bool IsCapturingInput() const;
    FEditorViewportClient *GetActiveViewportClient() const;
    void CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) const;
    void ForceDeactivateAllViewportClients();

    FAssetEditorWindow &GetAssetEditorWindow() { return AssetEditorWindow; }
    const FAssetEditorWindow &GetAssetEditorWindow() const { return AssetEditorWindow; }

  private:
    std::unique_ptr<IAssetEditor> CreateEditorForAsset(UObject *Asset) const;

  private:
    UEditorEngine *EditorEngine = nullptr;
    FRenderer     *Renderer = nullptr;

    // 이름은 Window지만 현재 구현은 별도 창이 아니라 Level Editor DockSpace 안의 패널 컨트롤러다.
    FAssetEditorWindow AssetEditorWindow;
};
