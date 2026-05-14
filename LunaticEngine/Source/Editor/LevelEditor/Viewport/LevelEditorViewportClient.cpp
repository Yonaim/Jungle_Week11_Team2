#include "PCH/LunaticPCH.h"
#include "LevelEditor/Viewport/LevelEditorViewportClient.h"

#include "Core/ProjectSettings.h"
#include "LevelEditor/Subsystem/OverlayStatSystem.h"
#include "LevelEditor/UI/Panels/LevelConsolePanel.h"
#include "LevelEditor/Settings/LevelEditorSettings.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputModifier.h"
#include "Engine/Input/InputTrigger.h"
#include "Engine/Profiling/PlatformTime.h"
#include "Engine/Runtime/WindowsWindow.h"


#include "Engine/Runtime/Engine.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Math/Vector.h"
#include "Viewport/Viewport.h"


#include "Collision/RayUtils.h"
#include "Component/BillboardComponent.h"
#include "Component/GizmoVisualComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/MeshComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/UIImageComponent.h"
#include "Component/UIScreenTextComponent.h"
#include "Common/UI/Style/AccentColor.h"
#include "EditorEngine.h"
#include "LevelEditor/Selection/SelectionManager.h"
#include "GameFramework/AActor.h"
#include "ImGui/imgui.h"
#include "Object/Object.h"
#include "Viewport/GameViewportClient.h"


#include <cfloat>

UWorld *FLevelEditorViewportClient::GetWorld() const
{
    return GEngine ? GEngine->GetWorld() : nullptr;
}

const char *FLevelEditorViewportClient::GetViewportTooltipBarText() const
{
    return nullptr;
}

namespace
{
void SyncPIEViewportRect(FViewport *Viewport, const FRect &ViewportScreenRect)
{
    if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
    {
        if (EditorEngine->IsPlayingInEditor())
        {
            if (UGameViewportClient *GameViewportClient = EditorEngine->GetGameViewportClient())
            {
                if (GameViewportClient->GetViewport() == Viewport)
                {
                    GameViewportClient->SetViewport(Viewport);
                    GameViewportClient->SetCursorClipRect(ViewportScreenRect);
                }
            }
        }
    }
}

struct FCameraBookmark
{
    FVector Location;
    FRotator Rotation;
    bool bValid = false;
};
static FCameraBookmark GCameraBookmarks[10];

enum class EUIScreenGizmoAxis : int32
{
    None = 0,
    X = 1,
    Y = 2,
    XY = 3
};

bool IsUIComponentSelectable(const UActorComponent *Component)
{
    return Component && !Component->IsHiddenInComponentTree() && !Component->IsEditorOnlyComponent() &&
           Component->SupportsUIScreenPicking();
}

bool IsLevelEditorPickablePrimitive(const UPrimitiveComponent *Primitive)
{
    if (!Primitive || !Primitive->IsVisible())
    {
        return false;
    }

    if (!Primitive->ParticipatesInPickingSpatialStructure())
    {
        return false;
    }

    return true;
}

bool HasLevelEditorPickablePrimitive(const AActor *Actor)
{
    if (!Actor || !Actor->IsVisible())
    {
        return false;
    }

    for (UActorComponent *Component : Actor->GetComponents())
    {
        const UPrimitiveComponent *Primitive = Cast<UPrimitiveComponent>(Component);
        if (IsLevelEditorPickablePrimitive(Primitive))
        {
            return true;
        }
    }

    return false;
}

USceneComponent *FindTopmostUIComponentAt(UWorld *World, float X, float Y)
{
    if (!World)
    {
        return nullptr;
    }

    USceneComponent *BestComponent = nullptr;
    int32 BestZOrder = INT_MIN;

    for (AActor *Actor : World->GetActors())
    {
        if (!Actor || !Actor->IsVisible())
        {
            continue;
        }

        for (UActorComponent *Component : Actor->GetComponents())
        {
            if (!IsUIComponentSelectable(Component))
            {
                continue;
            }

            USceneComponent *SceneComponent = Cast<USceneComponent>(Component);
            if (!SceneComponent || !Component->HitTestUIScreenPoint(X, Y))
            {
                continue;
            }

            const int32 ZOrder = Component->GetUIScreenPickingZOrder();
            if (!BestComponent || ZOrder >= BestZOrder)
            {
                BestComponent = SceneComponent;
                BestZOrder = ZOrder;
            }
        }
    }

    return BestComponent;
}

bool IsUIScreenTransformableComponent(const USceneComponent *Component)
{
    return Cast<UUIImageComponent>(Component) || Cast<UUIScreenTextComponent>(Component);
}

bool GetUIScreenTextBounds(const UUIScreenTextComponent *TextComponent, float &OutX, float &OutY, float &OutWidth,
                           float &OutHeight)
{
    return TextComponent && TextComponent->GetResolvedScreenBounds(OutX, OutY, OutWidth, OutHeight);
}

bool GetUIScreenComponentBounds(const USceneComponent *Component, float &OutX, float &OutY, float &OutWidth,
                                float &OutHeight)
{
    if (const UUIImageComponent *ImageComponent = Cast<UUIImageComponent>(Component))
    {
        const FVector2 ResolvedPosition = ImageComponent->GetResolvedScreenPosition();
        const FVector2 ResolvedSize = ImageComponent->GetResolvedScreenSize();
        OutX = ResolvedPosition.X;
        OutY = ResolvedPosition.Y;
        OutWidth = (std::max)(1.0f, ResolvedSize.X);
        OutHeight = (std::max)(1.0f, ResolvedSize.Y);
        return true;
    }

    if (const UUIScreenTextComponent *TextComponent = Cast<UUIScreenTextComponent>(Component))
    {
        return GetUIScreenTextBounds(TextComponent, OutX, OutY, OutWidth, OutHeight);
    }

    return false;
}

bool GetUIScreenComponentPosition(const USceneComponent *Component, FVector &OutPosition)
{
    if (const UUIImageComponent *ImageComponent = Cast<UUIImageComponent>(Component))
    {
        const FVector2 ResolvedPosition = ImageComponent->GetResolvedScreenPosition();
        OutPosition = FVector(ResolvedPosition.X, ResolvedPosition.Y, ImageComponent->GetScreenPosition().Z);
        return true;
    }

    if (const UUIScreenTextComponent *TextComponent = Cast<UUIScreenTextComponent>(Component))
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Width = 0.0f;
        float Height = 0.0f;
        if (!TextComponent->GetResolvedScreenBounds(X, Y, Width, Height))
        {
            return false;
        }

        OutPosition = FVector(X, Y, TextComponent->GetScreenPosition().Z);
        return true;
    }

    return false;
}

bool SetUIScreenComponentPosition(USceneComponent *Component, const FVector &InPosition)
{
    if (UUIImageComponent *ImageComponent = Cast<UUIImageComponent>(Component))
    {
        if (ImageComponent->IsAnchoredLayoutEnabled())
        {
            const FVector2 CurrentResolvedPosition = ImageComponent->GetResolvedScreenPosition();
            FVector AnchorOffset = ImageComponent->GetAnchorOffset();
            AnchorOffset.X += InPosition.X - CurrentResolvedPosition.X;
            AnchorOffset.Y += InPosition.Y - CurrentResolvedPosition.Y;
            ImageComponent->SetAnchorOffset(AnchorOffset);
            return true;
        }

        ImageComponent->SetScreenPosition(InPosition);
        return true;
    }

    if (UUIScreenTextComponent *TextComponent = Cast<UUIScreenTextComponent>(Component))
    {
        if (TextComponent->IsAnchoredLayoutEnabled())
        {
            float CurrentX = 0.0f;
            float CurrentY = 0.0f;
            float CurrentWidth = 0.0f;
            float CurrentHeight = 0.0f;
            if (!TextComponent->GetResolvedScreenBounds(CurrentX, CurrentY, CurrentWidth, CurrentHeight))
            {
                return false;
            }

            FVector AnchorOffset = TextComponent->GetAnchorOffset();
            AnchorOffset.X += InPosition.X - CurrentX;
            AnchorOffset.Y += InPosition.Y - CurrentY;
            TextComponent->SetAnchorOffset(AnchorOffset);
            return true;
        }

        TextComponent->SetScreenPosition(InPosition);
        return true;
    }

    return false;
}

