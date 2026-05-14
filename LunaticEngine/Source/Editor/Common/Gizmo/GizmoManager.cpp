#include "PCH/LunaticPCH.h"
#include "Common/Gizmo/GizmoManager.h"

#include "Component/GizmoVisualComponent.h"
#include "Object/Object.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

void FGizmoManager::SetTarget(std::shared_ptr<ITransformGizmoTarget> InTarget, std::shared_ptr<IGizmoDeltaTarget> InDeltaTarget)
{
    // Target replacement is an ownership/selection-context change, not an explicit user cancel.
    // Do not restore DragStartWorldMatrix here: tab/context switches can otherwise write a
    // stale start matrix back to an unrelated or old target.
    if (bDragging)
    {
        AbortLiveInteractionWithoutApplying();
    }

    Target = InTarget;
    DeltaTarget = InDeltaTarget;
    bHasTargetKey = false;
    SyncVisualFromTarget();
}

bool FGizmoManager::SetTargetIfChanged(const FGizmoTargetKey& InKey,
                                       std::shared_ptr<ITransformGizmoTarget> InTarget,
                                       std::shared_ptr<IGizmoDeltaTarget> InDeltaTarget)
{
    if (bHasTargetKey && TargetKey == InKey && HasValidTarget())
    {
        Target = InTarget;
        DeltaTarget = InDeltaTarget;
        SyncVisualFromTarget();
        return false;
    }

    // A target-key change means the old live target no longer owns the interaction.
    // This is not an explicit user cancel, so never restore the old drag-start matrix.
    if (bDragging)
    {
        AbortLiveInteractionWithoutApplying();
    }

    TargetKey = InKey;
    bHasTargetKey = true;
    Target = InTarget;
    DeltaTarget = InDeltaTarget;
    SyncVisualFromTarget();
    return true;
}

void FGizmoManager::ClearTarget()
{
    // Clearing a target usually happens on selection/tab/context loss.
    // Detach the live session without restoring DragStartWorldMatrix.
    if (bDragging)
    {
        AbortLiveInteractionWithoutApplying();
    }

    Target.reset();
    DeltaTarget.reset();
    bHasTargetKey = false;
    SyncVisualFromTarget();
}

bool FGizmoManager::HasValidTarget() const
{
    return Target && Target->IsValid();
}

void FGizmoManager::EnsureVisualComponent()
{
    if (VisualComponent)
    {
        SyncVisualFromTarget();
        return;
    }

    VisualComponent = UObjectManager::Get().CreateObject<UGizmoVisualComponent>();
    SyncVisualFromTarget();
}

void FGizmoManager::ReleaseVisualComponent()
{
    if (!VisualComponent)
    {
        RegisteredVisualScene = nullptr;
        return;
    }

    UnregisterVisualFromScene();
    VisualComponent->ResetVisualInteractionState();
    UObjectManager::Get().DestroyObject(VisualComponent);
    VisualComponent = nullptr;
}

void FGizmoManager::SetVisualWorldLocation(const FVector& InLocation)
{
    EnsureVisualComponent();
    if (VisualComponent)
    {
        VisualComponent->SetWorldLocation(InLocation);
    }
}

void FGizmoManager::SetVisualScene(FScene* InScene)
{
    EnsureVisualComponent();
    if (VisualComponent)
    {
        VisualComponent->SetScene(InScene);
    }
}

void FGizmoManager::ClearVisualScene()
{
    if (VisualComponent)
    {
        VisualComponent->SetScene(nullptr);
    }
}

void FGizmoManager::CreateVisualRenderState()
{
    EnsureVisualComponent();
    if (VisualComponent)
    {
        VisualComponent->CreateRenderState();
    }
}

void FGizmoManager::DestroyVisualRenderState()
{
    if (VisualComponent)
    {
        VisualComponent->DestroyRenderState();
    }
}

void FGizmoManager::RegisterVisualToScene(FScene* InScene)
{
    if (!InScene)
    {
        UnregisterVisualFromScene();
        return;
    }

    EnsureVisualComponent();

    // Mode 전환 시 UGizmoVisualComponent::MarkRenderStateDirty()가 호출되면
    // DestroyRenderState() -> CreateRenderState() 순서로 proxy가 재생성된다.
    // 이때 Actor 없이 독립 생성된 gizmo는 Scene 포인터가 반드시 다시 보장되어야 한다.
    // 따라서 같은 Scene이어도 SetVisualScene/CreateVisualRenderState를 idempotent하게 호출해
    // stale registration 상태를 회복할 수 있게 한다.
    if (RegisteredVisualScene != InScene)
    {
        UnregisterVisualFromScene();
        RegisteredVisualScene = InScene;
    }

    SetVisualScene(InScene);
    CreateVisualRenderState();
    SyncVisualFromTarget();
}

