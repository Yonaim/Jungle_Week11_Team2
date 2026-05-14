#pragma once

#include "PrimitiveComponent.h"
#include "Core/CoreTypes.h"
#include "Math/Rotator.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Render/Types/ViewTypes.h"
#include "Component/Gizmo/GizmoTypes.h"
#include "Component/Gizmo/GizmoHitProxyTypes.h"

class FPrimitiveSceneProxy;
class FScene;

// Transform Gizmo의 시각 표현 전용 컴포넌트.
// 실제 조작 대상, 드래그 계산, transform 적용은 FGizmoManager / ITransformGizmoTarget가 담당한다.
class UGizmoVisualComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UGizmoVisualComponent, UPrimitiveComponent)
    UGizmoVisualComponent();

    // Gizmo picking no longer uses component raycast.
    // Use HitProxyTest() so picking follows the actually rendered handle mesh.
    bool LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult) override;
    bool HitProxyTest(const FGizmoHitProxyContext& Context, FGizmoHitProxyResult& OutResult) const;

    FVector GetVectorForAxis(int32 Axis) const;
    int32 GetSelectedAxis() const { return SelectedAxis; }
    void SetSelectedAxis(int32 InAxis) { SelectedAxis = (InAxis >= 0 && InAxis <= 3) ? InAxis : -1; }
    void SetHolding(bool bHold);
    bool IsHolding() const { return bIsHolding; }
    bool IsHovered() const { return SelectedAxis != -1; }

    void SetPressedOnHandle(bool bPressed) { bPressedOnHandle = bPressed; }
    bool IsPressedOnHandle() const { return bPressedOnHandle; }

    EGizmoMode GetMode() const { return CurMode; }
    void UpdateGizmoMode(EGizmoMode NewMode);
    void SetTranslateMode() { UpdateGizmoMode(EGizmoMode::Translate); }
    void SetRotateMode() { UpdateGizmoMode(EGizmoMode::Rotate); }
    void SetScaleMode() { UpdateGizmoMode(EGizmoMode::Scale); }
    void SetSelectMode() { UpdateGizmoMode(EGizmoMode::Select); }

    void SetAxisMask(uint32 InMask) { AxisMask = InMask; }
    uint32 GetAxisMask() const { return AxisMask; }
    static uint32 ComputeAxisMask(ELevelViewportType ViewportType, EGizmoMode Mode);

    void SetGizmoWorldTransform(const FTransform& InWorldTransform);
    void ClearGizmoWorldTransform();
    bool HasVisualTarget() const { return bHasVisualTarget; }

    void ApplyGizmoWorldTransform(const FTransform& InWorldTransform);
    float ComputeScreenSpaceScale(const FVector& CameraLocation, bool bIsOrtho = false, float OrthoWidth = 10.0f, float ViewportHeight = 0.0f) const;
    void ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho = false, float OrthoWidth = 10.0f, float ViewportHeight = 0.0f);

    void SetWorldSpace(bool bWorldSpace);
    void SetGizmoSpace(EGizmoSpace InSpace) { SetWorldSpace(InSpace == EGizmoSpace::World); }
    bool IsWorldSpace() const { return bIsWorldSpace; }
    FQuat GetVisualRotationQuat() const;
    FMatrix GetVisualRotationMatrix() const;

    void ResetVisualInteractionState();

    // Editor gizmo visuals are render-only tool components. They must never enter
    // the normal actor/component picking path; gizmo picking is handled only by
    // FGizmoManager::HitTestHitProxy().
    bool SupportsWorldGizmo() const override { return false; }
    bool SupportsOutline() const override { return false; }
    bool ParticipatesInRenderSpatialStructure() const override { return false; }
    bool ParticipatesInPickingSpatialStructure() const override { return false; }
    FMeshBuffer* GetMeshBuffer() const override;
    FMeshDataView GetMeshDataView() const override { return MeshData ? FMeshDataView::FromMeshData(*MeshData) : FMeshDataView{}; }
    FPrimitiveSceneProxy* CreateSceneProxy() override;
    void CreateRenderState() override;
    void DestroyRenderState() override;
    void Deactivate() override;

    // Actor 없이 독립 생성된 Gizmo용 — 외부에서 Scene을 직접 지정한다.
    void SetScene(FScene* InScene) { RegisteredScene = InScene; }

private:
    void UpdateVisualTransform();

private:
    bool bHasVisualTarget = false;
    FTransform VisualWorldTransform;

    EGizmoMode CurMode = EGizmoMode::Translate;
    const float AxisLength = 1.0f;
    float Radius = 0.1f;

    int32 SelectedAxis = -1;
    bool bIsHolding = false;
    bool bIsWorldSpace = true;
    bool bPressedOnHandle = false;

    const FMeshData* MeshData = nullptr;
    uint32 AxisMask = 0x7; // 비트 0=X, 1=Y, 2=Z — hit proxy와 렌더링에서 함께 사용한다.
    FPrimitiveSceneProxy* InnerProxy = nullptr;
    FScene* RegisteredScene = nullptr;
};
