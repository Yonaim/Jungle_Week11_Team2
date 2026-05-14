#include "PCH/LunaticPCH.h"
#include "GizmoVisualComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Math/Quat.h"
#include "Math/Matrix.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Proxy/GizmoSceneProxy.h"
#include "Render/Types/EditorViewportScale.h"
#include "Render/Scene/FScene.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <limits>

IMPLEMENT_CLASS(UGizmoVisualComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UGizmoVisualComponent)

FPrimitiveSceneProxy* UGizmoVisualComponent::CreateSceneProxy()
{
	return new FGizmoSceneProxy(this, false); // Outer
}

void UGizmoVisualComponent::CreateRenderState()
{
	if (SceneProxy) return;

	FScene* Scene = RegisteredScene;
	if (!Scene && Owner && Owner->GetWorld())
		Scene = &Owner->GetWorld()->GetScene();
	if (!Scene) return;

	// Outer 프록시 (기본 경로)
	SceneProxy = Scene->AddPrimitive(this);

	// Inner 프록시 (별도 등록)
	InnerProxy = new FGizmoSceneProxy(this, true);
	Scene->RegisterProxy(InnerProxy);
}

void UGizmoVisualComponent::DestroyRenderState()
{
	FScene* Scene = RegisteredScene;
	if (!Scene && Owner && Owner->GetWorld())
		Scene = &Owner->GetWorld()->GetScene();

	if (Scene)
	{
		if (InnerProxy) { Scene->RemovePrimitive(InnerProxy); InnerProxy = nullptr; }
		if (SceneProxy) { Scene->RemovePrimitive(SceneProxy); SceneProxy = nullptr; }
	}

	// RegisteredScene은 여기서 끊지 않는다.
	// MarkRenderStateDirty()가 DestroyRenderState() 직후 CreateRenderState()를 호출하는데,
	// 여기서 RegisteredScene을 nullptr로 만들면 Actor 없이 독립 생성된 GizmoVisualComponent는
	// 다시 어떤 FScene에 등록되어야 하는지 알 수 없어 proxy가 재생성되지 않는다.
	// Scene 소유권을 실제로 끊어야 하는 경우에는 FGizmoManager::UnregisterVisualFromScene()이
	// DestroyRenderState() 이후 SetScene(nullptr)를 호출한다.
}

UGizmoVisualComponent::UGizmoVisualComponent()
{
	MeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::TransGizmo);
	LocalExtents = FVector(1.5f, 1.5f, 1.5f);

    // This component is not an actor-owned selectable primitive. It is owned by
    // FGizmoManager and rendered as an editor tool overlay, so keep it out of
    // component trees, details editing, collision, and normal world picking.
    SetEditorOnlyComponent(true);
    SetHiddenInComponentTree(true);
    SetCanDeleteFromDetails(false);
    SetCollisionEnabled(false);
}

void UGizmoVisualComponent::SetHolding(bool bHold)
{
    bIsHolding = bHold;
    if (!bIsHolding)
    {
        SetPressedOnHandle(false);
    }
}


bool UGizmoVisualComponent::LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult)
{
    (void)Ray;
    OutHitResult = {};
    // Transform gizmo picking is intentionally not raycast-based anymore.
    // FGizmoManager must use HitProxyTest(), then keep the returned handle id
    // as the interaction source of truth. This prevents the old visual-state /
    // analytical-raycast path from eating clicks at stale locations.
    return false;
}

namespace
{
struct FGizmoPickPixel
{
    int32 Axis = -1;
    float Depth = -std::numeric_limits<float>::max();
};

static float EdgeFunction(float Ax, float Ay, float Bx, float By, float Cx, float Cy)
{
    return (Cx - Ax) * (By - Ay) - (Cy - Ay) * (Bx - Ax);
}

static bool IsFiniteVec3(const FVector& V)
{
    return std::isfinite(V.X) && std::isfinite(V.Y) && std::isfinite(V.Z);
}

static bool IsAxisPickable(int32 Axis, uint32 AxisMask, EGizmoMode Mode)
{
    if (Axis >= 0 && Axis <= 2)
    {
        return (AxisMask & (1u << Axis)) != 0;
    }

    // Center/plane handle exists for translate gizmo only.
    return Axis == 3 && Mode == EGizmoMode::Translate;
}

static int32 ChooseTriangleAxis(const FVertex& A, const FVertex& B, const FVertex& C)
{
    if (A.SubID == B.SubID || A.SubID == C.SubID)
    {
        return A.SubID;
    }
    if (B.SubID == C.SubID)
    {
        return B.SubID;
    }
    return A.SubID;
}
}