void FGizmoManager::UnregisterVisualFromScene()
{
    if (RegisteredVisualScene)
    {
        DestroyVisualRenderState();
    }
    ClearVisualScene();
    RegisteredVisualScene = nullptr;
}

void FGizmoManager::SetInteractionPolicy(EGizmoInteractionPolicy InPolicy)
{
    if (InteractionPolicy == InPolicy)
    {
        return;
    }

    InteractionPolicy = InPolicy;
    if (InteractionPolicy == EGizmoInteractionPolicy::VisualOnly)
    {
        AbortLiveInteractionWithoutApplying();
        ResetVisualInteractionState();
    }
}

bool FGizmoManager::CanInteract() const
{
    return InteractionPolicy == EGizmoInteractionPolicy::Interactive && VisualComponent && HasValidTarget();
}

void FGizmoManager::SetMode(EGizmoMode InMode)
{
    if (Mode == InMode)
    {
        SyncVisualFromTarget();
        return;
    }

    // Mode change invalidates the currently selected handle and drag plane.
    // Keep this transition manager-owned so toolbar/shortcut paths cannot leave
    // UGizmoVisualComponent in a stale pressed/holding state.
    CancelDrag();
    ResetVisualInteractionState();

    Mode = InMode;
    if (VisualComponent)
    {
        VisualComponent->UpdateGizmoMode(Mode);
    }
    SyncVisualFromTarget();
}

void FGizmoManager::CycleMode()
{
    int NextInt = (static_cast<int>(Mode) + 1) % static_cast<int>(EGizmoMode::End);
    if (NextInt == 0)
    {
        NextInt = 1;
    }
    SetMode(static_cast<EGizmoMode>(NextInt));
}

void FGizmoManager::SetSpace(EGizmoSpace InSpace)
{
    if (Space == InSpace)
    {
        SyncVisualFromTarget();
        return;
    }

    // 좌표계 전환은 축/평면 source of truth를 바꾸므로 진행 중 drag를 유지하면
    // visual axis와 drag axis가 어긋날 수 있다.
    CancelDrag();
    ResetVisualInteractionState();

    Space = InSpace;
    if (VisualComponent)
    {
        VisualComponent->SetGizmoSpace(Space);
    }
    SyncVisualFromTarget();
}

void FGizmoManager::SetSnapSettings(bool bTranslationEnabled, float InTranslationSnapSize,
                                    bool bRotationEnabled, float InRotationSnapSizeDegrees,
                                    bool bScaleEnabled, float InScaleSnapSize)
{
    bTranslationSnapEnabled = bTranslationEnabled;
    TranslationSnapSize = (InTranslationSnapSize > FMath::Epsilon) ? InTranslationSnapSize : 10.0f;
    bRotationSnapEnabled = bRotationEnabled;
    RotationSnapSizeRadians = ((InRotationSnapSizeDegrees > FMath::Epsilon) ? InRotationSnapSizeDegrees : 15.0f) * DEG_TO_RAD;
    bScaleSnapEnabled = bScaleEnabled;
    ScaleSnapSize = (InScaleSnapSize > FMath::Epsilon) ? InScaleSnapSize : 0.1f;
}

void FGizmoManager::SyncVisualFromTarget()
{
    if (!VisualComponent)
    {
        return;
    }

    VisualComponent->UpdateGizmoMode(Mode);
    VisualComponent->SetGizmoSpace(Space);

    if (!Target || !Target->IsValid())
    {
        VisualComponent->ClearGizmoWorldTransform();
        return;
    }

    FTransform VisualTransform = Target->GetWorldTransform();

    if (bDragging)
    {
        // 드래그 중에는 기즈모의 기준 축이 변하면 안 된다.
        // Target transform은 매 프레임 갱신되지만, Local/Scale/Rotate 축까지 현재 transform에서
        // 다시 뽑으면 조작 중 링/축이 따라 돌면서 delta 계산과 visual 기준이 계속 바뀐다.
        // 따라서 위치는 현재 Target을 따라가되, 축을 결정하는 Rotation/Scale 기준은
        // 드래그 시작 시점의 transform으로 고정한다.
        VisualTransform.Rotation = DragStartTransform.Rotation;
        VisualTransform.Scale = DragStartTransform.Scale;
    }

    VisualComponent->SetGizmoWorldTransform(VisualTransform);
}