bool TryConvertMouseToViewportPixel(const ImVec2 &MousePos, const FRect &ViewportScreenRect, const FViewport *Viewport,
                                    float FallbackWidth, float FallbackHeight, float &OutViewportX, float &OutViewportY)
{
    if (ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
    {
        return false;
    }

    const float LocalX = MousePos.x - ViewportScreenRect.X;
    const float LocalY = MousePos.y - ViewportScreenRect.Y;

    // Drag 중 뷰포트 밖으로 나가도 같은 screen-rect -> render-target 변환식을 유지한다.
    // 여기서 false를 반환하고 raw local 좌표로 fallback하면 viewport rect와 RT 크기가 다를 때 delta가 튄다.
    const float TargetWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : FallbackWidth;
    const float TargetHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : FallbackHeight;
    if (TargetWidth <= 0.0f || TargetHeight <= 0.0f)
    {
        return false;
    }

    const float ScaleX = TargetWidth / ViewportScreenRect.Width;
    const float ScaleY = TargetHeight / ViewportScreenRect.Height;
    OutViewportX = LocalX * ScaleX;
    OutViewportY = LocalY * ScaleY;
    return true;
}

bool TryConvertViewportPixelToScreenPoint(float ViewportX, float ViewportY, const FRect &ViewportScreenRect,
                                          const FViewport *Viewport, float FallbackWidth, float FallbackHeight,
                                          float &OutScreenX, float &OutScreenY)
{
    if (ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
    {
        return false;
    }

    const float TargetWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : FallbackWidth;
    const float TargetHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : FallbackHeight;
    if (TargetWidth <= 0.0f || TargetHeight <= 0.0f)
    {
        return false;
    }

    const float ScaleX = ViewportScreenRect.Width / TargetWidth;
    const float ScaleY = ViewportScreenRect.Height / TargetHeight;
    OutScreenX = ViewportScreenRect.X + ViewportX * ScaleX;
    OutScreenY = ViewportScreenRect.Y + ViewportY * ScaleY;
    return true;
}

bool ProjectWorldToViewport(const FMatrix &ViewProjection, const FVector &WorldPosition, float ViewportWidth,
                            float ViewportHeight, float &OutScreenX, float &OutScreenY, float &OutDepth)
{
    const FVector ClipSpace = ViewProjection.TransformPositionWithW(WorldPosition);
    OutScreenX = (ClipSpace.X * 0.5f + 0.5f) * ViewportWidth;
    OutScreenY = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * ViewportHeight;
    OutDepth = ClipSpace.Z;
    return std::isfinite(OutScreenX) && std::isfinite(OutScreenY) && std::isfinite(OutDepth);
}

bool EditorRaycastAllVisiblePrimitives(UWorld *World, const FRay &Ray, FRayHitResult &OutHitResult, AActor *&OutActor)
{
    FRayHitResult BestHit{};
    AActor *BestActor = nullptr;

    if (!World)
    {
        return false;
    }

    for (AActor *Actor : World->GetActors())
    {
        if (!Actor || !Actor->IsVisible())
        {
            continue;
        }

        for (UActorComponent *Component : Actor->GetComponents())
        {
            UPrimitiveComponent *Primitive = Cast<UPrimitiveComponent>(Component);
            if (!IsLevelEditorPickablePrimitive(Primitive))
            {
                continue;
            }

            FRayHitResult CandidateHit{};
            if (!Primitive->LineTraceComponent(Ray, CandidateHit))
            {
                float AABBTMin = 0.0f;
                float AABBTMax = 0.0f;
                const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
                if (!Bounds.IsValid() || !FRayUtils::IntersectRayAABB(Ray, Bounds.Min, Bounds.Max, AABBTMin, AABBTMax))
                {
                    continue;
                }

                CandidateHit.HitComponent = Primitive;
                CandidateHit.Distance = AABBTMin >= 0.0f ? AABBTMin : AABBTMax;
                CandidateHit.WorldHitLocation = Ray.Origin + Ray.Direction * CandidateHit.Distance;
                CandidateHit.bHit = true;
            }

            if (CandidateHit.Distance < BestHit.Distance)
            {
                BestHit = CandidateHit;
                BestActor = Actor;
            }
        }
    }

    if (!BestActor)
    {
        return false;
    }

    OutHitResult = BestHit;
    OutActor = BestActor;
    return true;
}

void BuildBoundingBoxCorners(const FBoundingBox &Bounds, FVector OutCorners[8])
{
    const FVector &Min = Bounds.Min;
    const FVector &Max = Bounds.Max;
    OutCorners[0] = FVector(Min.X, Min.Y, Min.Z);
    OutCorners[1] = FVector(Max.X, Min.Y, Min.Z);
    OutCorners[2] = FVector(Min.X, Max.Y, Min.Z);
    OutCorners[3] = FVector(Max.X, Max.Y, Min.Z);
    OutCorners[4] = FVector(Min.X, Min.Y, Max.Z);
    OutCorners[5] = FVector(Max.X, Min.Y, Max.Z);
    OutCorners[6] = FVector(Min.X, Max.Y, Max.Z);
    OutCorners[7] = FVector(Max.X, Max.Y, Max.Z);
}

AActor *FindScreenSpacePrimitiveAt(UWorld *World, const FEditorViewportCamera *Camera, float MouseViewportX,
                                   float MouseViewportY, float ViewportWidth, float ViewportHeight,
                                   UPrimitiveComponent *&OutPrimitive)
{
    OutPrimitive = nullptr;
    if (!World || !Camera || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
    {
        return nullptr;
    }

    const FMatrix ViewProjection = Camera->GetViewProjectionMatrix();
    AActor *BestActor = nullptr;
    UPrimitiveComponent *BestPrimitive = nullptr;
    float BestArea = FLT_MAX;
    float BestDepth = FLT_MAX;
    float BestScore = FLT_MAX;
    constexpr float ScreenPickPadding = 12.0f;
    constexpr float CenterPickRadiusSq = 24.0f * 24.0f;

    for (AActor *Actor : World->GetActors())
    {
        if (!Actor || !Actor->IsVisible())
        {
            continue;
        }

        for (UActorComponent *Component : Actor->GetComponents())
        {
            UPrimitiveComponent *Primitive = Cast<UPrimitiveComponent>(Component);
            if (!IsLevelEditorPickablePrimitive(Primitive))
            {
                continue;
            }

            // 이 함수는 raycast가 실패한 뒤 screen-space helper를 보조 선택하기 위한 fallback이다.
            // StaticMesh까지 여기서 AABB screen rect로 잡으면 빈 공간 클릭도 hit로 오인되어 선택 해제가 막힌다.
            const UBillboardComponent *Billboard = Cast<UBillboardComponent>(Primitive);
            if (!Billboard || !Billboard->IsEditorOnlyComponent())
            {
                continue;
            }

            const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
            if (!Bounds.IsValid())
            {
                continue;
            }

            FVector Corners[8];
            BuildBoundingBoxCorners(Bounds, Corners);

            float MinX = FLT_MAX;
            float MinY = FLT_MAX;
            float MaxX = -FLT_MAX;
            float MaxY = -FLT_MAX;
            float MinDepth = FLT_MAX;
            bool bProjectedAny = false;

            for (const FVector &Corner : Corners)
            {
                float ScreenX = 0.0f;
                float ScreenY = 0.0f;
                float Depth = 0.0f;
                if (!ProjectWorldToViewport(ViewProjection, Corner, ViewportWidth, ViewportHeight, ScreenX, ScreenY,
                                            Depth))
                {
                    continue;
                }

                MinX = (std::min)(MinX, ScreenX);
                MinY = (std::min)(MinY, ScreenY);
                MaxX = (std::max)(MaxX, ScreenX);
                MaxY = (std::max)(MaxY, ScreenY);
                MinDepth = (std::min)(MinDepth, Depth);
                bProjectedAny = true;
            }

            if (!bProjectedAny)
            {
                continue;
            }

            const float ExpandedMinX = MinX - ScreenPickPadding;
            const float ExpandedMaxX = MaxX + ScreenPickPadding;
            const float ExpandedMinY = MinY - ScreenPickPadding;
            const float ExpandedMaxY = MaxY + ScreenPickPadding;
            if (MouseViewportX < ExpandedMinX || MouseViewportX > ExpandedMaxX || MouseViewportY < ExpandedMinY ||
                MouseViewportY > ExpandedMaxY)
            {
                continue;
            }

            const float Area = (std::max)(1.0f, MaxX - MinX) * (std::max)(1.0f, MaxY - MinY);
            const float ClampedX = (std::max)(MinX, (std::min)(MouseViewportX, MaxX));
            const float ClampedY = (std::max)(MinY, (std::min)(MouseViewportY, MaxY));
            const float DistanceSq = (MouseViewportX - ClampedX) * (MouseViewportX - ClampedX) +
                                     (MouseViewportY - ClampedY) * (MouseViewportY - ClampedY);
            if (!BestActor || DistanceSq < BestScore ||
                (std::abs(DistanceSq - BestScore) < 1.0f &&
                 (Area < BestArea || (std::abs(Area - BestArea) < 1.0f && MinDepth < BestDepth))))
            {
                BestActor = Actor;
                BestPrimitive = Primitive;
                BestScore = DistanceSq;
                BestArea = Area;
                BestDepth = MinDepth;
            }
        }
    }

    if (!BestActor)
    {
        for (AActor *Actor : World->GetActors())
        {
            if (!HasLevelEditorPickablePrimitive(Actor))
            {
                continue;
            }

            bool bHasEditorBillboard = false;
            for (UActorComponent *Component : Actor->GetComponents())
            {
                const UBillboardComponent *Billboard = Cast<UBillboardComponent>(Component);
                if (Billboard && Billboard->IsEditorOnlyComponent() && IsLevelEditorPickablePrimitive(Billboard))
                {
                    bHasEditorBillboard = true;
                    break;
                }
            }
            if (!bHasEditorBillboard)
            {
                continue;
            }

            float ScreenX = 0.0f;
            float ScreenY = 0.0f;
            float Depth = 0.0f;
            if (!ProjectWorldToViewport(ViewProjection, Actor->GetActorLocation(), ViewportWidth, ViewportHeight,
                                        ScreenX, ScreenY, Depth))
            {
                continue;
            }

            const float DistanceSq = (MouseViewportX - ScreenX) * (MouseViewportX - ScreenX) +
                                     (MouseViewportY - ScreenY) * (MouseViewportY - ScreenY);
            if (DistanceSq > CenterPickRadiusSq)
            {
                continue;
            }

            BestActor = Actor;
            BestScore = DistanceSq;
            BestDepth = Depth;
            for (UActorComponent *Component : Actor->GetComponents())
            {
                UBillboardComponent *Billboard = Cast<UBillboardComponent>(Component);
                if (Billboard && Billboard->IsEditorOnlyComponent() && IsLevelEditorPickablePrimitive(Billboard))
                {
                    BestPrimitive = Billboard;
                    break;
                }
            }
            break;
        }
    }

    OutPrimitive = BestPrimitive;
    return BestActor;
}
} // namespace

namespace
{
bool IsAnyEditorMouseButtonDownForContextSwitchGuard()
{
    FInputManager& Input = FInputManager::Get();
    return Input.IsMouseButtonDown(FInputManager::MOUSE_LEFT) ||
           Input.IsMouseButtonDown(FInputManager::MOUSE_RIGHT) ||
           ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
           ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
           ImGui::IsMouseDown(ImGuiMouseButton_Middle);
}
}

FLevelEditorViewportClient::FLevelEditorViewportClient()
{
    SetupInput();
}

FLevelEditorViewportClient::~FLevelEditorViewportClient()
{
    EnhancedInputManager.ClearBindings();
    EnhancedInputManager.ClearAllMappingContexts();

    if (EditorMappingContext)
    {
        for (auto &Mapping : EditorMappingContext->Mappings)
        {
            for (auto *Trigger : Mapping.Triggers)
                delete Trigger;
            for (auto *Modifier : Mapping.Modifiers)
                delete Modifier;
        }
        delete EditorMappingContext;
    }

    delete ActionEditorDelete;
    delete ActionEditorDuplicate;
    delete ActionEditorTogglePIE;
    delete ActionEditorVertexSnap;
    delete ActionEditorSnapToFloor;
    delete ActionEditorSetBookmark;
    delete ActionEditorJumpToBookmark;
    delete ActionEditorSetViewportPerspective;
    delete ActionEditorSetViewportTop;
    delete ActionEditorSetViewportFront;
    delete ActionEditorSetViewportRight;
}

void FLevelEditorViewportClient::Init(FWindowsWindow *InWindow)
{
    FEditorViewportClient::Init(InWindow);
    EnsureEditorGizmo();
}

void FLevelEditorViewportClient::Shutdown()
{
    bIsMarqueeSelecting = false;
    bNeedsDeferredTargetSync = false;
    CommonInput.InputState.ResetFrame();
    GizmoManager.GetUIScreenInteractionState().Reset();
    ReleaseEditorGizmo();
    FEditorViewportClient::Shutdown();
}

void FLevelEditorViewportClient::ActivateEditorContext()
{
    FEditorViewportClient::ActivateEditorContext();
    EnsureEditorGizmo();

    // Do not reconnect a transform target in the same frame as a tab/context switch.
    // The next live tick will sync selection -> gizmo after mouse capture has settled.
    bNeedsDeferredTargetSync = true;
    bIsMarqueeSelecting = false;
    CommonInput.InputState.ResetFrame();
    GizmoManager.ResetVisualInteractionState();
}

void FLevelEditorViewportClient::DeactivateEditorContext()
{
    // Keep selection/camera/viewport state, but detach every live interaction.
    bNeedsDeferredTargetSync = false;
    bIsMarqueeSelecting = false;
    CommonInput.InputState.ResetFrame();
    CurrentGizmoTargetComponent = nullptr;
    FEditorViewportClient::DeactivateEditorContext();
}

void FLevelEditorViewportClient::EnsureEditorGizmo()
{
    GizmoManager.SetInteractionPolicy(EGizmoInteractionPolicy::Interactive);
    GizmoManager.EnsureVisualComponent();
    GizmoManager.SetVisualWorldLocation(FVector(0.0f, 0.0f, 0.0f));
}

void FLevelEditorViewportClient::ReleaseEditorGizmo()
{
    UnregisterGizmoFromScene();
    GizmoManager.ReleaseVisualComponent();
}

void FLevelEditorViewportClient::DetachSceneResourcesForWorldChange()
{
    // NEW SCENE / LOAD SCENE처럼 World가 교체될 때는 UWorld 내부의 FScene이 먼저 파괴될 수 있다.
    // Transform GizmoVisual는 editor-only proxy를 FScene에 직접 등록하므로, World 파괴 전에 반드시 제거해야 한다.
    UnregisterGizmoFromScene();

    CurrentGizmoTargetComponent = nullptr;
    GizmoManager.ClearTarget();
}

void FLevelEditorViewportClient::UnregisterGizmoFromScene()
{
    // Editor gizmo must never survive across PIE world swaps.
    // The visual component is owned by FGizmoManager, so scene unregistration also goes through it.
    GizmoManager.UnregisterVisualFromScene();
    RegisteredGizmoScene = nullptr;
}

void FLevelEditorViewportClient::RegisterGizmoToScene(FScene* Scene)
{
    if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
    {
        if (EditorEngine->IsPlayingInEditor())
        {
            // PIE uses the game camera/input path. The editor transform gizmo is editor-only,
            // so keep it unregistered and invisible for both possessed and ejected PIE modes.
            UnregisterGizmoFromScene();
            return;
        }
    }

    EnsureEditorGizmo();
    if (!Scene)
    {
        return;
    }

    // 같은 Scene이어도 manager에 재등록을 요청한다.
    // Gizmo mode 변경은 mesh buffer 교체를 위해 MarkRenderStateDirty()를 타고,
    // 이 과정에서 proxy가 잠시 파괴될 수 있다. manager의 RegisterVisualToScene()은
    // idempotent하므로 매 render request마다 호출해 stale proxy 상태를 복구한다.
    GizmoManager.RegisterVisualToScene(Scene);
    RegisteredGizmoScene = Scene;
}

void FLevelEditorViewportClient::SetupInput()
{
    // 1. Create Actions
    CommonInput.CreateCommonActions();

    ActionEditorDelete = new FInputAction("IA_EditorDelete", EInputActionValueType::Bool);
    ActionEditorDuplicate = new FInputAction("IA_EditorDuplicate", EInputActionValueType::Bool);
    ActionEditorTogglePIE = new FInputAction("IA_EditorTogglePIE", EInputActionValueType::Bool);
    ActionEditorVertexSnap = new FInputAction("IA_EditorVertexSnap", EInputActionValueType::Bool);
    ActionEditorSnapToFloor = new FInputAction("IA_EditorSnapToFloor", EInputActionValueType::Bool);
    ActionEditorSetBookmark = new FInputAction("IA_EditorSetBookmark", EInputActionValueType::Float);
    ActionEditorJumpToBookmark = new FInputAction("IA_EditorJumpToBookmark", EInputActionValueType::Float);

    ActionEditorSetViewportPerspective = new FInputAction("IA_SetViewportPerspective", EInputActionValueType::Bool);
    ActionEditorSetViewportTop = new FInputAction("IA_SetViewportTop", EInputActionValueType::Bool);
    ActionEditorSetViewportFront = new FInputAction("IA_SetViewportFront", EInputActionValueType::Bool);
    ActionEditorSetViewportRight = new FInputAction("IA_SetViewportRight", EInputActionValueType::Bool);

    // 2. Create Mapping Context
    EditorMappingContext = new FInputMappingContext();
    EditorMappingContext->ContextName = "IMC_Editor";

    // Move: WASD QE
    EditorMappingContext->AddMapping(CommonInput.Move, 'W');
    EditorMappingContext->AddMapping(CommonInput.Move, 'S').Modifiers.push_back(new FModifierScale(FVector(-1, 1, 1)));
    EditorMappingContext->AddMapping(CommonInput.Move, 'D')
        .Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
    EditorMappingContext->AddMapping(CommonInput.Move, 'A')
        .Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
    EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, -1, 1)));
    EditorMappingContext->AddMapping(CommonInput.Move, 'E')
        .Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
    EditorMappingContext->AddMapping(CommonInput.Move, 'Q')
        .Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
    EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, 1, -1)));

    // Rotate: Arrows + Right Mouse
    EditorMappingContext->AddMapping(CommonInput.Rotate, VK_UP)
        .Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
    EditorMappingContext->AddMapping(CommonInput.Rotate, VK_DOWN)
        .Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
    EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, -1, 1)));
    EditorMappingContext->AddMapping(CommonInput.Rotate, VK_LEFT)
        .Modifiers.push_back(new FModifierScale(FVector(-1, 1, 1)));
    EditorMappingContext->AddMapping(CommonInput.Rotate, VK_RIGHT);

    // Mouse Rotate
    EditorMappingContext->AddMapping(CommonInput.Rotate, static_cast<int32>(EInputKey::MouseX));
    EditorMappingContext->AddMapping(CommonInput.Rotate, static_cast<int32>(EInputKey::MouseY))
        .Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

    // Pan: Middle Mouse
    EditorMappingContext->AddMapping(CommonInput.Pan, static_cast<int32>(EInputKey::MouseX));
    EditorMappingContext->AddMapping(CommonInput.Pan, static_cast<int32>(EInputKey::MouseY))
        .Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

    // Zoom: Wheel
    EditorMappingContext->AddMapping(CommonInput.Zoom, static_cast<int32>(EInputKey::MouseWheel));

    // Orbit: Alt + Left Mouse
    EditorMappingContext->AddMapping(CommonInput.Orbit, static_cast<int32>(EInputKey::MouseX));
    EditorMappingContext->AddMapping(CommonInput.Orbit, static_cast<int32>(EInputKey::MouseY))
        .Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

    // --- Shortcuts ---
    EditorMappingContext->AddMapping(CommonInput.Focus, 'F');
    EditorMappingContext->AddMapping(ActionEditorDelete, VK_DELETE);
    EditorMappingContext->AddMapping(ActionEditorDuplicate, 'D');
    EditorMappingContext->AddMapping(CommonInput.ToggleGizmoMode, VK_SPACE);
    EditorMappingContext->AddMapping(CommonInput.ToggleCoordSystem, 'X');
    EditorMappingContext->AddMapping(CommonInput.Escape, VK_ESCAPE);
    EditorMappingContext->AddMapping(ActionEditorTogglePIE, VK_F8);

    EditorMappingContext->AddMapping(CommonInput.DecreaseSnap, VK_OEM_4); // [
    EditorMappingContext->AddMapping(CommonInput.IncreaseSnap, VK_OEM_6); // ]
    EditorMappingContext->AddMapping(ActionEditorVertexSnap, 'V');
    EditorMappingContext->AddMapping(ActionEditorSnapToFloor, VK_END);

    // Viewport Type Shortcuts
    EditorMappingContext->AddMapping(ActionEditorSetViewportPerspective, 'G'); // Alt+G
    EditorMappingContext->AddMapping(ActionEditorSetViewportTop, 'J');         // Alt+J
    EditorMappingContext->AddMapping(ActionEditorSetViewportFront, 'H');       // Alt+H
    EditorMappingContext->AddMapping(ActionEditorSetViewportRight, 'K');       // Alt+K

    // Snap Toggles
    EditorMappingContext->AddMapping(CommonInput.ToggleGridSnap, 'G');     // Shift+G
    EditorMappingContext->AddMapping(CommonInput.ToggleRotationSnap, 'R'); // Shift+R
    EditorMappingContext->AddMapping(CommonInput.ToggleScaleSnap, 'S');    // Shift+S

    // Bookmarks 0-9
    for (int32 i = 0; i < 10; ++i)
    {
        int32 Key = '0' + i;
        EditorMappingContext->AddMapping(ActionEditorSetBookmark, Key)
            .Modifiers.push_back(new FModifierScale(FVector(static_cast<float>(i), 0, 0)));
        EditorMappingContext->AddMapping(ActionEditorJumpToBookmark, Key)
            .Modifiers.push_back(new FModifierScale(FVector(static_cast<float>(i), 0, 0)));
    }

    // UE Style GizmoVisual Shortcuts (QWER)
    EditorMappingContext->AddMapping(CommonInput.ToggleGizmoMode, 'Q'); // Select
    EditorMappingContext->AddMapping(CommonInput.ToggleGizmoMode, 'W'); // Translate
    EditorMappingContext->AddMapping(CommonInput.ToggleGizmoMode, 'E'); // Rotate
    EditorMappingContext->AddMapping(CommonInput.ToggleGizmoMode, 'R'); // Scale

    // Game View Toggle (G)
    FInputAction *ActionEditorToggleGameView = new FInputAction("IA_EditorToggleGameView", EInputActionValueType::Bool);
    EditorMappingContext->AddMapping(ActionEditorToggleGameView, 'G');
    EnhancedInputManager.BindAction(
        ActionEditorToggleGameView, ETriggerEvent::Started, [this](const FInputActionValue &V) {
            if (!FInputManager::Get().IsKeyDown(VK_MENU) && !FInputManager::Get().IsKeyDown(VK_SHIFT))
                RenderOptions.bGameView = !RenderOptions.bGameView;
        });

    // 3. Add Context
    EnhancedInputManager.AddMappingContext(EditorMappingContext, 0);

    // 4. Bind Actions
    EnhancedInputManager.BindAction(CommonInput.Move, ETriggerEvent::Triggered,
                                    [this](const FInputActionValue &V) { OnEditorMove(V); });
    EnhancedInputManager.BindAction(CommonInput.Rotate, ETriggerEvent::Triggered,
                                    [this](const FInputActionValue &V) { OnEditorRotate(V); });
    EnhancedInputManager.BindAction(CommonInput.Pan, ETriggerEvent::Triggered,
                                    [this](const FInputActionValue &V) { OnEditorPan(V); });
    EnhancedInputManager.BindAction(CommonInput.Zoom, ETriggerEvent::Triggered,
                                    [this](const FInputActionValue &V) { OnEditorZoom(V); });
    EnhancedInputManager.BindAction(CommonInput.Orbit, ETriggerEvent::Triggered,
                                    [this](const FInputActionValue &V) { OnEditorOrbit(V); });

    EnhancedInputManager.BindAction(CommonInput.Focus, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) { OnEditorFocus(V); });
    EnhancedInputManager.BindAction(ActionEditorDelete, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) { OnEditorDelete(V); });
    EnhancedInputManager.BindAction(ActionEditorDuplicate, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) { OnEditorDuplicate(V); });
    EnhancedInputManager.BindAction(CommonInput.ToggleGizmoMode, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) { OnEditorToggleGizmoMode(V); });
    EnhancedInputManager.BindAction(CommonInput.ToggleCoordSystem, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) { OnEditorToggleCoordSystem(V); });
    EnhancedInputManager.BindAction(CommonInput.Escape, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) { OnEditorEscape(V); });
    EnhancedInputManager.BindAction(ActionEditorTogglePIE, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) { OnEditorTogglePIE(V); });

    EnhancedInputManager.BindAction(CommonInput.DecreaseSnap, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) {
                                        FLevelEditorSettings &S = FLevelEditorSettings::Get();
                                        if (FInputManager::Get().IsKeyDown(VK_SHIFT))
                                            S.RotationSnapSize = (std::max)(1.0f, S.RotationSnapSize - 5.0f);
                                        else
                                            S.TranslationSnapSize = (std::max)(0.1f, S.TranslationSnapSize / 2.0f);
                                    });
    EnhancedInputManager.BindAction(CommonInput.IncreaseSnap, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) {
                                        FLevelEditorSettings &S = FLevelEditorSettings::Get();
                                        if (FInputManager::Get().IsKeyDown(VK_SHIFT))
                                            S.RotationSnapSize = (std::min)(90.0f, S.RotationSnapSize + 5.0f);
                                        else
                                            S.TranslationSnapSize = (std::min)(1000.0f, S.TranslationSnapSize * 2.0f);
                                    });

    EnhancedInputManager.BindAction(
        ActionEditorSnapToFloor, ETriggerEvent::Started, [this](const FInputActionValue &V) {
            if (SelectionManager)
            {
                UWorld *W = GetWorld();
                for (AActor *Actor : SelectionManager->GetSelectedActors())
                {
                    if (!Actor)
                        continue;
                    FRay DownRay(Actor->GetActorLocation(), FVector(0, 0, -1));
                    FRayHitResult Hit;
                    AActor *HitActor = nullptr;
                    if (W && W->RaycastPrimitives(DownRay, Hit, HitActor))
                    {
                        Actor->SetActorLocation(DownRay.Origin + DownRay.Direction * Hit.Distance);
                    }
                }
                GizmoManager.SyncVisualFromTarget();
            }
        });

    EnhancedInputManager.BindAction(
        ActionEditorSetBookmark, ETriggerEvent::Started, [this](const FInputActionValue &V) {
            if (FInputManager::Get().IsKeyDown(VK_CONTROL) && GetCamera())
            {
                int32 Index = static_cast<int32>(V.Get());
                if (Index >= 0 && Index < 10)
                    GCameraBookmarks[Index] = {GetCamera()->GetWorldLocation(), GetCamera()->GetRelativeRotation(), true};
            }
        });

    EnhancedInputManager.BindAction(ActionEditorJumpToBookmark, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) {
                                        if (!FInputManager::Get().IsKeyDown(VK_CONTROL) && GetCamera())
                                        {
                                            int32 Index = static_cast<int32>(V.Get());
                                            if (Index >= 0 && Index < 10 && GCameraBookmarks[Index].bValid)
                                            {
                                                const auto &BM = GCameraBookmarks[Index];
                                                GetCameraController().StartFocusToTransform(BM.Location, BM.Rotation, FocusAnimDuration);
                                            }
                                        }
                                    });

    EnhancedInputManager.BindAction(ActionEditorSetViewportPerspective, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) {
                                        if (FInputManager::Get().IsKeyDown(VK_MENU))
                                            SetViewportType(ELevelViewportType::Perspective);
                                    });
    EnhancedInputManager.BindAction(ActionEditorSetViewportTop, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) {
                                        if (FInputManager::Get().IsKeyDown(VK_MENU))
                                            SetViewportType(ELevelViewportType::Top);
                                    });
    EnhancedInputManager.BindAction(ActionEditorSetViewportFront, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) {
                                        if (FInputManager::Get().IsKeyDown(VK_MENU))
                                            SetViewportType(ELevelViewportType::Front);
                                    });
    EnhancedInputManager.BindAction(ActionEditorSetViewportRight, ETriggerEvent::Started,
                                    [this](const FInputActionValue &V) {
                                        if (FInputManager::Get().IsKeyDown(VK_MENU))
                                            SetViewportType(ELevelViewportType::Right);
                                    });

    EnhancedInputManager.BindAction(
        CommonInput.ToggleGridSnap, ETriggerEvent::Started, [this](const FInputActionValue &V) {
            if (FInputManager::Get().IsKeyDown(VK_SHIFT))
                FLevelEditorSettings::Get().bEnableTranslationSnap = !FLevelEditorSettings::Get().bEnableTranslationSnap;
        });
    EnhancedInputManager.BindAction(
        CommonInput.ToggleRotationSnap, ETriggerEvent::Started, [this](const FInputActionValue &V) {
            if (FInputManager::Get().IsKeyDown(VK_SHIFT))
                FLevelEditorSettings::Get().bEnableRotationSnap = !FLevelEditorSettings::Get().bEnableRotationSnap;
        });
    EnhancedInputManager.BindAction(
        CommonInput.ToggleScaleSnap, ETriggerEvent::Started, [this](const FInputActionValue &V) {
            if (FInputManager::Get().IsKeyDown(VK_SHIFT))
                FLevelEditorSettings::Get().bEnableScaleSnap = !FLevelEditorSettings::Get().bEnableScaleSnap;
        });
}

