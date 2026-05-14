#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"
#include "AssetEditor/SkeletalMesh/Viewport/SkeletalMeshPreviewViewportClient.h"
#include "Common/UI/Panels/Panel.h"
#include "Viewport/Viewport.h"
#include "UI/SWindow.h"

#include <memory>

#include "ImGui/imgui.h"

class FRenderer;
class FWindowsWindow;
class USkeletalMesh;
class FSkeletalMeshEditorToolbar;
class FSkeletalMeshSelectionManager;

/**
 * Skeletal Mesh Editor의 Preview Viewport 패널.
 *
 * 역할:
 * - ImGui 패널 영역을 만든다.
 * - Preview 영역의 screen rect를 계산한다.
 * - 실제 viewport 상태/입력/렌더 표시를 FSkeletalMeshPreviewViewportClient에 위임한다.
 *
 * 주의:
 * - 여기서는 실제 CPU Skinning / SkeletalMeshComponent 렌더링을 하지 않는다.
 * - 남윤지 담당 Runtime/CPU Skinning이 준비되면 PreviewViewportClient에 연결한다.
 */
class FSkeletalMeshPreviewViewport
{
  public:
    ~FSkeletalMeshPreviewViewport();

    void Initialize(FWindowsWindow *InWindow, FRenderer *InRenderer);
    void Shutdown();

    void BindEditorContext(FSkeletalMeshEditorState& State, FSkeletalMeshSelectionManager* SelectionManager);
    void ActivateEditorContext();
    void DeactivateEditorContext();

    void Tick(float DeltaTime);
    void Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, FSkeletalMeshSelectionManager *SelectionManager,
                FSkeletalMeshEditorToolbar *Toolbar, float DeltaTime, const FPanelDesc &PanelDesc);

    FSkeletalMeshPreviewViewportClient *GetViewportClient() { return PreviewViewportClient.get(); }

  private:
    void EnsureViewportResources();
    void RenderViewportPanel(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, FSkeletalMeshSelectionManager *SelectionManager,
                             FSkeletalMeshEditorToolbar *Toolbar, float DeltaTime);

  private:
    FWindowsWindow *Window = nullptr;
    FRenderer *Renderer = nullptr;

    std::unique_ptr<FSkeletalMeshPreviewViewportClient> PreviewViewportClient;
    std::unique_ptr<FViewport> PreviewViewport;
    std::unique_ptr<SWindow> PreviewLayoutWindow;
};