void FGizmoManager::ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth, float ViewportHeight)
{
    if (VisualComponent && HasValidTarget())
    {
        VisualComponent->ApplyScreenSpaceScaling(CameraLocation, bIsOrtho, OrthoWidth, ViewportHeight);
    }
}

void FGizmoManager::SetAxisMask(uint32 InAxisMask)
{
    if (VisualComponent)
    {
        VisualComponent->SetAxisMask(InAxisMask);
    }
}

void FGizmoManager::ResetVisualInteractionState()
{
    // Reset both the 3D gizmo visual and the screen-space interaction cache.
    // Tab/context switches must not leave HoveredAxis/ActiveAxis/bDragging alive,
    // otherwise returning to the tab can apply stale input to the old target.
    UIScreenInteraction.Reset();

    if (VisualComponent)
    {
        VisualComponent->ResetVisualInteractionState();
    }
}

bool FGizmoManager::HitTestHitProxy(const FGizmoHitProxyContext& Context, FGizmoHitProxyResult& OutHitResult)
{
    OutHitResult.Reset();

    if (!CanInteract() || Mode == EGizmoMode::Select)
    {
        return false;
    }

    SyncVisualFromTarget();

    if (!VisualComponent || !VisualComponent->IsVisible() || !VisualComponent->HasVisualTarget())
    {
        return false;
    }

    if (Target && Target->IsValid())
    {
        const FVector TargetLocation = Target->GetWorldTransform().GetLocation();
        const FVector VisualLocation = VisualComponent->GetWorldLocation();
        const float AllowedErrorSq = 0.01f;
        if (FVector::DistSquared(TargetLocation, VisualLocation) > AllowedErrorSq)
        {
            VisualComponent->ClearGizmoWorldTransform();
            return false;
        }
    }

    if (!bDragging && VisualComponent->IsHolding())
    {
        VisualComponent->ResetVisualInteractionState();
    }

    if (!VisualComponent->HitProxyTest(Context, OutHitResult))
    {
        if (!VisualComponent->IsHolding())
        {
            VisualComponent->SetSelectedAxis(-1);
        }
        return false;
    }

    if (!VisualComponent->IsHolding())
    {
        VisualComponent->SetSelectedAxis(OutHitResult.Axis);
    }
    return true;
}

bool FGizmoManager::BeginDragFromHitProxy(const FGizmoHitProxyResult& HitResult)
{
    if (!CanInteract() || !HitResult.bHit || HitResult.Axis < 0)
    {
        if (VisualComponent)
        {
            VisualComponent->SetPressedOnHandle(false);
        }
        return false;
    }

    ActiveAxis = HitResult.Axis;
    if (VisualComponent)
    {
        VisualComponent->SetSelectedAxis(ActiveAxis);
    }

    Target->BeginTransform();
    DragStartMode = Mode;
    DragStartSpace = Space;
    DragStartTransform = Target->GetWorldTransform();
    DragStartWorldMatrix = Target->GetWorldMatrix();
    DragStartCenter = DragStartTransform.GetLocation();
    DragAxisVector = GetAxisVectorFromTransform(DragStartTransform, ActiveAxis, DragStartMode, DragStartSpace).Normalized();
    if (ActiveAxis >= 0 && ActiveAxis <= 2 && DragAxisVector.IsNearlyZero())
    {
        Target->EndTransform();
        ActiveAxis = -1;
        if (VisualComponent)
        {
            VisualComponent->SetPressedOnHandle(false);
            VisualComponent->SetHolding(false);
        }
        return false;
    }
    LastIntersectionLocation = FVector::ZeroVector;
    bFirstDragUpdate = true;

    if (DragStartMode == EGizmoMode::Translate && DeltaTarget)
    {
        FGizmoDragBeginContext BeginContext{};
        BeginContext.Mode = DragStartMode;
        BeginContext.Space = DragStartSpace;
        BeginContext.ActiveAxis = ActiveAxis;
        BeginContext.StartTargetTransform = DragStartTransform;
        BeginContext.StartTargetWorldMatrix = DragStartWorldMatrix;
        BeginContext.StartAxisVector = DragAxisVector;
        if (!DeltaTarget->BeginGizmoDrag(BeginContext))
        {
            Target->EndTransform();
            ActiveAxis = -1;
            if (VisualComponent)
            {
                VisualComponent->SetPressedOnHandle(false);
                VisualComponent->SetHolding(false);
            }
            return false;
        }
    }

    bDragging = true;
    ResetSnapAccumulation();

    if (VisualComponent)
    {
        VisualComponent->SetPressedOnHandle(true);
        VisualComponent->SetHolding(true);
    }
    return true;
}