void FLevelEditorViewportClient::OnEditorMove(const FInputActionValue &Value)
{
    if (FInputManager::Get().IsKeyDown(VK_CONTROL))
        return;
    if (FInputManager::Get().IsMouseButtonDown(VK_RBUTTON))
    {
        CommonInput.InputState.MoveAccumulator = CommonInput.InputState.MoveAccumulator + Value.GetVector();
    }
}

void FLevelEditorViewportClient::OnEditorRotate(const FInputActionValue &Value)
{
    FInputManager &Input = FInputManager::Get();
    if (Input.IsMouseButtonDown(VK_RBUTTON))
    {
        CommonInput.InputState.RotateAccumulator = CommonInput.InputState.RotateAccumulator + Value.GetVector();
    }
    else
    {
        FVector KeyboardRotate(0, 0, 0);
        if (Input.IsKeyDown(VK_RIGHT))
            KeyboardRotate.X += 1.0f;
        if (Input.IsKeyDown(VK_LEFT))
            KeyboardRotate.X -= 1.0f;
        if (Input.IsKeyDown(VK_UP))
            KeyboardRotate.Y += 1.0f;
        if (Input.IsKeyDown(VK_DOWN))
            KeyboardRotate.Y -= 1.0f;
        if (!KeyboardRotate.IsNearlyZero())
        {
            CommonInput.InputState.RotateAccumulator = CommonInput.InputState.RotateAccumulator + KeyboardRotate;
        }
    }
}

