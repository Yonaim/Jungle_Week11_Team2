#pragma once

#include "Viewport/ViewportClient.h"
#include "UI/SWindow.h"
#include "Runtime/WindowsWindow.h"
#include "Render/Types/ViewTypes.h"
#include "Camera/MinimalViewInfo.h"
#include "Common/Viewport/EditorViewportCamera.h"
#include "Common/Viewport/ViewportCameraController.h"
#include "Common/Gizmo/GizmoManager.h"

class FWindowsWindow;
class FViewport;
class FScene;
class FEditorViewportClient;
class UWorld;
class FLevelEditorViewportClient;

/**
 * 모든 Editor Viewport Client의 공통 베이스.
 *
 * 역할:
 * - FViewport 포인터와 ImGui/SWindow layout 정보를 보관한다.
 * - Viewport screen rect, active/hovered 상태, viewport image 렌더링 같은 공통 기능을 제공한다.
 *
 * 주의:
 * - 이 클래스는 Level Editor 전용 기능을 몰라야 한다.
 * - ViewCamera와 선택적 GizmoManager는 공통으로 가진다.
 * - Selection policy, picking, camera navigation, PIE shortcut은 파생 ViewportClient 쪽에 둔다.
 * - SkeletalMeshPreviewViewportClient 같은 Asset Preview 뷰포트도 이 베이스를 상속한다.
 */

/**
 * Editor viewport를 Renderer에 넘기기 위한 공통 렌더 요청.
 *
 * Level Editor와 Asset Preview Editor는 같은 FViewport/Renderer 경로를 사용하지만,
 * 렌더 대상 Scene과 전용 기능(Actor Picking, Bone Gizmo 등)은 서로 다르다.
 * 그래서 RenderPipeline은 구체 ViewportClient 타입 대신 이 요청 구조체만 읽는다.
 */
struct FEditorViewportRenderRequest
{
    FViewport *Viewport = nullptr;
    // 에디터 뷰포트는 에디터 소유 UCameraComponent가 아니라 순수 POV를 넘겨야 한다.
    FMinimalViewInfo ViewInfo;
    FScene *Scene = nullptr;
    FViewportRenderOptions RenderOptions;
    FEditorViewportClient *CursorProvider = nullptr;
    UWorld *World = nullptr;
    FLevelEditorViewportClient *LevelViewportClient = nullptr;

    bool bRenderGrid = true;
    bool bEnableGPUOcclusion = false;
    bool bAllowLevelDebugVisuals = false;
};

class FEditorViewportClient : public FViewportClient
{
  public:
    FEditorViewportClient() = default;
    ~FEditorViewportClient() override = default;

    virtual void Init(FWindowsWindow *InWindow);
    virtual void Shutdown();
    virtual void Tick(float DeltaTime);

    void SetViewport(FViewport *InViewport) { Viewport = InViewport; }
    FViewport *GetViewport() const { return Viewport; }

    void SetLayoutWindow(SWindow *InWindow) { LayoutWindow = InWindow; }
    SWindow *GetLayoutWindow() const { return LayoutWindow; }

    void SetActive(bool bInActive) { bIsActive = bInActive; }
    bool IsActive() const { return bIsActive; }

    void SetHovered(bool bInHovered) { bIsHovered = bInHovered; }
    bool IsHovered() const { return bIsHovered; }

    // Owner-context lifecycle.
    // Document tabs / editor contexts should activate exactly the viewport clients they own
    // and deactivate all hidden clients through this common API.
    virtual void ActivateEditorContext();
    virtual void DeactivateEditorContext();
    bool IsEditorContextActive() const { return bEditorContextActive; }
    const bool* GetEditorContextActiveFlag() const { return &bEditorContextActive; }
    uint64 GetEditorContextEpoch() const { return EditorContextEpoch; }

    // Stronger than bEditorContextActive. The first Level tab and first Asset tab can
    // both have been activated once during workspace bootstrapping. Before accepting
    // input/gizmo deltas, verify that the engine currently considers this viewport
    // the single live owner.
    bool IsLiveContextOwner() const;
    bool CanProcessLiveContextWork() const;

    void SetViewportSize(float InWidth, float InHeight);
    void SetViewportScreenRect(const FRect &InRect);

    const FRect &GetViewportScreenRect() const { return ViewportScreenRect; }

    virtual void UpdateLayoutRect();
    virtual void RenderViewportImage(bool bIsActiveViewport);
    virtual void NotifyViewportPresented() {}
    virtual const char *GetViewportTooltipBarText() const { return nullptr; }

    /**
     * 실제 렌더 대상이 있는 ViewportClient만 true를 반환한다.
     * 기본 EditorViewportClient는 UI 표시용 공통 기능만 제공하므로 렌더 요청을 만들지 않는다.
     */
    virtual bool BuildRenderRequest(FEditorViewportRenderRequest &OutRequest) { (void)OutRequest; return false; }

    FEditorViewportCamera *GetCamera() { return &ViewCamera; }
    const FEditorViewportCamera *GetCamera() const { return &ViewCamera; }
    FViewportCameraController &GetCameraController() { return CameraController; }
    const FViewportCameraController &GetCameraController() const { return CameraController; }
    FGizmoManager &GetGizmoManager() { return GizmoManager; }
    const FGizmoManager &GetGizmoManager() const { return GizmoManager; }

    bool GetCursorViewportPosition(uint32 &OutX, uint32 &OutY) const;

  protected:
    void RenderViewportTooltipBar() const;

    FWindowsWindow *Window = nullptr;
    FViewport *Viewport = nullptr;
    SWindow *LayoutWindow = nullptr;

    float WindowWidth = 1920.0f;
    float WindowHeight = 1080.0f;

    bool bIsActive = false;
    bool bIsHovered = false;
    bool bEditorContextActive = false;
    uint64 EditorContextEpoch = 0;

    FRect ViewportScreenRect;

    // 각 ViewportClient가 독립 소유하는 에디터 전용 카메라/카메라 컨트롤러/기즈모 상태.
    FEditorViewportCamera ViewCamera;
    FViewportCameraController CameraController;
    FGizmoManager GizmoManager;
};