void FGizmoManager::UpdateDrag(const FRay& Ray)
{
    if (!bDragging || !CanInteract())
    {
        return;
    }

    if (DragStartMode == EGizmoMode::Rotate)
    {
        ApplyAngularDrag(Ray);
    }
    else
    {
        ApplyLinearDrag(Ray);
    }

    SyncVisualFromTarget();
}

void FGizmoManager::EndDrag()
{
    if (bDragging && Target && Target->IsValid())
    {
        if (DragStartMode == EGizmoMode::Translate && DeltaTarget)
        {
            DeltaTarget->EndGizmoDrag(false);
        }
        Target->EndTransform();
    }

    bDragging = false;
    bFirstDragUpdate = true;
    ActiveAxis = -1;
    ResetSnapAccumulation();

    ResetVisualInteractionState();
    SyncVisualFromTarget();
}

void FGizmoManager::CancelDrag()
{
    if (bDragging && Target && Target->IsValid())
    {
        if (DragStartMode == EGizmoMode::Translate && DeltaTarget)
        {
            DeltaTarget->EndGizmoDrag(true);
        }
        else
        {
            Target->SetWorldMatrix(DragStartWorldMatrix);
        }
        Target->EndTransform();
    }

    bDragging = false;
    bFirstDragUpdate = true;
    ActiveAxis = -1;
    ResetSnapAccumulation();
    ResetVisualInteractionState();
    SyncVisualFromTarget();
}

void FGizmoManager::AbortLiveInteractionWithoutApplying()
{
    // Context/tab deactivation is not a user Cancel command. It is a live-session
    // detach. Do not restore DragStartWorldMatrix here: when bDragging is stale or
    // the start matrix was captured before the current target was fully synced,
    // restoring it can snap the actor/component to identity/origin when returning
    // to the Level Editor.
    if (bDragging && Target && Target->IsValid())
    {
        if (DragStartMode == EGizmoMode::Translate && DeltaTarget)
        {
            DeltaTarget->EndGizmoDrag(false);
        }

        // End the target edit scope if one was opened, but leave the current target
        // transform untouched. Hidden tabs must not receive further input after this.
        Target->EndTransform();
    }

    bDragging = false;
    bFirstDragUpdate = true;
    ActiveAxis = -1;
    bHasTargetKey = false;
    Target.reset();
    DeltaTarget.reset();
    ResetSnapAccumulation();
    ResetVisualInteractionState();
    SyncVisualFromTarget();
}

void FGizmoManager::SetTargetWorldLocation(const FVector& NewLocation)
{
    if (!HasValidTarget())
    {
        return;
    }
    FTransform Transform = Target->GetWorldTransform();
    Transform.SetLocation(NewLocation);
    Target->SetWorldTransform(Transform);
    SyncVisualFromTarget();
}

FVector FGizmoManager::GetAxisVectorFromTransform(const FTransform& Transform, int32 Axis, EGizmoMode InMode, EGizmoSpace InSpace) const
{
    (void)InMode;
    // 실제 조작 축은 visual component의 현재 matrix가 아니라 mode/space와 대상 transform에서 계산한다.
    // Render/HitProxy/Drag가 동일한 축 정책을 쓰도록 드래그 시작 시점의 mode/space를 함께 넘긴다.
    const bool bUseLocalAxes = (InSpace == EGizmoSpace::Local);
    if (!bUseLocalAxes)
    {
        switch (Axis)
        {
        case 0: return FVector(1.0f, 0.0f, 0.0f);
        case 1: return FVector(0.0f, 1.0f, 0.0f);
        case 2: return FVector(0.0f, 0.0f, 1.0f);
        default: return FVector::ZeroVector;
        }
    }

    const FQuat Rotation = Transform.Rotation.GetNormalized();
    switch (Axis)
    {
    case 0: return Rotation.GetForwardVector().Normalized();
    case 1: return Rotation.GetRightVector().Normalized();
    case 2: return Rotation.GetUpVector().Normalized();
    default: return FVector::ZeroVector;
    }
}

