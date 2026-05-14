#pragma once

#include "Common/Viewport/EditorViewportClient.h"
#include "Common/Viewport/EditorViewportInputController.h"
#include "Common/Viewport/EditorViewportCamera.h"
#include "Core/CollisionTypes.h"
#include "Core/CoreTypes.h"
#include "Core/RayTypes.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputAction.h"
#include "Input/InputMappingContext.h"
#include "Math/Rotator.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"

#include "ImGui/imgui.h"

class UWorld;
class ULightComponentBase;
class AActor;
class FLevelEditorSettings;
class FWindowsWindow;
class FSelectionManager;
class USceneComponent;
class FOverlayStatSystem;
class FScene;

// Level Editor ?꾩슜 Viewport Client.
// 移대찓??議곗옉, Actor picking, Gizmo 議곗옉, View mode, PIE shortcut ?깆쓣 ?대떦?쒕떎.
class FLevelEditorViewportClient : public FEditorViewportClient
{
  public:
    FLevelEditorViewportClient();
    ~FLevelEditorViewportClient() override;

    void Init(FWindowsWindow *InWindow) override;
    void Shutdown() override;
    void ActivateEditorContext() override;
    void DeactivateEditorContext() override;
    void Tick(float DeltaTime) override;

    // World/FScene 교체 전에 viewport가 들고 있는 editor-only scene resource를 먼저 해제한다.
    void DetachSceneResourcesForWorldChange();

    UWorld *GetWorld() const;

    void             SetOverlayStatSystem(FOverlayStatSystem *InOverlayStatSystem) { OverlayStatSystem = InOverlayStatSystem; }
    void             SetSettings(const FLevelEditorSettings *InSettings) { Settings = InSettings; }
    void             SetSelectionManager(FSelectionManager *InSelectionManager) { SelectionManager = InSelectionManager; }

    FViewportRenderOptions       &GetRenderOptions() { return RenderOptions; }
    const FViewportRenderOptions &GetRenderOptions() const { return RenderOptions; }

    void SetViewportType(ELevelViewportType NewType);
    void SetViewportSize(float InWidth, float InHeight);

    void              InitializeCameraState();
    void              ReleaseCameraState();
    void              ResetCamera();
    FEditorViewportCamera *GetCamera() { return FEditorViewportClient::GetCamera(); }
    const FEditorViewportCamera *GetCamera() const { return FEditorViewportClient::GetCamera(); }
    bool              FocusActor(AActor *Actor);

    void                 SetLightViewOverride(ULightComponentBase *Light);
    void                 ClearLightViewOverride();
    bool                 IsViewingFromLight() const { return LightViewOverride != nullptr; }
    ULightComponentBase *GetLightViewOverride() const { return LightViewOverride; }

    int32 GetPointLightFaceIndex() const { return PointLightFaceIndex; }
    void  SetPointLightFaceIndex(int32 Index) { PointLightFaceIndex = (Index < 0) ? 0 : (Index > 5) ? 5 : Index; }

    void UpdateLayoutRect() override;
    void RenderViewportImage(bool bIsActiveViewport) override;
    const char *GetViewportTooltipBarText() const override;
    bool BuildRenderRequest(FEditorViewportRenderRequest &OutRequest) override;

  private:
    void SetupInput();

    void OnEditorMove(const FInputActionValue &Value);
    void OnEditorRotate(const FInputActionValue &Value);
    void OnEditorPan(const FInputActionValue &Value);
    void OnEditorZoom(const FInputActionValue &Value);
    void OnEditorOrbit(const FInputActionValue &Value);

    void OnEditorFocus(const FInputActionValue &Value);
    void OnEditorDelete(const FInputActionValue &Value);
    void OnEditorDuplicate(const FInputActionValue &Value);
    void OnEditorToggleGizmoMode(const FInputActionValue &Value);
    void OnEditorToggleCoordSystem(const FInputActionValue &Value);
    void OnEditorEscape(const FInputActionValue &Value);
    void OnEditorTogglePIE(const FInputActionValue &Value);

    void  EnsureEditorGizmo();
    void  ReleaseEditorGizmo();
    void  RegisterGizmoToScene(FScene* Scene);
    void  UnregisterGizmoFromScene();
    void  TickEditorShortcuts();
    void  TickInput(float DeltaTime);
    void  TickInteraction(float DeltaTime);
    void  SyncGizmoTargetFromSelection();
    void  HandleDragStart(const FRay &Ray, const FGizmoHitProxyResult& GizmoHitResult, bool bHasGizmoHit);
    void  DrawUIScreenTranslateGizmo();
    bool  HasUIScreenTranslateGizmo() const;
    int32 HitTestUIScreenTranslateGizmo(const ImVec2 &MousePos) const;
    bool  BeginUIScreenTranslateDrag(const ImVec2 &MousePos);
    void  UpdateUIScreenTranslateDrag(const ImVec2 &MousePos);
    void  EndUIScreenTranslateDrag(bool bCommitChange);
    void  SyncCameraSmoothingTarget();
    void  ApplySmoothedCameraLocation(float DeltaTime);

  private:
    FOverlayStatSystem    *OverlayStatSystem = nullptr;
    FScene                *RegisteredGizmoScene = nullptr;
    const FLevelEditorSettings *Settings = nullptr;
    FSelectionManager     *SelectionManager = nullptr;
    USceneComponent        *CurrentGizmoTargetComponent = nullptr;
    FViewportRenderOptions RenderOptions;
    ULightComponentBase   *LightViewOverride = nullptr;
    int32                  PointLightFaceIndex = 0;

    bool    bIsMarqueeSelecting = false;
    bool    bNeedsDeferredTargetSync = false;
    FVector MarqueeStartPos;
    FVector MarqueeCurrentPos;

    const float FocusAnimDuration = 0.5f;
    const float SmoothLocationSpeed = 10.0f;

    FEnhancedInputManager EnhancedInputManager;
    FInputMappingContext *EditorMappingContext = nullptr;

    FEditorViewportInputController CommonInput;

    // Level Editor only shortcuts. Common camera/gizmo/snap actions live in CommonInput.
    FInputAction *ActionEditorDelete = nullptr;
    FInputAction *ActionEditorDuplicate = nullptr;
    FInputAction *ActionEditorTogglePIE = nullptr;
    FInputAction *ActionEditorVertexSnap = nullptr;
    FInputAction *ActionEditorSnapToFloor = nullptr;
    FInputAction *ActionEditorSetBookmark = nullptr;
    FInputAction *ActionEditorJumpToBookmark = nullptr;
    FInputAction *ActionEditorSetViewportPerspective = nullptr;
    FInputAction *ActionEditorSetViewportTop = nullptr;
    FInputAction *ActionEditorSetViewportFront = nullptr;
    FInputAction *ActionEditorSetViewportRight = nullptr;

};