bool UGizmoVisualComponent::HitProxyTest(const FGizmoHitProxyContext& Context, FGizmoHitProxyResult& OutResult) const
{
    OutResult.Reset();

    if (!IsVisible() || CurMode == EGizmoMode::Select || !HasVisualTarget() || !MeshData || MeshData->Indices.empty())
    {
        return false;
    }

    if (Context.ViewportWidth <= 0.0f || Context.ViewportHeight <= 0.0f || Context.PickRadius < 0)
    {
        return false;
    }

    constexpr int32 MaxPickRadius = 4; // 9x9 safety cap; default caller uses 2 => 5x5.
    const int32 RadiusPixels = std::min(Context.PickRadius, MaxPickRadius);
    const int32 PickSize = RadiusPixels * 2 + 1;
    constexpr int32 MaxPickSize = MaxPickRadius * 2 + 1;
    std::array<FGizmoPickPixel, MaxPickSize * MaxPickSize> PickBuffer{};

    const float PickMinX = Context.MouseX - static_cast<float>(RadiusPixels);
    const float PickMinY = Context.MouseY - static_cast<float>(RadiusPixels);

    // Render path does not rely on the component's relative scale.
    // FGizmoSceneProxy::UpdatePerViewport() recomputes a per-viewport
    // screen-space scale every frame from camera + viewport data.
    // Do the same here so the software ID-buffer pick mesh exactly matches
    // the rendered gizmo size after zoom/dolly/ortho-size changes.
    const float HitProxyScale = ComputeScreenSpaceScale(
        Context.CameraLocation,
        Context.bIsOrtho,
        Context.OrthoWidth,
        Context.ViewportHeight);

    const FMatrix WorldMatrix =
        FMatrix::MakeScaleMatrix(FVector(HitProxyScale, HitProxyScale, HitProxyScale)) *
        GetVisualRotationMatrix() *
        FMatrix::MakeTranslationMatrix(GetWorldLocation());
    const FMatrix WorldViewProjection = WorldMatrix * Context.ViewProjection;

    auto ProjectToViewport = [&](const FVector& LocalPosition, float& OutX, float& OutY, float& OutDepth) -> bool
    {
        const FVector Ndc = WorldViewProjection.TransformPositionWithW(LocalPosition);
        if (!IsFiniteVec3(Ndc))
        {
            return false;
        }

        // Keep a loose clip. The 5x5 pick buffer will clip rasterization anyway,
        // but rejecting completely invalid depth prevents handles behind the camera
        // from becoming pickable through projection artifacts.
        if (Ndc.Z < -1.0f || Ndc.Z > 1.0f)
        {
            return false;
        }

        OutX = (Ndc.X * 0.5f + 0.5f) * Context.ViewportWidth;
        OutY = (-Ndc.Y * 0.5f + 0.5f) * Context.ViewportHeight;
        OutDepth = Ndc.Z;
        return std::isfinite(OutX) && std::isfinite(OutY) && std::isfinite(OutDepth);
    };

    for (uint32 Tri = 0; Tri + 2 < static_cast<uint32>(MeshData->Indices.size()); Tri += 3)
    {
        const uint32 I0 = MeshData->Indices[Tri + 0];
        const uint32 I1 = MeshData->Indices[Tri + 1];
        const uint32 I2 = MeshData->Indices[Tri + 2];
        if (I0 >= MeshData->Vertices.size() || I1 >= MeshData->Vertices.size() || I2 >= MeshData->Vertices.size())
        {
            continue;
        }

        const FVertex& V0 = MeshData->Vertices[I0];
        const FVertex& V1 = MeshData->Vertices[I1];
        const FVertex& V2 = MeshData->Vertices[I2];
        const int32 Axis = ChooseTriangleAxis(V0, V1, V2);
        if (!IsAxisPickable(Axis, AxisMask, CurMode))
        {
            continue;
        }

        float X0, Y0, Z0;
        float X1, Y1, Z1;
        float X2, Y2, Z2;
        if (!ProjectToViewport(V0.Position, X0, Y0, Z0) ||
            !ProjectToViewport(V1.Position, X1, Y1, Z1) ||
            !ProjectToViewport(V2.Position, X2, Y2, Z2))
        {
            continue;
        }

        const float MinX = std::min({ X0, X1, X2 });
        const float MaxX = std::max({ X0, X1, X2 });
        const float MinY = std::min({ Y0, Y1, Y2 });
        const float MaxY = std::max({ Y0, Y1, Y2 });
        if (MaxX < PickMinX || MinX > PickMinX + static_cast<float>(PickSize) ||
            MaxY < PickMinY || MinY > PickMinY + static_cast<float>(PickSize))
        {
            continue;
        }

        const int32 StartX = std::max(0, static_cast<int32>(std::floor(MinX - PickMinX)));
        const int32 EndX = std::min(PickSize - 1, static_cast<int32>(std::ceil(MaxX - PickMinX)));
        const int32 StartY = std::max(0, static_cast<int32>(std::floor(MinY - PickMinY)));
        const int32 EndY = std::min(PickSize - 1, static_cast<int32>(std::ceil(MaxY - PickMinY)));
        if (StartX > EndX || StartY > EndY)
        {
            continue;
        }

        const float Area = EdgeFunction(X0, Y0, X1, Y1, X2, Y2);
        if (std::abs(Area) < 1e-6f)
        {
            continue;
        }

        for (int32 Py = StartY; Py <= EndY; ++Py)
        {
            for (int32 Px = StartX; Px <= EndX; ++Px)
            {
                const float SampleX = PickMinX + static_cast<float>(Px) + 0.5f;
                const float SampleY = PickMinY + static_cast<float>(Py) + 0.5f;

                const float W0 = EdgeFunction(X1, Y1, X2, Y2, SampleX, SampleY) / Area;
                const float W1 = EdgeFunction(X2, Y2, X0, Y0, SampleX, SampleY) / Area;
                const float W2 = EdgeFunction(X0, Y0, X1, Y1, SampleX, SampleY) / Area;
                const float Epsilon = -0.0001f;
                if (W0 < Epsilon || W1 < Epsilon || W2 < Epsilon)
                {
                    continue;
                }

                const float Depth = W0 * Z0 + W1 * Z1 + W2 * Z2;
                FGizmoPickPixel& Pixel = PickBuffer[Py * MaxPickSize + Px];
                // This renderer uses reversed-Z in most paths, so larger Z is closer.
                if (Pixel.Axis < 0 || Depth > Pixel.Depth)
                {
                    Pixel.Axis = Axis;
                    Pixel.Depth = Depth;
                }
            }
        }
    }

    int32 BestPixel = -1;
    int32 BestDistanceSq = std::numeric_limits<int32>::max();
    float BestDepth = -std::numeric_limits<float>::max();
    for (int32 Py = 0; Py < PickSize; ++Py)
    {
        for (int32 Px = 0; Px < PickSize; ++Px)
        {
            const FGizmoPickPixel& Pixel = PickBuffer[Py * MaxPickSize + Px];
            if (Pixel.Axis < 0)
            {
                continue;
            }

            const int32 Dx = Px - RadiusPixels;
            const int32 Dy = Py - RadiusPixels;
            const int32 DistSq = Dx * Dx + Dy * Dy;
            if (DistSq < BestDistanceSq || (DistSq == BestDistanceSq && Pixel.Depth > BestDepth))
            {
                BestDistanceSq = DistSq;
                BestDepth = Pixel.Depth;
                BestPixel = Py * MaxPickSize + Px;
            }
        }
    }

    if (BestPixel < 0)
    {
        return false;
    }

    const FGizmoPickPixel& HitPixel = PickBuffer[BestPixel];
    OutResult.bHit = true;
    OutResult.Axis = HitPixel.Axis;
    OutResult.Depth = HitPixel.Depth;
    return true;
}