FVector FGizmoManager::GetAxisVector(int32 Axis) const
{
    if (bDragging && Axis == ActiveAxis && Axis >= 0 && Axis <= 2)
    {
        return DragAxisVector.Normalized();
    }

    if (Target && Target->IsValid())
    {
        return GetAxisVectorFromTransform(Target->GetWorldTransform(), Axis, Mode, Space).Normalized();
    }

    return FVector::ZeroVector;
}

bool FGizmoManager::ComputeLinearIntersection(const FRay& Ray, FVector& OutPoint)
{
    if (!VisualComponent || ActiveAxis < 0 || ActiveAxis > 2)
    {
        return false;
    }

    const FVector AxisVector = GetAxisVector(ActiveAxis);
    const FVector PlaneNormal = AxisVector.Cross(Ray.Direction);
    const FVector ProjectDir = PlaneNormal.Cross(AxisVector);
    const float Denom = Ray.Direction.Dot(ProjectDir);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const FVector Center = bDragging ? DragStartCenter : VisualComponent->GetWorldLocation();
    const float DistanceToPlane = (Center - Ray.Origin).Dot(ProjectDir) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return true;
}

bool FGizmoManager::ComputePlanarIntersection(const FRay& Ray, FVector& OutPoint)
{
    if (!VisualComponent)
    {
        return false;
    }

    if (bFirstDragUpdate)
    {
        DragPlaneNormal = (Ray.Direction * -1.0f).Normalized();
    }

    const float Denom = Ray.Direction.Dot(DragPlaneNormal);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const FVector Center = bDragging ? DragStartCenter : VisualComponent->GetWorldLocation();
    const float DistanceToPlane = (Center - Ray.Origin).Dot(DragPlaneNormal) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return true;
}

bool FGizmoManager::ComputeAngularIntersection(const FRay& Ray, FVector& OutPoint)
{
    if (!VisualComponent || ActiveAxis < 0 || ActiveAxis > 2)
    {
        return false;
    }

    const FVector AxisVector = GetAxisVector(ActiveAxis);
    const float Denom = Ray.Direction.Dot(AxisVector);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const FVector Center = bDragging ? DragStartCenter : VisualComponent->GetWorldLocation();
    const float DistanceToPlane = (Center - Ray.Origin).Dot(AxisVector) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return true;
}


float FGizmoManager::ApplySnapToDragAmount(float DragAmount)
{
    bool bSnapEnabled = false;
    float SnapSize = 0.0f;

    const EGizmoMode SnapMode = bDragging ? DragStartMode : Mode;
    switch (SnapMode)
    {
    case EGizmoMode::Translate:
        bSnapEnabled = bTranslationSnapEnabled;
        SnapSize = TranslationSnapSize;
        break;
    case EGizmoMode::Rotate:
        bSnapEnabled = bRotationSnapEnabled;
        SnapSize = RotationSnapSizeRadians;
        break;
    case EGizmoMode::Scale:
        bSnapEnabled = bScaleSnapEnabled;
        SnapSize = ScaleSnapSize;
        break;
    default:
        break;
    }

    if (!bSnapEnabled || SnapSize <= FMath::Epsilon)
    {
        return DragAmount;
    }

    AccumulatedRawDragAmount += DragAmount;
    const float SnappedTotal = std::floor((AccumulatedRawDragAmount / SnapSize) + 0.5f) * SnapSize;
    const float DeltaToApply = SnappedTotal - LastAppliedSnappedDragAmount;
    LastAppliedSnappedDragAmount = SnappedTotal;
    return DeltaToApply;
}