void FLevelEditorViewportClient::OnEditorPan(const FInputActionValue &Value)
{
    if (FInputManager::Get().IsMouseButtonDown(VK_MBUTTON))
    {
        CommonInput.InputState.PanAccumulator = CommonInput.InputState.PanAccumulator + Value.GetVector();
    }
    else if (FInputManager::Get().IsKeyDown(VK_MENU) && FInputManager::Get().IsMouseButtonDown(VK_MBUTTON)) // Alt + MMB
    {
        CommonInput.InputState.PanAccumulator = CommonInput.InputState.PanAccumulator + Value.GetVector();
    }
}

void FLevelEditorViewportClient::OnEditorZoom(const FInputActionValue &Value)
{
    FInputManager &Input = FInputManager::Get();
    if (Input.IsMouseButtonDown(VK_RBUTTON))
    {
        float &Speed = FLevelEditorSettings::Get().CameraSpeed;
        Speed = Clamp(Speed + Value.Get() * 2.0f, 1.0f, 100.0f);
        return;
    }
    CommonInput.InputState.ZoomAccumulator += Value.Get();
}

void FLevelEditorViewportClient::OnEditorOrbit(const FInputActionValue &Value)
{
    FInputManager &Input = FInputManager::Get();
    if (Input.IsKeyDown(VK_MENU))
    {
        // Alt + LMB = Selection Orbit
        if (Input.IsMouseButtonDown(VK_LBUTTON))
        {
            if (SelectionManager && SelectionManager->GetPrimarySelection() && GetCamera())
            {
                FVector Pivot = SelectionManager->GetPrimarySelection()->GetActorLocation();
                FVector CameraPos = GetCamera()->GetWorldLocation();
                float Dist = FVector::Distance(CameraPos, Pivot);
                const float Sensitivity = 0.25f;
                FRotator Rotation = GetCamera()->GetRelativeRotation();
                Rotation.Yaw += Value.GetVector().X * Sensitivity;
                Rotation.Pitch = Clamp(Rotation.Pitch + Value.GetVector().Y * Sensitivity, -89.0f, 89.0f);
                FVector NewPos = Pivot - Rotation.ToVector() * (Dist > 0.1f ? Dist : 5.0f);
                GetCamera()->SetWorldLocation(NewPos);
                GetCamera()->SetRelativeRotation(Rotation);
                SyncCameraSmoothingTarget();
            }
            else
            {
                CommonInput.InputState.RotateAccumulator = CommonInput.InputState.RotateAccumulator + Value.GetVector();
            }
        }
        // Alt + RMB = Scrub Zoom
        else if (Input.IsMouseButtonDown(VK_RBUTTON))
        {
            if (GetCamera())
            {
                float ScrubSpeed = 0.05f;
                GetCameraController().AddForwardTargetDelta(Value.GetVector().X * ScrubSpeed);
            }
        }
    }
}

void FLevelEditorViewportClient::OnEditorFocus(const FInputActionValue &Value)
{
    if (FInputManager::Get().IsMouseButtonDown(VK_RBUTTON))
        return;

    if (!SelectionManager || !GetCamera())
        return;

    AActor *Selected = SelectionManager->GetPrimarySelection();
    if (!Selected)
        return;

    FVector TargetLoc = Selected->GetActorLocation();
    float FocusDistance = 5.0f;
    if (UPrimitiveComponent *RootPrim = Cast<UPrimitiveComponent>(Selected->GetRootComponent()))
    {
        FVector Extent = RootPrim->GetWorldBoundingBox().GetExtent();
        float MaxDim = (std::max)({Extent.X, Extent.Y, Extent.Z});
        FocusDistance = (std::max)(5.0f, MaxDim * 2.5f);
    }

    GetCameraController().StartFocus(TargetLoc, FocusDistance, FocusAnimDuration);
}