FVector UGizmoVisualComponent::GetVectorForAxis(int32 Axis) const
{
	switch (Axis)
	{
	case 0:
		return GetForwardVector();
	case 1:
		return GetRightVector();
	case 2:
		return GetUpVector();
	default:
		return FVector(0.f, 0.f, 0.f);
	}
}


void UGizmoVisualComponent::UpdateGizmoMode(EGizmoMode NewMode)
{
	if (CurMode != NewMode)
	{
		// Mode changes swap the underlying handle mesh, so any cached hover/press
		// axis from the previous mode is no longer valid. The manager is the normal
		// entry point, but keep the visual component safe against legacy direct calls.
		ResetVisualInteractionState();
	}

	CurMode = NewMode;
	UpdateVisualTransform();
}

void UGizmoVisualComponent::SetGizmoWorldTransform(const FTransform& InWorldTransform)
{
	VisualWorldTransform = InWorldTransform;
	bHasVisualTarget = true;
	UpdateVisualTransform();
}

void UGizmoVisualComponent::ClearGizmoWorldTransform()
{
    bHasVisualTarget = false;
    SetVisibility(false);
    ResetVisualInteractionState();
}

void UGizmoVisualComponent::UpdateVisualTransform()
{
    if (bHasVisualTarget)
    {
        ApplyGizmoWorldTransform(VisualWorldTransform);
        return;
    }

    SetVisibility(false);
    ResetVisualInteractionState();
}

