#pragma once

#include "Component/Gizmo/GizmoTypes.h"
#include "Component/Gizmo/GizmoHitProxyTypes.h"
#include "Component/Gizmo/GizmoDeltaTarget.h"
#include "Component/Gizmo/TransformGizmoTarget.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Math/Transform.h"

#include "ImGui/imgui.h"

#include <memory>

class UGizmoVisualComponent;
class FScene;

enum class EGizmoInteractionPolicy : uint8
{
    Interactive,
    VisualOnly
};


struct FUIScreenGizmoInteractionState
{
    int32 HoveredAxis = 0;
    int32 ActiveAxis = 0;
    bool bDragging = false;
    ImVec2 LastMousePos = ImVec2(0.0f, 0.0f);

    void Reset()
    {
        HoveredAxis = 0;
        ActiveAxis = 0;
        bDragging = false;
        LastMousePos = ImVec2(0.0f, 0.0f);
    }
};

struct FGizmoTargetKey
{
    const void* Owner = nullptr;
    int32 SubIndex = -1;

    bool operator==(const FGizmoTargetKey& Other) const
    {
        return Owner == Other.Owner && SubIndex == Other.SubIndex;
    }

    bool operator!=(const FGizmoTargetKey& Other) const
    {
        return !(*this == Other);
    }
};

// Editor 공용 기즈모 컨트롤러.
// - 실제 조작 대상은 ITransformGizmoTarget로 추상화한다.
// - 화면에 보이는 3D 기즈모는 UGizmoVisualComponent가 담당한다.
// - mode/space/snap/피킹/드래그/타겟 Transform 반영은 Manager가 담당한다.
class FGizmoManager
{
public:
    void SetTarget(std::shared_ptr<ITransformGizmoTarget> InTarget, std::shared_ptr<IGizmoDeltaTarget> InDeltaTarget = nullptr);
    bool SetTargetIfChanged(const FGizmoTargetKey& InKey,
                           std::shared_ptr<ITransformGizmoTarget> InTarget,
                           std::shared_ptr<IGizmoDeltaTarget> InDeltaTarget = nullptr);
    void ClearTarget();

    bool HasValidTarget() const;
    std::shared_ptr<ITransformGizmoTarget> GetTarget() const { return Target; }

    // Visual component is owned by FGizmoManager. External code must not mutate it directly.
    // Viewport clients create/register/release the visual only through these manager APIs.
    void EnsureVisualComponent();
    void ReleaseVisualComponent();
    bool HasVisualComponent() const { return VisualComponent != nullptr; }
    void SetVisualWorldLocation(const FVector& InLocation);
    void SetVisualScene(FScene* InScene);
    void ClearVisualScene();
    void CreateVisualRenderState();
    void DestroyVisualRenderState();
    void RegisterVisualToScene(FScene* InScene);
    void UnregisterVisualFromScene();

    void SetInteractionPolicy(EGizmoInteractionPolicy InPolicy);
    EGizmoInteractionPolicy GetInteractionPolicy() const { return InteractionPolicy; }
    bool CanInteract() const;

    void       SetMode(EGizmoMode InMode);
    EGizmoMode GetMode() const { return Mode; }
    void       CycleMode();

    void        SetSpace(EGizmoSpace InSpace);
    EGizmoSpace GetSpace() const { return Space; }

    void SetSnapSettings(bool bTranslationEnabled, float InTranslationSnapSize,
                         bool bRotationEnabled, float InRotationSnapSizeDegrees,
                         bool bScaleEnabled, float InScaleSnapSize);

    bool IsDragging() const { return bDragging; }

    void SyncVisualFromTarget();
    void ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth, float ViewportHeight = 0.0f);
    void SetAxisMask(uint32 InAxisMask);
    void ResetVisualInteractionState();

    // Gizmo handle selection uses a small ID-buffer hit proxy, not raycast.
    // The ray is still used after a handle has been selected to compute drag deltas.
    bool HitTestHitProxy(const FGizmoHitProxyContext& Context, FGizmoHitProxyResult& OutHitResult);
    bool BeginDragFromHitProxy(const FGizmoHitProxyResult& HitResult);
    void UpdateDrag(const FRay& Ray);
    void EndDrag();
    void CancelDrag();

    // Used when a document tab/editor context loses live ownership.
    // This must only detach the live drag/target session; it must not write
    // DragStartWorldMatrix back to the target because stale or uninitialized
    // drag-start data can move actors/components to the origin on tab return.
    void AbortLiveInteractionWithoutApplying();

    void SetTargetWorldLocation(const FVector& NewLocation);

    FUIScreenGizmoInteractionState& GetUIScreenInteractionState() { return UIScreenInteraction; }
    const FUIScreenGizmoInteractionState& GetUIScreenInteractionState() const { return UIScreenInteraction; }

private:
    FVector GetAxisVector(int32 Axis) const;
    FVector GetAxisVectorFromTransform(const FTransform& Transform, int32 Axis, EGizmoMode InMode, EGizmoSpace InSpace) const;
    bool ComputeLinearIntersection(const FRay& Ray, FVector& OutPoint);
    bool ComputePlanarIntersection(const FRay& Ray, FVector& OutPoint);
    bool ComputeAngularIntersection(const FRay& Ray, FVector& OutPoint);
    float ApplySnapToDragAmount(float DragAmount);
    float ApplySnapToTotalDragAmount(float RawTotalAmount) const;
    void ResetSnapAccumulation();
    void ApplyLinearDrag(const FRay& Ray);
    void ApplyAngularDrag(const FRay& Ray);
    void ApplyWorldScaleDrag(float ScaleDelta);

private:
    std::shared_ptr<ITransformGizmoTarget> Target;
    std::shared_ptr<IGizmoDeltaTarget> DeltaTarget;
    FGizmoTargetKey TargetKey{};
    bool bHasTargetKey = false;
    UGizmoVisualComponent* VisualComponent = nullptr;
    FScene* RegisteredVisualScene = nullptr;
    FUIScreenGizmoInteractionState UIScreenInteraction;

    EGizmoInteractionPolicy InteractionPolicy = EGizmoInteractionPolicy::Interactive;
    EGizmoMode  Mode = EGizmoMode::Translate;
    EGizmoSpace Space = EGizmoSpace::Local;

    bool bDragging = false;
    bool bFirstDragUpdate = true;
    int32 ActiveAxis = -1;
    EGizmoMode DragStartMode = EGizmoMode::Translate;
    EGizmoSpace DragStartSpace = EGizmoSpace::Local;
    FTransform DragStartTransform;
    FMatrix DragStartWorldMatrix = FMatrix::Identity;
    FVector DragStartCenter = FVector::ZeroVector;
    FVector DragStartIntersectionLocation = FVector::ZeroVector;
    FVector LastIntersectionLocation;
    FVector DragAxisVector = FVector(1.0f, 0.0f, 0.0f);
    FVector DragPlaneNormal = FVector(0.0f, 0.0f, 1.0f);

    bool bTranslationSnapEnabled = false;
    bool bRotationSnapEnabled = false;
    bool bScaleSnapEnabled = false;
    float TranslationSnapSize = 10.0f;
    float RotationSnapSizeRadians = 0.261799395f;
    float ScaleSnapSize = 0.1f;
    float AccumulatedRawDragAmount = 0.0f;
    float LastAppliedSnappedDragAmount = 0.0f;
};