void FLevelEditorViewportClient::OnEditorDelete(const FInputActionValue &Value)
{
    if (SelectionManager)
        SelectionManager->DeleteSelectedActors();
}

void FLevelEditorViewportClient::OnEditorDuplicate(const FInputActionValue &Value)
{
    if (SelectionManager && FInputManager::Get().IsKeyDown(VK_CONTROL))
    {
        const TArray<AActor *> ToDuplicate = SelectionManager->GetSelectedActors();
        if (!ToDuplicate.empty())
        {
            const FVector DuplicateOffsetStep(0.1f, 0.1f, 0.1f);
            TArray<AActor *> NewSelection;
            int32 DuplicateIndex = 0;
            for (AActor *Src : ToDuplicate)
            {
                if (!Src)
                    continue;
                AActor *Dup = Cast<AActor>(Src->Duplicate(nullptr));
                if (Dup)
                {
                    Dup->AddActorWorldOffset(DuplicateOffsetStep * static_cast<float>(DuplicateIndex + 1));
                    NewSelection.push_back(Dup);
                    ++DuplicateIndex;
                }
            }
            SelectionManager->ClearSelection();
            for (AActor *Actor : NewSelection)
                SelectionManager->ToggleSelect(Actor);
            GizmoManager.SyncVisualFromTarget();
        }
    }
}

void FLevelEditorViewportClient::OnEditorToggleGizmoMode(const FInputActionValue &Value)
{
    if (!CanProcessLiveContextWork())
    {
        return;
    }

    FInputManager &Input = FInputManager::Get();
    if (Input.IsMouseButtonDown(VK_RBUTTON))
    {
        return;
    }

    EGizmoMode NewMode = GizmoManager.GetMode();
    if (Input.IsKeyPressed('W'))
    {
        NewMode = EGizmoMode::Translate;
    }
    else if (Input.IsKeyPressed('E'))
    {
        NewMode = EGizmoMode::Rotate;
    }
    else if (Input.IsKeyPressed('R'))
    {
        NewMode = EGizmoMode::Scale;
    }
    else if (Input.IsKeyPressed('Q'))
    {
        NewMode = EGizmoMode::Select;
    }
    else
    {
        int NextInt = (static_cast<int>(NewMode) + 1) % static_cast<int>(EGizmoMode::End);
        if (NextInt == 0)
        {
            NextInt = 1;
        }
        NewMode = static_cast<EGizmoMode>(NextInt);
    }

    if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
    {
        EditorEngine->SetEditorGizmoMode(NewMode);
    }
    else
    {
        GizmoManager.SetMode(NewMode);
    }
}

void FLevelEditorViewportClient::OnEditorToggleCoordSystem(const FInputActionValue &Value)
{
    if (!CanProcessLiveContextWork())
    {
        return;
    }

    if (!FInputManager::Get().IsKeyDown(VK_CONTROL))
    {
        if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
            EditorEngine->ToggleCoordSystem();
    }
}

void FLevelEditorViewportClient::OnEditorEscape(const FInputActionValue &Value)
{
    if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
    {
        if (EditorEngine->IsPlayingInEditor())
            EditorEngine->RequestEndPlayMap();
    }
}

void FLevelEditorViewportClient::OnEditorTogglePIE(const FInputActionValue &Value)
{
    if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
    {
        if (EditorEngine->IsPlayingInEditor())
            EditorEngine->TogglePIEControlMode();
    }
}

void FLevelEditorViewportClient::SetLightViewOverride(ULightComponentBase *Light)
{
    LightViewOverride = Light;
    PointLightFaceIndex = 0;
    if (Light && SelectionManager)
        SelectionManager->ClearSelection();
}

void FLevelEditorViewportClient::ClearLightViewOverride()
{
    LightViewOverride = nullptr;
}

void FLevelEditorViewportClient::InitializeCameraState()
{
    // Editor camera is owned by FEditorViewportClient as a value member.
    ResetCamera();
}

void FLevelEditorViewportClient::ReleaseCameraState()
{
    // No-op: editor camera is no longer heap/component owned.
}

void FLevelEditorViewportClient::ResetCamera()
{
    if (!GetCamera() || !Settings)
        return;
    GetCameraController().ResetLookAt(Settings->InitViewPos, Settings->InitLookAt);
}

bool FLevelEditorViewportClient::FocusActor(AActor *Actor)
{
    if (!Actor || !GetCamera())
    {
        return false;
    }

    const FVector TargetLoc = Actor->GetActorLocation();
    constexpr float FocusDistance = 5.0f;
    GetCameraController().StartFocus(TargetLoc, FocusDistance, FocusAnimDuration);
    return true;
}

void FLevelEditorViewportClient::SetViewportType(ELevelViewportType NewType)
{
    if (!GetCamera())
        return;

    RenderOptions.ViewportType = NewType;
    GetCameraController().SetViewportType(NewType, 50.0f);
}


void FLevelEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
    if (InWidth > 0.0f)
        WindowWidth = InWidth;
    if (InHeight > 0.0f)
        WindowHeight = InHeight;
    if (GetCamera())
        GetCamera()->OnResize(static_cast<int32>(WindowWidth), static_cast<int32>(WindowHeight));
}

void FLevelEditorViewportClient::Tick(float DeltaTime)
{
    if (!CanProcessLiveContextWork() || !bIsActive)
        return;
    if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
    {
        if (EditorEngine->IsPlayingInEditor())
        {
            FInputManager &Input = FInputManager::Get();
            if (Input.IsKeyPressed(VK_ESCAPE))
            {
                EditorEngine->RequestEndPlayMap();
                return;
            }
            if (Input.IsKeyPressed(VK_F8))
            {
                EditorEngine->TogglePIEControlMode();
            }

            // 留?Tick留덈떎 SetDrivingCamera(EditorViewportCamera)瑜??섏? 留?寃?
            if (EditorEngine->IsPIEPossessedMode())
            {
                if (UGameViewportClient *GameViewportClient = EditorEngine->GetGameViewportClient())
                {
                    GameViewportClient->SetViewport(Viewport);
                    GameViewportClient->SetCursorClipRect(ViewportScreenRect);

                    if (!GameViewportClient->HasPossessedTarget())
                    {
                        if (UWorld *World = EditorEngine->GetWorld())
                        {
                            GameViewportClient->Possess(World->GetActiveCamera());
                        }
                    }

                    GameViewportClient->ProcessPIEInput(DeltaTime);
                }
                return;
            }
        }
    }
    GetCameraController().SyncTargetToCamera();
    if (!GetCameraController().TickFocus(DeltaTime))
    {
        GetCameraController().ApplySmoothedLocation(DeltaTime, SmoothLocationSpeed);
    }
    TickInput(DeltaTime);

    if (bNeedsDeferredTargetSync)
    {
        // If the user switches tabs while a mouse button is still held, do not
        // resurrect the previous selection as a live gizmo target yet.
        if (IsAnyEditorMouseButtonDownForContextSwitchGuard())
        {
            GizmoManager.ResetVisualInteractionState();
            return;
        }

        bNeedsDeferredTargetSync = false;
        SyncGizmoTargetFromSelection();
    }
    else
    {
        SyncGizmoTargetFromSelection();
    }

    TickInteraction(DeltaTime);
}

void FLevelEditorViewportClient::SyncGizmoTargetFromSelection()
{
    if (!CanProcessLiveContextWork())
    {
        CurrentGizmoTargetComponent = nullptr;
        GizmoManager.AbortLiveInteractionWithoutApplying();
        return;
    }

    if (!SelectionManager)
    {
        CurrentGizmoTargetComponent = nullptr;
        GizmoManager.ClearTarget();
        return;
    }

    USceneComponent *SelectedComponent = SelectionManager->GetSelectedComponent();
    auto Target = SelectionManager->MakeTransformGizmoTarget(this, GetEditorContextActiveFlag());
    if (!Target)
    {
        CurrentGizmoTargetComponent = nullptr;
        GizmoManager.ClearTarget();
        return;
    }

    FGizmoTargetKey TargetKey{SelectedComponent, 0};
    CurrentGizmoTargetComponent = SelectedComponent;
    GizmoManager.SetTargetIfChanged(TargetKey, Target);
}

bool FLevelEditorViewportClient::BuildRenderRequest(FEditorViewportRenderRequest &OutRequest)
{
    // Render eligibility is decided by UEditorEngine::ShouldRenderViewportClient().
    // Do not use the live input/gizmo context flag here: during startup the Level editor
    // is already the active editor context, but individual viewport clients may not have
    // received ActivateEditorContext() yet. Blocking here makes the first Level viewport black.
    UWorld *World = GetWorld();
    if (!Viewport || !World || !GetCamera())
    {
        return false;
    }

    FScene* Scene = &World->GetScene();
    RegisterGizmoToScene(Scene);

    OutRequest.Viewport = Viewport;
    OutRequest.ViewInfo = GetCamera()->GetCameraState();
    OutRequest.Scene = Scene;
    OutRequest.RenderOptions = RenderOptions;
    OutRequest.CursorProvider = this;
    OutRequest.bRenderGrid = RenderOptions.ShowFlags.bGrid;
    OutRequest.bEnableGPUOcclusion = true;
    OutRequest.bAllowLevelDebugVisuals = true;
    OutRequest.World = World;
    OutRequest.LevelViewportClient = this;
    return true;
}

void FLevelEditorViewportClient::SyncCameraSmoothingTarget()
{
    GetCameraController().SyncTargetToCamera();
}

void FLevelEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
{
    GetCameraController().ApplySmoothedLocation(DeltaTime, SmoothLocationSpeed);
}


void FLevelEditorViewportClient::TickEditorShortcuts()
{
    if (!CanProcessLiveContextWork())
    {
        return;
    }

    UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine);
    if (!EditorEngine)
    {
        return;
    }

    if (EditorEngine->IsPlayingInEditor() && FInputManager::Get().IsKeyPressed(VK_ESCAPE))
    {
        EditorEngine->RequestEndPlayMap();
    }

    const bool bAllowKeyboardInput = !FInputManager::Get().IsGuiUsingKeyboard() && !ImGui::GetIO().WantTextInput;
    if (!bAllowKeyboardInput)
    {
        return;
    }

    if (SelectionManager && FInputManager::Get().IsKeyPressed(VK_DELETE))
    {
        EditorEngine->BeginTrackedSceneChange();
        SelectionManager->DeleteSelectedActors();
        EditorEngine->CommitTrackedSceneChange();
        return;
    }

    if (!FInputManager::Get().IsKeyDown(VK_CONTROL) && FInputManager::Get().IsKeyPressed('X'))
    {
        EditorEngine->ToggleCoordSystem();
        return;
    }

    if (SelectionManager && FInputManager::Get().IsKeyPressed('F'))
    {
        AActor *Selected = SelectionManager->GetPrimarySelection();
        FocusActor(Selected);
    }

    if (SelectionManager && FInputManager::Get().IsKeyDown(VK_CONTROL) && FInputManager::Get().IsKeyPressed('D'))
    {
        const TArray<AActor *> ToDuplicate = SelectionManager->GetSelectedActors();
        if (!ToDuplicate.empty())
        {
            EditorEngine->BeginTrackedSceneChange();
            const FVector DuplicateOffsetStep(0.1f, 0.1f, 0.1f);
            TArray<AActor *> NewSelection;
            int32 DuplicateIndex = 0;
            for (AActor *Src : ToDuplicate)
            {
                if (!Src)
                    continue;
                AActor *Dup = Cast<AActor>(Src->Duplicate(nullptr));
                if (Dup)
                {
                    Dup->AddActorWorldOffset(DuplicateOffsetStep * static_cast<float>(DuplicateIndex + 1));
                    NewSelection.push_back(Dup);
                    ++DuplicateIndex;
                }
            }
            SelectionManager->ClearSelection();
            for (AActor *Actor : NewSelection)
            {
                SelectionManager->ToggleSelect(Actor);
            }
            GizmoManager.SyncVisualFromTarget();
            EditorEngine->CommitTrackedSceneChange();
        }
    }
}