void UGizmoVisualComponent::ApplyGizmoWorldTransform(const FTransform& InWorldTransform)
{
	if (CurMode == EGizmoMode::Select)
	{
		SetVisibility(false);
		return;
	}
	else
	{
		SetVisibility(true);
	}

	const FVector DesiredLocation = InWorldTransform.GetLocation();
		
	FQuat DesiredRotation = FQuat::Identity;
	if (!bIsWorldSpace)
	{
		// 회전 표시에는 scale이 섞인 Transform matrix를 쓰지 않는다.
		// Non-uniform/negative scale이 있는 대상에서 한 축이 뒤집히는 원인이 된다.
		DesiredRotation = InWorldTransform.Rotation.GetNormalized();
	}

	const FMeshData* DesiredMeshData = nullptr;

	switch (CurMode)
	{
	case EGizmoMode::Scale:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::ScaleGizmo);
		break;

	case EGizmoMode::Rotate:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::RotGizmo);
		break;

	case EGizmoMode::Translate:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::TransGizmo);
		break;

	default:
		break;
	}

	if (FVector::DistSquared(GetWorldLocation(), DesiredLocation) > FMath::Epsilon * FMath::Epsilon)
	{
		SetWorldLocation(DesiredLocation);
	}

	if (!GetRelativeQuat().Equals(DesiredRotation))
	{
		SetRelativeRotation(DesiredRotation);
	}

	if (MeshData != DesiredMeshData && DesiredMeshData)
	{
		MeshData = DesiredMeshData;
		MarkRenderStateDirty();
	}
}

float UGizmoVisualComponent::ComputeScreenSpaceScale(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth, float ViewportHeight) const
{
	return FEditorViewportScale::ComputeWorldScale(
		CameraLocation,
		GetWorldLocation(),
		bIsOrtho,
		OrthoWidth,
		ViewportHeight,
		FEditorViewportScaleRules::Gizmo);
}

void UGizmoVisualComponent::ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth, float ViewportHeight)
{
	float NewScale = ComputeScreenSpaceScale(CameraLocation, bIsOrtho, OrthoWidth, ViewportHeight);
	SetRelativeScale(FVector(NewScale, NewScale, NewScale));
}

void UGizmoVisualComponent::SetWorldSpace(bool bWorldSpace)
{
	bIsWorldSpace = bWorldSpace;
	UpdateVisualTransform();
}

uint32 UGizmoVisualComponent::ComputeAxisMask(ELevelViewportType ViewportType, EGizmoMode Mode)
{
	constexpr uint32 AllAxes = 0x7;
	uint32 ViewAxis = AllAxes;

	switch (ViewportType)
	{
	case ELevelViewportType::Top:
	case ELevelViewportType::Bottom:
		ViewAxis = 0x4; break; // Z
	case ELevelViewportType::Front:
	case ELevelViewportType::Back:
		ViewAxis = 0x1; break; // X
	case ELevelViewportType::Left:
	case ELevelViewportType::Right:
		ViewAxis = 0x2; break; // Y
	default: break;
	}

	if (ViewAxis == AllAxes)
		return AllAxes;

	if (Mode == EGizmoMode::Rotate)
		return ViewAxis;            // Rotate: 시선 축만

	return AllAxes & ~ViewAxis;     // Translate/Scale: 시선 축 제외
}


FQuat UGizmoVisualComponent::GetVisualRotationQuat() const
{
    if (!bHasVisualTarget)
    {
        return FQuat::Identity;
    }

    // World space는 T/R/S 모두 월드 축 정렬, Local space는 대상 로컬 축 정렬.
    if (!bIsWorldSpace)
    {
        return VisualWorldTransform.Rotation.GetNormalized();
    }

    return FQuat::Identity;
}

FMatrix UGizmoVisualComponent::GetVisualRotationMatrix() const
{
    return GetVisualRotationQuat().ToMatrix();
}

void UGizmoVisualComponent::Deactivate()
{
    ResetVisualInteractionState();
    ClearGizmoWorldTransform();
}

void UGizmoVisualComponent::ResetVisualInteractionState()
{
    bIsHolding = false;
    bPressedOnHandle = false;
    SelectedAxis = -1;
}

FMeshBuffer* UGizmoVisualComponent::GetMeshBuffer() const
{
	EMeshShape Shape = EMeshShape::TransGizmo;
	switch (CurMode)
	{
	case EGizmoMode::Translate:
		break;
	case EGizmoMode::Rotate:
		Shape = EMeshShape::RotGizmo;
		break;
	case EGizmoMode::Scale:
		Shape = EMeshShape::ScaleGizmo;
		break;
	case EGizmoMode::Select:
		break;
	}
	return &FMeshBufferManager::Get().GetMeshBuffer(Shape);
}