float FGizmoManager::ApplySnapToTotalDragAmount(float RawTotalAmount) const
{
    bool bSnapEnabled = false;
    float SnapSize = 0.0f;

    const EGizmoMode SnapMode = bDragging ? DragStartMode : Mode;
    switch (SnapMode)
    {
    case EGizmoMode::Translate:
        bSnapEnabled = bTranslationSnapEnabled;
        SnapSize = TranslationSnapSize;
        break;
    case EGizmoMode::Rotate:
        bSnapEnabled = bRotationSnapEnabled;
        SnapSize = RotationSnapSizeRadians;
        break;
    case EGizmoMode::Scale:
        bSnapEnabled = bScaleSnapEnabled;
        SnapSize = ScaleSnapSize;
        break;
    default:
        break;
    }

    if (!bSnapEnabled || SnapSize <= FMath::Epsilon)
    {
        return RawTotalAmount;
    }

    return std::floor((RawTotalAmount / SnapSize) + 0.5f) * SnapSize;
}


void FGizmoManager::ResetSnapAccumulation()
{
    AccumulatedRawDragAmount = 0.0f;
    LastAppliedSnappedDragAmount = 0.0f;
}

void FGizmoManager::ApplyLinearDrag(const FRay& Ray)
{
    FVector CurrentIntersection;
    const bool bPlanar = (ActiveAxis == 3);
    const bool bHit = bPlanar ? ComputePlanarIntersection(Ray, CurrentIntersection)
                              : ComputeLinearIntersection(Ray, CurrentIntersection);
    if (!bHit)
    {
        return;
    }

    if (bFirstDragUpdate)
    {
        DragStartIntersectionLocation = CurrentIntersection;
        LastIntersectionLocation = CurrentIntersection;
        bFirstDragUpdate = false;
        return;
    }

    if (DragStartMode == EGizmoMode::Scale && !bPlanar)
    {
        const FVector TotalDelta = CurrentIntersection - DragStartIntersectionLocation;
        const float RawScaleDelta = TotalDelta.Dot(DragAxisVector);
        const float ScaleDelta = ApplySnapToTotalDragAmount(RawScaleDelta);
        if (std::abs(ScaleDelta) <= FMath::Epsilon)
        {
            LastIntersectionLocation = CurrentIntersection;
            return;
        }

        if (DragStartSpace == EGizmoSpace::World)
        {
            ApplyWorldScaleDrag(ScaleDelta);
        }
        else
        {
            FTransform NewTransform = DragStartTransform;
            FVector NewScale = DragStartTransform.Scale;
            if (ActiveAxis == 0) NewScale.X = (std::max)(0.001f, DragStartTransform.Scale.X + ScaleDelta);
            if (ActiveAxis == 1) NewScale.Y = (std::max)(0.001f, DragStartTransform.Scale.Y + ScaleDelta);
            if (ActiveAxis == 2) NewScale.Z = (std::max)(0.001f, DragStartTransform.Scale.Z + ScaleDelta);
            NewTransform.Scale = NewScale;
            Target->SetWorldTransform(NewTransform);
        }

        LastIntersectionLocation = CurrentIntersection;
        return;
    }

    if (DragStartMode == EGizmoMode::Translate && DeltaTarget)
    {
        FGizmoDelta Delta{};
        Delta.Mode = DragStartMode;
        Delta.Space = DragStartSpace;
        Delta.ActiveAxis = ActiveAxis;
        Delta.StartTargetTransform = DragStartTransform;

        if (bPlanar)
        {
            Delta.LinearDeltaComponent = CurrentIntersection - DragStartIntersectionLocation;
            if (Delta.LinearDeltaComponent.Dot(Delta.LinearDeltaComponent) <= FMath::Epsilon)
            {
                LastIntersectionLocation = CurrentIntersection;
                return;
            }
        }
        else
        {
            const FVector TotalDelta = CurrentIntersection - DragStartIntersectionLocation;
            const float RawTotalDragAmount = TotalDelta.Dot(DragAxisVector);
            const float SnappedTotalDragAmount = ApplySnapToTotalDragAmount(RawTotalDragAmount);
            if (std::abs(SnappedTotalDragAmount) <= FMath::Epsilon)
            {
                LastIntersectionLocation = CurrentIntersection;
                return;
            }

            Delta.LinearDeltaComponent = DragAxisVector * SnappedTotalDragAmount;
        }

        if (DeltaTarget->ApplyGizmoDelta(Delta))
        {
            LastIntersectionLocation = CurrentIntersection;
            return;
        }
    }

    const FVector FullDelta = CurrentIntersection - LastIntersectionLocation;
    if (FullDelta.Dot(FullDelta) <= FMath::Epsilon)
    {
        return;
    }

    FTransform NewTransform = Target->GetWorldTransform();
    if (bPlanar)
    {
        NewTransform.SetLocation(NewTransform.GetLocation() + FullDelta);
    }
    else
    {
        const FVector AxisVector = GetAxisVector(ActiveAxis);
        float DragAmount = FullDelta.Dot(AxisVector);
        DragAmount = ApplySnapToDragAmount(DragAmount);
        if (std::abs(DragAmount) <= FMath::Epsilon)
        {
            LastIntersectionLocation = CurrentIntersection;
            return;
        }

        NewTransform.SetLocation(NewTransform.GetLocation() + AxisVector * DragAmount);
    }

    Target->SetWorldTransform(NewTransform);
    LastIntersectionLocation = CurrentIntersection;
}