void FLevelEditorViewportClient::TickInput(float DeltaTime)
{
    if (!CanProcessLiveContextWork())
        return;
    if (!GetCamera())
        return;
    if (IsViewingFromLight())
        return;
    FInputManager &Input = FInputManager::Get();
    CommonInput.InputState.ResetFrame();
    bool bForceInput = CanProcessLiveContextWork() && (bIsHovered || bIsActive || Input.IsMouseButtonDown(VK_RBUTTON));
    EnhancedInputManager.ProcessInput(&Input, DeltaTime, bForceInput);
    const FMinimalViewInfo &CameraState = GetCamera()->GetCameraState();
    const bool bIsOrtho = CameraState.bIsOrthogonal;
    const float MoveSensitivity = RenderOptions.CameraMoveSensitivity;
    const float CameraSpeed = (Settings ? Settings->CameraSpeed : 10.f) * MoveSensitivity;
    const float PanMouseScale = CameraSpeed * 0.01f;
    if (!bIsOrtho)
    {
        GetCameraController().AddLocalTargetDelta(FVector(
            CommonInput.InputState.MoveAccumulator.X * CameraSpeed * DeltaTime,
            CommonInput.InputState.MoveAccumulator.Y * CameraSpeed * DeltaTime,
            0.0f));
        GetCameraController().AddWorldTargetDelta(FVector(0.0f, 0.0f, CommonInput.InputState.MoveAccumulator.Z * CameraSpeed * DeltaTime));
        if (!CommonInput.InputState.PanAccumulator.IsNearlyZero())
        {
            GetCameraController().AddPanTargetDelta(CommonInput.InputState.PanAccumulator.X, CommonInput.InputState.PanAccumulator.Y, PanMouseScale * 0.15f);
        }
        if (!CommonInput.InputState.RotateAccumulator.IsNearlyZero())
        {
            const float RotateSensitivity = RenderOptions.CameraRotateSensitivity;
            const float MouseRotationSpeed = 0.15f * RotateSensitivity;
            const float AngleVelocity = (Settings ? Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
            float Yaw = 0.0f, Pitch = 0.0f;
            if (!Input.IsMouseButtonDown(VK_RBUTTON))
            {
                Yaw = CommonInput.InputState.RotateAccumulator.X * AngleVelocity * DeltaTime;
                Pitch = CommonInput.InputState.RotateAccumulator.Y * AngleVelocity * DeltaTime;
            }
            else
            {
                Yaw = CommonInput.InputState.RotateAccumulator.X * MouseRotationSpeed;
                Pitch = CommonInput.InputState.RotateAccumulator.Y * MouseRotationSpeed;
            }
            GetCameraController().Rotate(Yaw, Pitch);
        }
    }
    else
    {
        if (!CommonInput.InputState.RotateAccumulator.IsNearlyZero() && Input.IsMouseButtonDown(VK_RBUTTON))
        {
            float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;
            GetCameraController().MoveLocalImmediate(FVector(0, -CommonInput.InputState.RotateAccumulator.Y * PanScale, CommonInput.InputState.RotateAccumulator.Z * PanScale));
        }
    }
}

// Find closest vertex in the world to a ray
static FVector FindClosestVertex(UWorld *World, const FRay &Ray, float MaxDistance = 10.0f)
{
    if (!World)
        return FVector::ZeroVector;
    FVector BestVertex = FVector::ZeroVector;
    float BestDistSq = MaxDistance * MaxDistance;
    bool bFound = false;
    for (AActor *Actor : World->GetActors())
    {
        if (!Actor || !Actor->IsVisible() || Actor->IsA<UGizmoVisualComponent>())
            continue;
        for (UActorComponent *Comp : Actor->GetComponents())
        {
            if (UMeshComponent *MeshComp = Comp->IsA<UMeshComponent>() ? static_cast<UMeshComponent *>(Comp) : nullptr)
            {
                FMeshDataView View = MeshComp->GetMeshDataView();
                if (!View.IsValid())
                    continue;
                FMatrix WorldMat = MeshComp->GetWorldMatrix();
                for (uint32 i = 0; i < View.VertexCount; ++i)
                {
                    FVector LocalPos = View.GetPosition(i);
                    FVector WorldPos = WorldMat.TransformPositionWithW(LocalPos); // Use TransformPositionWithW
                    FVector W = WorldPos - Ray.Origin;
                    float Proj = W.Dot(Ray.Direction);
                    FVector ClosestP = Ray.Origin + Ray.Direction * (std::max)(0.0f, Proj);
                    float DistSq = (WorldPos - ClosestP).Dot(WorldPos - ClosestP);
                    if (DistSq < BestDistSq)
                    {
                        BestDistSq = DistSq;
                        BestVertex = WorldPos;
                        bFound = true;
                    }
                }
            }
        }
    }
    return bFound ? BestVertex : FVector::ZeroVector;
}

void FLevelEditorViewportClient::TickInteraction(float DeltaTime)
{
    (void)DeltaTime;
    if (!CanProcessLiveContextWork())
        return;
    if (!GetCamera() || !GetWorld())
        return;
    EnsureEditorGizmo();
    GizmoManager.ApplyScreenSpaceScaling(GetCamera()->GetWorldLocation(), GetCamera()->IsOrthogonal(), GetCamera()->GetOrthoWidth(), Viewport ? static_cast<float>(Viewport->GetHeight()) : 0.0f);
    GizmoManager.SetAxisMask(UGizmoVisualComponent::ComputeAxisMask(RenderOptions.ViewportType, GizmoManager.GetMode()));

    uint32 CursorViewportX = 0;
    uint32 CursorViewportY = 0;
    const bool bCursorInViewport = GetCursorViewportPosition(CursorViewportX, CursorViewportY);
    if (FInputManager::Get().IsGuiUsingMouse() && !bCursorInViewport && !GizmoManager.IsDragging() && !bIsMarqueeSelecting)
    {
        return;
    }

    const float ZoomSpeed = Settings ? Settings->CameraZoomSpeed : 300.f;
    if (std::abs(CommonInput.InputState.ZoomAccumulator) > 1e-6f)
    {
        if (GetCamera()->IsOrthogonal())
        {
            float NewWidth = GetCamera()->GetOrthoWidth() - CommonInput.InputState.ZoomAccumulator * ZoomSpeed * DeltaTime;
            GetCamera()->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f));
        }
        else
        {
            GetCameraController().AddForwardTargetDelta(CommonInput.InputState.ZoomAccumulator * ZoomSpeed * 0.015f);
        }
    }
    FInputManager &Input = FInputManager::Get();

    // Mouse-up can be missed when the cursor leaves the viewport, a tab changes focus,
    // or ImGui captures the mouse for one frame. In that case the manager/visual keeps
    // Holding=true and every later hit test uses stale axis state. Treat "not down"
    // as the source of truth and recover before doing another pick.
    if (!Input.IsMouseButtonDown(FInputManager::MOUSE_LEFT))
    {
        bool bCommittedInteraction = false;
        if (GizmoManager.GetUIScreenInteractionState().bDragging)
        {
            EndUIScreenTranslateDrag(true);
            bCommittedInteraction = true;
        }
        if (GizmoManager.IsDragging())
        {
            GizmoManager.EndDrag();
            bCommittedInteraction = true;
        }
        else
        {
            GizmoManager.ResetVisualInteractionState();
        }

        if (bCommittedInteraction)
        {
            if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
            {
                EditorEngine->CommitTrackedTransformChange();
            }
        }
    }

    ImVec2 MousePos = ImGui::GetIO().MousePos;
    GizmoManager.GetUIScreenInteractionState().HoveredAxis = HasUIScreenTranslateGizmo() ? HitTestUIScreenTranslateGizmo(MousePos) : 0;
    float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
    float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
    float LocalMouseX = 0.0f;
    float LocalMouseY = 0.0f;
    if (!TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, LocalMouseX,
                                        LocalMouseY))
    {
        LocalMouseX = MousePos.x - ViewportScreenRect.X;
        LocalMouseY = MousePos.y - ViewportScreenRect.Y;
    }
    FRay Ray = GetCamera()->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
    FGizmoHitProxyContext GizmoPickContext{};
    GizmoPickContext.ViewProjection = GetCamera()->GetViewProjectionMatrix();
    GizmoPickContext.CameraLocation = GetCamera()->GetWorldLocation();
    GizmoPickContext.bIsOrtho = GetCamera()->IsOrthogonal();
    GizmoPickContext.OrthoWidth = GetCamera()->GetOrthoWidth();
    GizmoPickContext.ViewportWidth = VPWidth;
    GizmoPickContext.ViewportHeight = VPHeight;
    GizmoPickContext.MouseX = LocalMouseX;
    GizmoPickContext.MouseY = LocalMouseY;
    GizmoPickContext.PickRadius = 2; // 5x5 ID buffer

    FGizmoHitProxyResult GizmoHitResult{};
    const bool bCanInteractWithGizmo =
        SelectionManager && SelectionManager->GetSelectedComponent() && GizmoManager.HasValidTarget();
    bool bGizmoHit = bCanInteractWithGizmo && GizmoManager.HitTestHitProxy(GizmoPickContext, GizmoHitResult);
    if (Input.IsKeyPressed(FInputManager::MOUSE_LEFT) && bIsHovered)
    {
        if (Input.IsKeyDown(VK_CONTROL) && Input.IsKeyDown(VK_MENU))
        {
            bIsMarqueeSelecting = true;
            MarqueeStartPos = FVector(MousePos.x, MousePos.y, 0);
            MarqueeCurrentPos = FVector(MousePos.x, MousePos.y, 0);
        }
        else
        {
            if (Input.IsKeyDown(VK_MENU) && bGizmoHit && SelectionManager && !SelectionManager->IsEmpty())
            {
                const TArray<AActor *> ToDuplicate = SelectionManager->GetSelectedActors();
                TArray<AActor *> NewSelection;
                for (AActor *Src : ToDuplicate)
                {
                    if (AActor *Dup = Cast<AActor>(Src->Duplicate(nullptr)))
                        NewSelection.push_back(Dup);
                }
                SelectionManager->ClearSelection();
                for (AActor *Actor : NewSelection)
                    SelectionManager->ToggleSelect(Actor);
            }
            HandleDragStart(Ray, GizmoHitResult, bGizmoHit);
        }
    }
    else if (Input.IsMouseButtonDown(FInputManager::MOUSE_LEFT))
    {
        if (bIsMarqueeSelecting)
        {
            MarqueeCurrentPos = FVector(MousePos.x, MousePos.y, 0);
        }
        else if (GizmoManager.GetUIScreenInteractionState().bDragging)
        {
            UpdateUIScreenTranslateDrag(MousePos);
        }
        else
        {
            if (GizmoManager.IsDragging())
            {
                if (Input.IsKeyDown('V'))
                {
                    FVector BestV = FindClosestVertex(GetWorld(), Ray, 2.0f);
                    if (!BestV.IsNearlyZero())
                    {
                        GizmoManager.SetTargetWorldLocation(BestV);
                        GetCameraController().SyncTargetToCamera();
                    }
                }
                GizmoManager.UpdateDrag(Ray);
            }
        }    }
    else if (Input.IsKeyReleased(FInputManager::MOUSE_LEFT))
    {
        if (GizmoManager.GetUIScreenInteractionState().bDragging)
        {
            EndUIScreenTranslateDrag(true);
        }
        else if (bIsMarqueeSelecting)
        {
            bIsMarqueeSelecting = false;
            float MinX = (std::min)(MarqueeStartPos.X, MarqueeCurrentPos.X);
            float MaxX = (std::max)(MarqueeStartPos.X, MarqueeCurrentPos.X);
            float MinY = (std::min)(MarqueeStartPos.Y, MarqueeCurrentPos.Y);
            float MaxY = (std::max)(MarqueeStartPos.Y, MarqueeCurrentPos.Y);
            if (std::abs(MaxX - MinX) > 2.0f || std::abs(MaxY - MinY) > 2.0f)
            {
                UWorld *World = GetWorld();
                if (World && SelectionManager)
                {
                    if (!Input.IsKeyDown(VK_CONTROL))
                        SelectionManager->ClearSelection();
                    FMatrix VP = GetCamera()->GetViewProjectionMatrix();
                    for (AActor *Actor : World->GetActors())
                    {
                        if (!Actor || !Actor->IsVisible() || Actor->IsA<UGizmoVisualComponent>())
                            continue;
                        FVector WorldPos = Actor->GetActorLocation();
                        FVector ClipSpace = VP.TransformPositionWithW(WorldPos);
                        float ScreenX = (ClipSpace.X * 0.5f + 0.5f) * VPWidth + ViewportScreenRect.X;
                        float ScreenY = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * VPHeight + ViewportScreenRect.Y;
                        if (ScreenX >= MinX && ScreenX <= MaxX && ScreenY >= MinY && ScreenY <= MaxY)
                            SelectionManager->ToggleSelect(Actor);
                    }
                }
            }
        }
        else
        {
            GizmoManager.EndDrag();
            if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
            {
                EditorEngine->CommitTrackedTransformChange();
            }
        }
    }
    else if (Input.IsKeyReleased(VK_LBUTTON))
    {
        if (GizmoManager.GetUIScreenInteractionState().bDragging)
        {
            EndUIScreenTranslateDrag(true);
        }
        GizmoManager.ResetVisualInteractionState();
        if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
        {
            EditorEngine->CommitTrackedTransformChange();
        }
        bIsMarqueeSelecting = false;
    }
}

void FLevelEditorViewportClient::HandleDragStart(const FRay &Ray, const FGizmoHitProxyResult& GizmoHitResult, bool bHasGizmoHit)
{
    FInputManager &Input = FInputManager::Get();
    if (!CanProcessLiveContextWork() || !bIsHovered)
        return;
    if (BeginUIScreenTranslateDrag(ImGui::GetIO().MousePos))
    {
        return;
    }
    FScopeCycleCounter PickCounter;
    const bool bCanInteractWithGizmo =
        SelectionManager && SelectionManager->GetSelectedComponent() && GizmoManager.HasValidTarget();
    if (bCanInteractWithGizmo && bHasGizmoHit && GizmoHitResult.bHit)
    {
        if (SelectionManager)
        {
            for (AActor *Actor : SelectionManager->GetSelectedActors())
            {
                if (Actor && Actor->IsActorMovementLocked())
                {
                    GizmoManager.ResetVisualInteractionState();
                    return;
                }
            }
        }
        if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
        {
            EditorEngine->BeginTrackedTransformChange();
        }
        GizmoManager.BeginDragFromHitProxy(GizmoHitResult);
    }
    else
    {
        uint32 CursorX = 0;
        uint32 CursorY = 0;
        USceneComponent *HitUIComponent =
            GetCursorViewportPosition(CursorX, CursorY)
                ? FindTopmostUIComponentAt(GetWorld(), static_cast<float>(CursorX), static_cast<float>(CursorY))
                : nullptr;
        if (HitUIComponent)
        {
            AActor *HitActor = HitUIComponent->GetOwner();
            const bool bCtrlHeld = Input.IsKeyDown(VK_CONTROL);
            if (HitActor)
            {
                if (bCtrlHeld)
                {
                    const bool bWasSelected = SelectionManager->IsSelected(HitActor);
                    SelectionManager->ToggleSelect(HitActor);
                    if (!bWasSelected)
                    {
                        SelectionManager->SelectComponent(HitUIComponent);
                    }
                }
                else
                {
                    if (SelectionManager->GetPrimarySelection() != HitActor)
                    {
                        SelectionManager->Select(HitActor);
                    }
                    SelectionManager->SelectComponent(HitUIComponent);
                }
            }
        }
        else
        {
            FRayHitResult HitResult{};
            AActor *BestActor = nullptr;
            if (UWorld *W = GetWorld())
            {
                W->RaycastPrimitives(Ray, HitResult, BestActor);
                if (!BestActor)
                {
                    EditorRaycastAllVisiblePrimitives(W, Ray, HitResult, BestActor);
                }
                if (!BestActor && GetCamera())
                {
                    const ImVec2 MousePos = ImGui::GetIO().MousePos;
                    float LocalMouseX = 0.0f;
                    float LocalMouseY = 0.0f;
                    if (TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth,
                                                       WindowHeight, LocalMouseX, LocalMouseY))
                    {
                        UPrimitiveComponent *ScreenHitPrimitive = nullptr;
                        const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
                        const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
                        BestActor = FindScreenSpacePrimitiveAt(W, GetCamera(), LocalMouseX, LocalMouseY, VPWidth, VPHeight,
                                                               ScreenHitPrimitive);
                        if (ScreenHitPrimitive)
                        {
                            HitResult.HitComponent = ScreenHitPrimitive;
                        }
                    }
                }
            }
            bool bCtrlHeld = Input.IsKeyDown(VK_CONTROL);
            if (BestActor == nullptr)
            {
                if (!bCtrlHeld)
                    SelectionManager->ClearSelection();
            }
            else
            {
                if (bCtrlHeld)
                    SelectionManager->ToggleSelect(BestActor);
                else
                {
                    if (SelectionManager->GetPrimarySelection() == BestActor)
                    {
                        if (HitResult.HitComponent)
                            SelectionManager->SelectComponent(HitResult.HitComponent);
                    }
                    else
                        SelectionManager->Select(BestActor);
                }
            }
        }
    }
    if (OverlayStatSystem)
    {
        const uint64 PickCycles = PickCounter.Finish();
        const double ElapsedMs = FPlatformTime::ToMilliseconds(PickCycles);
        OverlayStatSystem->RecordPickingAttempt(ElapsedMs);
    }
}

bool FLevelEditorViewportClient::HasUIScreenTranslateGizmo() const
{
    if (!SelectionManager || GizmoManager.GetMode() != EGizmoMode::Translate)
    {
        return false;
    }

    if (const UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
    {
        if (EditorEngine->IsPlayingInEditor())
        {
            return false;
        }
    }

    USceneComponent *SelectedComponent = SelectionManager->GetSelectedComponent();
    return SelectedComponent && IsUIScreenTransformableComponent(SelectedComponent);
}

int32 FLevelEditorViewportClient::HitTestUIScreenTranslateGizmo(const ImVec2 &MousePos) const
{
    if (!HasUIScreenTranslateGizmo())
    {
        return static_cast<int32>(EUIScreenGizmoAxis::None);
    }

    USceneComponent *SelectedComponent = SelectionManager->GetSelectedComponent();
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
    if (!GetUIScreenComponentBounds(SelectedComponent, X, Y, Width, Height))
    {
        return static_cast<int32>(EUIScreenGizmoAxis::None);
    }

    float CenterX = X + Width * 0.5f;
    float CenterY = Y + Height * 0.5f;
    if (!TryConvertViewportPixelToScreenPoint(CenterX, CenterY, ViewportScreenRect, Viewport, WindowWidth, WindowHeight,
                                              CenterX, CenterY))
    {
        CenterX += ViewportScreenRect.X;
        CenterY += ViewportScreenRect.Y;
    }
    const ImVec2 Local(MousePos.x - CenterX, MousePos.y - CenterY);
    const float CenterHalf = 8.0f;
    const float AxisThickness = 6.0f;
    const float AxisLength = 48.0f;

    if (std::abs(Local.x) <= CenterHalf && std::abs(Local.y) <= CenterHalf)
    {
        return static_cast<int32>(EUIScreenGizmoAxis::XY);
    }

    if (Local.x >= CenterHalf && Local.x <= AxisLength && std::abs(Local.y) <= AxisThickness)
    {
        return static_cast<int32>(EUIScreenGizmoAxis::X);
    }

    if (Local.y >= CenterHalf && Local.y <= AxisLength && std::abs(Local.x) <= AxisThickness)
    {
        return static_cast<int32>(EUIScreenGizmoAxis::Y);
    }

    return static_cast<int32>(EUIScreenGizmoAxis::None);
}

bool FLevelEditorViewportClient::BeginUIScreenTranslateDrag(const ImVec2 &MousePos)
{
    if (!HasUIScreenTranslateGizmo())
    {
        return false;
    }

    const int32 HitAxis = HitTestUIScreenTranslateGizmo(MousePos);
    if (HitAxis == static_cast<int32>(EUIScreenGizmoAxis::None))
    {
        return false;
    }

    if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
    {
        EditorEngine->BeginTrackedTransformChange();
    }

    GizmoManager.GetUIScreenInteractionState().ActiveAxis = HitAxis;
    float ViewportMouseX = 0.0f;
    float ViewportMouseY = 0.0f;
    if (TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight,
                                       ViewportMouseX, ViewportMouseY))
    {
        GizmoManager.GetUIScreenInteractionState().LastMousePos = ImVec2(ViewportMouseX, ViewportMouseY);
    }
    else
    {
        // Drag 좌표계는 항상 viewport-local로 유지한다.
        // screen/global 좌표를 섞으면 커서가 뷰포트 밖으로 나간 순간 큰 delta가 발생한다.
        GizmoManager.GetUIScreenInteractionState().LastMousePos =
            ImVec2(MousePos.x - ViewportScreenRect.X, MousePos.y - ViewportScreenRect.Y);
    }
    GizmoManager.GetUIScreenInteractionState().bDragging = true;
    return true;
}