void FGizmoManager::ApplyWorldScaleDrag(float ScaleDelta)
{
    if (!Target || ActiveAxis < 0 || ActiveAxis > 2)
    {
        return;
    }

    const float ScaleFactor = (std::max)(0.001f, 1.0f + ScaleDelta);
    FVector WorldScale(1.0f, 1.0f, 1.0f);
    if (ActiveAxis == 0) WorldScale.X = ScaleFactor;
    if (ActiveAxis == 1) WorldScale.Y = ScaleFactor;
    if (ActiveAxis == 2) WorldScale.Z = ScaleFactor;

    // Row-major convention: local point -> DragStartWorldMatrix -> world-space centered scale.
    // This keeps Scale World's source of truth as a world matrix instead of pretending that
    // FTransform::Scale is enough for a world-axis non-uniform scale on a rotated target.
    const FMatrix ToCenter = FMatrix::MakeTranslationMatrix(DragStartCenter * -1.0f);
    const FMatrix WorldScaleMatrix = FMatrix::MakeScaleMatrix(WorldScale);
    const FMatrix FromCenter = FMatrix::MakeTranslationMatrix(DragStartCenter);
    const FMatrix NewWorldMatrix = DragStartWorldMatrix * ToCenter * WorldScaleMatrix * FromCenter;
    Target->SetWorldMatrix(NewWorldMatrix);
}

void FGizmoManager::ApplyAngularDrag(const FRay& Ray)
{
    FVector CurrentIntersection;
    if (!ComputeAngularIntersection(Ray, CurrentIntersection))
    {
        return;
    }

    if (bFirstDragUpdate)
    {
        DragStartIntersectionLocation = CurrentIntersection;
        LastIntersectionLocation = CurrentIntersection;
        bFirstDragUpdate = false;
        return;
    }

    const FVector AxisVector = DragAxisVector.Normalized();
    const FVector Center = DragStartCenter;
    FVector CenterToStart = DragStartIntersectionLocation - Center;
    FVector CenterToCurrent = CurrentIntersection - Center;
    if (CenterToStart.Dot(CenterToStart) <= 1.0e-8f || CenterToCurrent.Dot(CenterToCurrent) <= 1.0e-8f)
    {
        LastIntersectionLocation = CurrentIntersection;
        return;
    }
    CenterToStart = CenterToStart.Normalized();
    CenterToCurrent = CenterToCurrent.Normalized();

    // acos(dot)+sign 방식은 0/180도 근처에서 부호가 튀기 쉽다.
    // atan2(signedSin, cos)를 쓰면 작은 각도, 큰 각도 모두 연속적으로 계산된다.
    const float SignedSin = AxisVector.Dot(CenterToStart.Cross(CenterToCurrent));
    const float CosAngle = Clamp(CenterToStart.Dot(CenterToCurrent), -1.0f, 1.0f);
    const float RawAngleRadians = std::atan2(SignedSin, CosAngle);
    const float TotalAngleRadians = ApplySnapToTotalDragAmount(RawAngleRadians);
    if (std::abs(TotalAngleRadians) <= FMath::Epsilon)
    {
        LastIntersectionLocation = CurrentIntersection;
        return;
    }

    FTransform NewTransform = DragStartTransform;
    const FQuat TotalDeltaQuat = FQuat::FromAxisAngle(AxisVector, TotalAngleRadians);
    NewTransform.Rotation = (TotalDeltaQuat * DragStartTransform.Rotation).GetNormalized();
    Target->SetWorldTransform(NewTransform);

    LastIntersectionLocation = CurrentIntersection;
}