void FLevelEditorViewportClient::UpdateUIScreenTranslateDrag(const ImVec2 &MousePos)
{
    if (!GizmoManager.GetUIScreenInteractionState().bDragging || !SelectionManager)
    {
        return;
    }

    USceneComponent *SelectedComponent = SelectionManager->GetSelectedComponent();
    if (!IsUIScreenTransformableComponent(SelectedComponent))
    {
        return;
    }

    FVector ScreenPosition;
    if (!GetUIScreenComponentPosition(SelectedComponent, ScreenPosition))
    {
        return;
    }

    float ViewportMouseX = 0.0f;
    float ViewportMouseY = 0.0f;
    ImVec2 CurrentMouseInViewport(MousePos.x - ViewportScreenRect.X, MousePos.y - ViewportScreenRect.Y);
    if (TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight,
                                       ViewportMouseX, ViewportMouseY))
    {
        CurrentMouseInViewport = ImVec2(ViewportMouseX, ViewportMouseY);
    }

    const ImVec2 Delta(CurrentMouseInViewport.x - GizmoManager.GetUIScreenInteractionState().LastMousePos.x,
                       CurrentMouseInViewport.y - GizmoManager.GetUIScreenInteractionState().LastMousePos.y);
    switch (static_cast<EUIScreenGizmoAxis>(GizmoManager.GetUIScreenInteractionState().ActiveAxis))
    {
    case EUIScreenGizmoAxis::X:
        ScreenPosition.X += Delta.x;
        break;
    case EUIScreenGizmoAxis::Y:
        ScreenPosition.Y += Delta.y;
        break;
    case EUIScreenGizmoAxis::XY:
        ScreenPosition.X += Delta.x;
        ScreenPosition.Y += Delta.y;
        break;
    default:
        return;
    }

    SetUIScreenComponentPosition(SelectedComponent, ScreenPosition);
    GizmoManager.GetUIScreenInteractionState().LastMousePos = CurrentMouseInViewport;
}

void FLevelEditorViewportClient::EndUIScreenTranslateDrag(bool bCommitChange)
{
    if (!GizmoManager.GetUIScreenInteractionState().bDragging)
    {
        return;
    }

    GizmoManager.GetUIScreenInteractionState().bDragging = false;
    GizmoManager.GetUIScreenInteractionState().ActiveAxis = static_cast<int32>(EUIScreenGizmoAxis::None);
    if (bCommitChange)
    {
        if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
        {
            EditorEngine->CommitTrackedTransformChange();
        }
    }
}

void FLevelEditorViewportClient::UpdateLayoutRect()
{
    if (!LayoutWindow)
        return;
    const FRect &R = LayoutWindow->GetRect();
    ViewportScreenRect = R;

    if (!Viewport)
    {
        return;
    }

    if (FProjectSettings::Get().Game.bLockWindowResolution)
    {
        const uint32 TargetW = (std::max)(320u, FProjectSettings::Get().Game.WindowWidth);
        const uint32 TargetH = (std::max)(240u, FProjectSettings::Get().Game.WindowHeight);
        if (GetCamera())
        {
            GetCamera()->OnResize(static_cast<int32>(TargetW), static_cast<int32>(TargetH));
        }
        if (Viewport->GetWidth() != TargetW || Viewport->GetHeight() != TargetH)
        {
            Viewport->RequestResize(TargetW, TargetH);
        }

        if (R.Width > 0.0f && R.Height > 0.0f)
        {
            const float Scale =
                (std::min)(R.Width / static_cast<float>(TargetW), R.Height / static_cast<float>(TargetH));
            const float DrawW = static_cast<float>(TargetW) * Scale;
            const float DrawH = static_cast<float>(TargetH) * Scale;
            ViewportScreenRect.X = R.X + (R.Width - DrawW) * 0.5f;
            ViewportScreenRect.Y = R.Y + (R.Height - DrawH) * 0.5f;
            ViewportScreenRect.Width = DrawW;
            ViewportScreenRect.Height = DrawH;
        }
        SyncPIEViewportRect(Viewport, ViewportScreenRect);
        return;
    }

    uint32 SlotW = static_cast<uint32>(R.Width);
    uint32 SlotH = static_cast<uint32>(R.Height);
    if (GetCamera() && SlotW > 0 && SlotH > 0)
    {
        GetCamera()->OnResize(static_cast<int32>(SlotW), static_cast<int32>(SlotH));
    }
    if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight()))
    {
        Viewport->RequestResize(SlotW, SlotH);
    }
    SyncPIEViewportRect(Viewport, ViewportScreenRect);
}

void FLevelEditorViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
    if (!Viewport || !Viewport->GetSRV())
        return;
    const FRect &R = ViewportScreenRect;
    if (R.Width <= 0 || R.Height <= 0)
        return;
    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    ImVec2 Min(R.X, R.Y);
    ImVec2 Max(R.X + R.Width, R.Y + R.Height);
    DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);
    if (bIsActiveViewport)
    {
        constexpr float ActiveBorderThickness = 4.0f;
        const float BorderInset = ActiveBorderThickness * 0.5f;
        ImU32 BorderColor = IM_COL32(255, 165, 0, 220);
        if (UEditorEngine *EditorEngine = Cast<UEditorEngine>(GEngine))
        {
            if (EditorEngine->IsPlayingInEditor())
            {
                BorderColor = EditorEngine->IsGamePaused() ? UIAccentColor::ToU32() : IM_COL32(52, 199, 89, 255);
            }
        }
        if (R.Width > ActiveBorderThickness && R.Height > ActiveBorderThickness)
        {
            DrawList->AddRect(ImVec2(Min.x + BorderInset, Min.y + BorderInset),
                              ImVec2(Max.x - BorderInset, Max.y - BorderInset), BorderColor, 0.0f, 0,
                              ActiveBorderThickness);
        }
    }
    if (bIsMarqueeSelecting)
    {
        ImDrawList *ForegroundDrawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
        ImVec2 RectMin((std::min)(MarqueeStartPos.X, MarqueeCurrentPos.X),
                       (std::min)(MarqueeStartPos.Y, MarqueeCurrentPos.Y));
        ImVec2 RectMax((std::max)(MarqueeStartPos.X, MarqueeCurrentPos.X),
                       (std::max)(MarqueeStartPos.Y, MarqueeCurrentPos.Y));
        ForegroundDrawList->AddRect(RectMin, RectMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 5.0f);
    }

    DrawUIScreenTranslateGizmo();
}

void FLevelEditorViewportClient::DrawUIScreenTranslateGizmo()
{
    if (!HasUIScreenTranslateGizmo())
    {
        return;
    }

    USceneComponent *SelectedComponent = SelectionManager->GetSelectedComponent();
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
    if (!GetUIScreenComponentBounds(SelectedComponent, X, Y, Width, Height))
    {
        return;
    }

    float CenterX = X + Width * 0.5f;
    float CenterY = Y + Height * 0.5f;
    if (!TryConvertViewportPixelToScreenPoint(CenterX, CenterY, ViewportScreenRect, Viewport, WindowWidth, WindowHeight,
                                              CenterX, CenterY))
    {
        CenterX += ViewportScreenRect.X;
        CenterY += ViewportScreenRect.Y;
    }
    const float AxisLength = 48.0f;
    const float CenterHalf = 8.0f;
    const float Thickness = 3.0f;
    const bool bHoverX = GizmoManager.GetUIScreenInteractionState().HoveredAxis == static_cast<int32>(EUIScreenGizmoAxis::X);
    const bool bHoverY = GizmoManager.GetUIScreenInteractionState().HoveredAxis == static_cast<int32>(EUIScreenGizmoAxis::Y);
    const bool bHoverXY = GizmoManager.GetUIScreenInteractionState().HoveredAxis == static_cast<int32>(EUIScreenGizmoAxis::XY);
    const bool bActiveX = GizmoManager.GetUIScreenInteractionState().ActiveAxis == static_cast<int32>(EUIScreenGizmoAxis::X);
    const bool bActiveY = GizmoManager.GetUIScreenInteractionState().ActiveAxis == static_cast<int32>(EUIScreenGizmoAxis::Y);
    const bool bActiveXY = GizmoManager.GetUIScreenInteractionState().ActiveAxis == static_cast<int32>(EUIScreenGizmoAxis::XY);
    const ImU32 XColor = (bHoverX || bActiveX) ? IM_COL32(255, 120, 120, 255) : IM_COL32(230, 70, 70, 255);
    const ImU32 YColor = (bHoverY || bActiveY) ? IM_COL32(120, 255, 160, 255) : IM_COL32(70, 210, 110, 255);
    const ImU32 CenterColor = (bHoverXY || bActiveXY) ? IM_COL32(120, 190, 255, 255) : IM_COL32(26, 138, 245, 255);

    ImDrawList *DrawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
    const ImVec2 Center(CenterX, CenterY);
    DrawList->AddLine(Center, ImVec2(CenterX + AxisLength, CenterY), XColor, Thickness);
    DrawList->AddTriangleFilled(ImVec2(CenterX + AxisLength + 8.0f, CenterY),
                                ImVec2(CenterX + AxisLength - 4.0f, CenterY - 6.0f),
                                ImVec2(CenterX + AxisLength - 4.0f, CenterY + 6.0f), XColor);
    DrawList->AddLine(Center, ImVec2(CenterX, CenterY + AxisLength), YColor, Thickness);
    DrawList->AddTriangleFilled(ImVec2(CenterX, CenterY + AxisLength + 8.0f),
                                ImVec2(CenterX - 6.0f, CenterY + AxisLength - 4.0f),
                                ImVec2(CenterX + 6.0f, CenterY + AxisLength - 4.0f), YColor);
    DrawList->AddRectFilled(ImVec2(CenterX - CenterHalf, CenterY - CenterHalf),
                            ImVec2(CenterX + CenterHalf, CenterY + CenterHalf), CenterColor, 2.0f);
    DrawList->AddRect(ImVec2(CenterX - CenterHalf, CenterY - CenterHalf),
                      ImVec2(CenterX + CenterHalf, CenterY + CenterHalf), IM_COL32(255, 255, 255, 220), 2.0f, 0, 1.5f);
}
