#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/Viewport/SkeletalMeshPreviewViewportClient.h"

#include "AssetEditor/SkeletalMesh/Gizmo/BoneTransformGizmoTarget.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshPreviewPoseController.h"
#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"

#include "Component/GizmoVisualComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Log.h"
#include "Core/RayTypes.h"
#include "Debug/DrawDebugHelpers.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Engine/Input/InputManager.h"
#include "Object/Object.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Viewport/Viewport.h"
#include "Math/Rotator.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

namespace
{
bool IsAnyPreviewMouseButtonDownForContextSwitchGuard()
{
    FInputManager& Input = FInputManager::Get();
    return Input.IsMouseButtonDown(FInputManager::MOUSE_LEFT) ||
           Input.IsMouseButtonDown(FInputManager::MOUSE_RIGHT) ||
           ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
           ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
           ImGui::IsMouseDown(ImGuiMouseButton_Middle);
}

const char *PreviewModeToText(ESkeletalMeshPreviewMode Mode)
{
    switch (Mode)
    {
    case ESkeletalMeshPreviewMode::ReferencePose:
        return "Reference Pose";
    case ESkeletalMeshPreviewMode::SkinnedPose:
        return "Skinned Pose";
    default:
        return "Unknown";
    }
}

float ClampFloat(float Value, float MinValue, float MaxValue)
{
    return (std::max)(MinValue, (std::min)(Value, MaxValue));
}

bool IsFiniteVector(const FVector& Value)
{
    return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
}

float GetComponentWorldRadiusScale(const USkeletalMeshComponent* Component)
{
    if (!Component)
    {
        return 1.0f;
    }

    const FVector Scale = Component->GetWorldScale();
    const float MaxScale = (std::max)((std::max)(std::abs(Scale.X), std::abs(Scale.Y)), std::abs(Scale.Z));
    return std::isfinite(MaxScale) && MaxScale > 1.0e-4f ? MaxScale : 1.0f;
}

FVector GetBoneWorldPosition(const USkeletalMeshComponent* Component, const FMatrix& BoneComponentMatrix)
{
    if (!Component)
    {
        return BoneComponentMatrix.GetLocation();
    }

    // Mesh rendering uses ComponentSpaceBone * ComponentWorld.
    // Skeleton debug/picking must use the same space; otherwise a non-identity preview component
    // transform makes bones look much larger/smaller than the mesh after context switches.
    return (BoneComponentMatrix * Component->GetWorldMatrix()).GetLocation();
}

void SyncLegacySelectedBoneIndex(FSkeletalMeshEditorState* State, const FSkeletalMeshSelectionManager* SelectionManager)
{
    if (!State)
    {
        return;
    }

    State->SelectedBoneIndex = SelectionManager ? SelectionManager->GetPrimaryBoneIndex() : -1;
}

int32 ResolveSelectedBoneIndex(const FSkeletalMeshEditorState* State, const FSkeletalMeshSelectionManager* SelectionManager)
{
    if (SelectionManager)
    {
        return SelectionManager->GetPrimaryBoneIndex();
    }

    return State ? State->SelectedBoneIndex : -1;
}

float ComputeAverageBoneLength(const TArray<FBoneInfo>& Bones, const FSkeletonPose& Pose)
{
    float TotalLength = 0.0f;
    int32 ConnectedBoneCount = 0;

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
    {
        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        if (ParentIndex < 0 || ParentIndex >= static_cast<int32>(Bones.size()))
        {
            continue;
        }

        const FVector BonePosition = Pose.ComponentTransforms[BoneIndex].GetLocation();
        const FVector ParentPosition = Pose.ComponentTransforms[ParentIndex].GetLocation();
        const float BoneLength = FVector::Distance(BonePosition, ParentPosition);
        if (!std::isfinite(BoneLength) || BoneLength <= 1.0e-4f)
        {
            continue;
        }

        TotalLength += BoneLength;
        ++ConnectedBoneCount;
    }

    return ConnectedBoneCount > 0 ? (TotalLength / static_cast<float>(ConnectedBoneCount)) : 0.0f;
}

float ComputePoseBoundsRadius(const FSkeletonPose& Pose)
{
    if (Pose.ComponentTransforms.empty())
    {
        return 0.0f;
    }

    bool bHasFinitePosition = false;
    FVector Min = FVector::ZeroVector;
    FVector Max = FVector::ZeroVector;

    for (const FMatrix& BoneTransform : Pose.ComponentTransforms)
    {
        const FVector Position = BoneTransform.GetLocation();
        if (!IsFiniteVector(Position))
        {
            continue;
        }

        if (!bHasFinitePosition)
        {
            Min = Position;
            Max = Position;
            bHasFinitePosition = true;
            continue;
        }

        Min.X = (std::min)(Min.X, Position.X);
        Min.Y = (std::min)(Min.Y, Position.Y);
        Min.Z = (std::min)(Min.Z, Position.Z);
        Max.X = (std::max)(Max.X, Position.X);
        Max.Y = (std::max)(Max.Y, Position.Y);
        Max.Z = (std::max)(Max.Z, Position.Z);
    }

    if (!bHasFinitePosition)
    {
        return 0.0f;
    }

    const FVector Extent = (Max - Min) * 0.5f;
    return std::sqrt(Extent.X * Extent.X + Extent.Y * Extent.Y + Extent.Z * Extent.Z);
}

float ComputeBoneSphereRadius(const TArray<FBoneInfo>& Bones, const FSkeletonPose& Pose)
{
    const float BoundsRadius = ComputePoseBoundsRadius(Pose);
    const float BoundsDrivenRadius = ClampFloat(BoundsRadius * 0.015f, 0.02f, 0.18f);

    const float AverageBoneLength = ComputeAverageBoneLength(Bones, Pose);
    if (AverageBoneLength <= 1.0e-4f)
    {
        return BoundsDrivenRadius;
    }

    const float LengthDrivenRadius = ClampFloat(AverageBoneLength * 0.18f, 0.02f, 0.18f);
    return ClampFloat((BoundsDrivenRadius + LengthDrivenRadius) * 0.5f, 0.02f, 0.18f);
}

float ComputeBoneConnectionBaseRadius(float BoneLength, float ReferenceSphereRadius)
{
    const float MinBaseRadius = ReferenceSphereRadius * 0.85f;
    const float MaxBaseRadius = (std::max)(MinBaseRadius, ReferenceSphereRadius * 2.0f);
    return ClampFloat(BoneLength * 0.08f, MinBaseRadius, MaxBaseRadius);
}

void BuildBoneDebugRadii(
    const TArray<FBoneInfo>& Bones,
    const FSkeletonPose& Pose,
    float BoneDebugScale,
    TArray<float>& OutBoneSphereRadii,
    TArray<float>& OutConnectionBaseRadii)
{
    const int32 BoneCount = static_cast<int32>(Bones.size());
    const float DefaultSphereRadius = ComputeBoneSphereRadius(Bones, Pose);
    constexpr float SphereRadiusScale = 10.f;
    const float ResolvedBoneDebugScale = ClampFloat(BoneDebugScale, 0.01f, 100.0f);

    OutBoneSphereRadii.resize(BoneCount, DefaultSphereRadius * SphereRadiusScale * ResolvedBoneDebugScale);
    OutConnectionBaseRadii.resize(BoneCount, 0.0f);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        if (ParentIndex < 0 || ParentIndex >= BoneCount)
        {
            continue;
        }

        const FVector BonePosition = Pose.ComponentTransforms[BoneIndex].GetLocation();
        const FVector ParentPosition = Pose.ComponentTransforms[ParentIndex].GetLocation();
        if (!IsFiniteVector(BonePosition) || !IsFiniteVector(ParentPosition))
        {
            continue;
        }

        const float BoneLength = FVector::Distance(ParentPosition, BonePosition);
        const float ConnectionBaseRadius =
            ComputeBoneConnectionBaseRadius(BoneLength, DefaultSphereRadius) * ResolvedBoneDebugScale;
        OutConnectionBaseRadii[BoneIndex] = ConnectionBaseRadius;
        const float DesiredSphereRadius = ConnectionBaseRadius * SphereRadiusScale;
        OutBoneSphereRadii[BoneIndex] = (std::max)(OutBoneSphereRadii[BoneIndex], DesiredSphereRadius);
        OutBoneSphereRadii[ParentIndex] = (std::max)(OutBoneSphereRadii[ParentIndex], DesiredSphereRadius);
    }
}

TArray<int32> BuildBoneSelectionOrder(int32 BoneCount)
{
    TArray<int32> BoneOrder;
    BoneOrder.reserve((std::max)(0, BoneCount));

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        BoneOrder.push_back(BoneIndex);
    }

    return BoneOrder;
}

bool IntersectRaySphere(const FRay& Ray, const FVector& Center, float Radius, float& OutDistance)
{
    if (!IsFiniteVector(Ray.Origin) || !IsFiniteVector(Ray.Direction) || !IsFiniteVector(Center) || !std::isfinite(Radius) || Radius <= 1.0e-4f)
    {
        return false;
    }

    const float DirectionLengthSq = Ray.Direction.Dot(Ray.Direction);
    if (!std::isfinite(DirectionLengthSq) || DirectionLengthSq <= 1.0e-8f)
    {
        return false;
    }

    const FVector Offset = Ray.Origin - Center;
    const float A = DirectionLengthSq;
    const float B = 2.0f * Ray.Direction.Dot(Offset);
    const float C = Offset.Dot(Offset) - Radius * Radius;
    const float Discriminant = B * B - 4.0f * A * C;
    if (!std::isfinite(Discriminant) || Discriminant < 0.0f)
    {
        return false;
    }

    const float SqrtDiscriminant = std::sqrt(Discriminant);
    const float InvDenominator = 0.5f / A;
    const float NearT = (-B - SqrtDiscriminant) * InvDenominator;
    const float FarT = (-B + SqrtDiscriminant) * InvDenominator;
    const float HitDistance = NearT >= 0.0f ? NearT : FarT;
    if (!std::isfinite(HitDistance) || HitDistance < 0.0f)
    {
        return false;
    }

    OutDistance = HitDistance;
    return true;
}

FVector ComputePreviewLightDirection(float YawDegrees, float PitchDegrees)
{
    constexpr float DegToRad = 3.14159265358979323846f / 180.0f;
    const float Yaw = YawDegrees * DegToRad;
    const float Pitch = PitchDegrees * DegToRad;
    const float CosPitch = std::cos(Pitch);
    FVector Direction(CosPitch * std::cos(Yaw), CosPitch * std::sin(Yaw), std::sin(Pitch));
    if (!IsFiniteVector(Direction) || Direction.Length() <= 1.0e-4f)
    {
        Direction = FVector(-0.45f, -0.55f, -0.70f);
    }
    return Direction.Normalized();
}

bool BuildBonePickingBasis(const FVector& SegmentStart, const FVector& SegmentEnd, FVector& OutDirection, FVector& OutRight, FVector& OutUp, float& OutLength)
{
    if (!IsFiniteVector(SegmentStart) || !IsFiniteVector(SegmentEnd))
    {
        return false;
    }

    const FVector Segment = SegmentEnd - SegmentStart;
    const float SegmentLength = Segment.Length();
    if (!std::isfinite(SegmentLength) || SegmentLength <= 1.0e-4f)
    {
        return false;
    }

    const FVector Direction = Segment / SegmentLength;
    FVector ReferenceAxis = FVector::UpVector;
    if (std::abs(Direction.Dot(ReferenceAxis)) > 0.95f)
    {
        ReferenceAxis = FVector::RightVector;
    }
    if (std::abs(Direction.Dot(ReferenceAxis)) > 0.95f)
    {
        ReferenceAxis = FVector::ForwardVector;
    }

    FVector Right = Direction.Cross(ReferenceAxis);
    const float RightLength = Right.Length();
    if (!std::isfinite(RightLength) || RightLength <= 1.0e-4f)
    {
        return false;
    }
    Right /= RightLength;

    FVector Up = Right.Cross(Direction);
    const float UpLength = Up.Length();
    if (!std::isfinite(UpLength) || UpLength <= 1.0e-4f)
    {
        return false;
    }
    Up /= UpLength;

    OutDirection = Direction;
    OutRight = Right;
    OutUp = Up;
    OutLength = SegmentLength;
    return true;
}

bool IntersectRayBoneOBB(
    const FRay& Ray,
    const FVector& SegmentStart,
    const FVector& SegmentEnd,
    float HalfThickness,
    float& OutRayDistance)
{
    if (!IsFiniteVector(Ray.Origin) || !IsFiniteVector(Ray.Direction) || !std::isfinite(HalfThickness) || HalfThickness <= 1.0e-4f)
    {
        return false;
    }

    FVector Direction = FVector::ZeroVector;
    FVector Right = FVector::ZeroVector;
    FVector Up = FVector::ZeroVector;
    float SegmentLength = 0.0f;
    if (!BuildBonePickingBasis(SegmentStart, SegmentEnd, Direction, Right, Up, SegmentLength))
    {
        return IntersectRaySphere(Ray, SegmentEnd, HalfThickness, OutRayDistance);
    }

    const FVector Center = (SegmentStart + SegmentEnd) * 0.5f;
    const float HalfLength = SegmentLength * 0.5f + HalfThickness * 0.25f;
    const FVector Extent(HalfLength, HalfThickness, HalfThickness);
    const FVector Delta = Center - Ray.Origin;
    const FVector Axes[3] = { Direction, Right, Up };

    float TMin = 0.0f;
    float TMax = (std::numeric_limits<float>::max)();

    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        const float AxisProjection = Axes[AxisIndex].Dot(Delta);
        const float RayProjection = Axes[AxisIndex].Dot(Ray.Direction);
        const float AxisExtent = Extent.Data[AxisIndex];

        if (std::abs(RayProjection) <= 1.0e-6f)
        {
            if (std::abs(AxisProjection) > AxisExtent)
            {
                return false;
            }
            continue;
        }

        const float InvProjection = 1.0f / RayProjection;
        float EntryT = (AxisProjection - AxisExtent) * InvProjection;
        float ExitT = (AxisProjection + AxisExtent) * InvProjection;
        if (EntryT > ExitT)
        {
            std::swap(EntryT, ExitT);
        }

        TMin = (std::max)(TMin, EntryT);
        TMax = (std::min)(TMax, ExitT);
        if (TMin > TMax)
        {
            return false;
        }
    }

    OutRayDistance = TMin >= 0.0f ? TMin : TMax;
    if (!std::isfinite(OutRayDistance) || OutRayDistance < 0.0f)
    {
        return false;
    }

    return true;
}

bool IsLegacyImGuiKeyDown(int Key)
{
    const ImGuiKey ImKey = [&]() {
        switch (Key)
        {
        case 'A': return ImGuiKey_A;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case ' ': return ImGuiKey_Space;
        case 'Q': return ImGuiKey_Q;
        case 'S': return ImGuiKey_S;
        case 'W': return ImGuiKey_W;
        default: return ImGuiKey_None;
        }
    }();
    return ImKey != ImGuiKey_None && ImGui::IsKeyDown(ImKey);
}

bool IsLegacyImGuiKeyPressed(int Key)
{
    const ImGuiKey ImKey = [&]() {
        switch (Key)
        {
        case 'A': return ImGuiKey_A;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case ' ': return ImGuiKey_Space;
        case 'Q': return ImGuiKey_Q;
        case 'S': return ImGuiKey_S;
        case 'W': return ImGuiKey_W;
        default: return ImGuiKey_None;
        }
    }();
    return ImKey != ImGuiKey_None && ImGui::IsKeyPressed(ImKey, false);
}

FVector MakeOrbitCameraLocation(const FVector &Target, float Distance, float YawDeg, float PitchDeg)
{
    constexpr float DegToRad = 3.14159265358979323846f / 180.0f;
    const float Yaw = YawDeg * DegToRad;
    const float Pitch = PitchDeg * DegToRad;

    const float CP = std::cos(Pitch);
    const float SP = std::sin(Pitch);
    const float CY = std::cos(Yaw);
    const float SY = std::sin(Yaw);

    const FVector Forward(CP * CY, CP * SY, SP);
    return Target - Forward * Distance;
}

EViewMode ToRuntimeViewMode(ESkeletalMeshPreviewViewMode Mode)
{
    switch (Mode)
    {
    case ESkeletalMeshPreviewViewMode::Unlit: return EViewMode::Unlit;
    case ESkeletalMeshPreviewViewMode::LitGouraud: return EViewMode::Lit_Gouraud;
    case ESkeletalMeshPreviewViewMode::LitLambert: return EViewMode::Lit_Lambert;
    case ESkeletalMeshPreviewViewMode::Wireframe: return EViewMode::Wireframe;
    case ESkeletalMeshPreviewViewMode::SceneDepth: return EViewMode::SceneDepth;
    case ESkeletalMeshPreviewViewMode::WorldNormal: return EViewMode::WorldNormal;
    case ESkeletalMeshPreviewViewMode::LightCulling: return EViewMode::LightCulling;
    case ESkeletalMeshPreviewViewMode::Lit:
    default: return EViewMode::Lit_Phong;
    }
}

ELevelViewportType ToRuntimeViewportType(ESkeletalMeshPreviewViewportType Type)
{
    switch (Type)
    {
    case ESkeletalMeshPreviewViewportType::Top: return ELevelViewportType::Top;
    case ESkeletalMeshPreviewViewportType::Bottom: return ELevelViewportType::Bottom;
    case ESkeletalMeshPreviewViewportType::Left: return ELevelViewportType::Left;
    case ESkeletalMeshPreviewViewportType::Right: return ELevelViewportType::Right;
    case ESkeletalMeshPreviewViewportType::Front: return ELevelViewportType::Front;
    case ESkeletalMeshPreviewViewportType::Back: return ELevelViewportType::Back;
    case ESkeletalMeshPreviewViewportType::FreeOrtho: return ELevelViewportType::FreeOrthographic;
    case ESkeletalMeshPreviewViewportType::Perspective:
    default: return ELevelViewportType::Perspective;
    }
}

void ApplySkeletalMeshGizmoStateToManager(const FSkeletalMeshEditorState* State, FGizmoManager& Manager)
{
    if (!State)
    {
        return;
    }

    Manager.SetMode(State->GizmoMode);
    Manager.SetSpace(State->GizmoSpace);
    Manager.SetSnapSettings(State->bEnableTranslationSnap, State->TranslationSnapSize,
                            State->bEnableRotationSnap, State->RotationSnapSize,
                            State->bEnableScaleSnap, State->ScaleSnapSize);
    Manager.SyncVisualFromTarget();
}

FRotator GetFixedViewportRotation(ESkeletalMeshPreviewViewportType Type)
{
    switch (Type)
    {
    case ESkeletalMeshPreviewViewportType::Top: return FRotator(90.0f, 0.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::Bottom: return FRotator(-90.0f, 0.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::Left: return FRotator(0.0f, 90.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::Right: return FRotator(0.0f, -90.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::Front: return FRotator(0.0f, 180.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::Back: return FRotator(0.0f, 0.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::FreeOrtho:
    case ESkeletalMeshPreviewViewportType::Perspective:
    default: return FRotator::ZeroRotator;
    }
}
} // namespace

FSkeletalMeshPreviewViewportClient::~FSkeletalMeshPreviewViewportClient()
{
    Shutdown();
}

void FSkeletalMeshPreviewViewportClient::Init(FWindowsWindow *InWindow)
{
    FEditorViewportClient::Init(InWindow);
    EnsurePreviewObjects();
}

void FSkeletalMeshPreviewViewportClient::Shutdown()
{
    ReleasePreviewObjects();
    FEditorViewportClient::Shutdown();
}

void FSkeletalMeshPreviewViewportClient::SetPoseController(std::shared_ptr<FSkeletalMeshPreviewPoseController> InPoseController)
{
    PoseController = std::move(InPoseController);
    if (PoseController)
    {
        PoseController->BindPreviewComponent(PreviewComponent);
    }
}

void FSkeletalMeshPreviewViewportClient::BindEditorContext(FSkeletalMeshEditorState* InState,
                                                            FSkeletalMeshSelectionManager* InSelectionManager)
{
    State = InState;
    SelectionManager = InSelectionManager;
}

void FSkeletalMeshPreviewViewportClient::ActivateEditorContext()
{
    FEditorViewportClient::ActivateEditorContext();

    // The preview scene is kept alive per tab, but debug skeleton lines are transient.
    // Clear one-frame lines on context enter/exit so a reactivated tab does not render stale
    // bone visuals from the previous panel rect or component transform.
    PreviewScene.GetScene().GetDebugDrawQueue().Clear();

    bNeedsDeferredTargetSync = true;
    bHasRenderedViewportFrameSinceActivation = false;
    bSuppressViewportInputUntilMouseRelease = true;

    if (!CanProcessLiveViewportWork())
    {
        GizmoManager.AbortLiveInteractionWithoutApplying();
        GizmoTargetBoneIndex = -1;
        return;
    }

    // Do not rebuild a live gizmo target during context activation itself.
    // Activation is only ownership restoration; the first normal live Tick will sync selection -> target
    // after mouse capture has settled. This prevents stale input from applying to the old bone target.
    GizmoManager.ResetVisualInteractionState();

    // 탭 복귀 시 자신의 State(GizmoMode/GizmoSpace/Snap)를 즉시 GizmoManager에 적용한다.
    // Tick/BuildRenderRequest에서도 ApplyEditorStateToViewport()가 이를 수행하지만,
    // ActivateEditorContext()와 첫 Tick 사이의 구간에서도 올바른 모드가 보장되도록 여기서도 적용한다.
    // 특히 Level Editor에서 기즈모 모드를 바꾼 후 Asset Editor로 복귀할 때,
    // Asset Editor의 GizmoManager가 자신의 State 값으로 즉시 덮어써지므로
    // Level Editor의 마지막 상태가 잔류하는 현상이 차단된다.
    ApplySkeletalMeshGizmoStateToManager(State, GizmoManager);
}

void FSkeletalMeshPreviewViewportClient::DeactivateEditorContext()
{
    bNeedsDeferredTargetSync = false;
    bHasRenderedViewportFrameSinceActivation = false;
    bSuppressViewportInputUntilMouseRelease = false;

    // Exit live context completely. Keep preview scene/camera/selection state in the owning editor,
    // but detach transient gizmo target, drag session, hover/pressed axis, and input capture.
    FEditorViewportClient::DeactivateEditorContext();

    PreviewScene.GetScene().GetDebugDrawQueue().Clear();
    GizmoManager.SetInteractionPolicy(EGizmoInteractionPolicy::VisualOnly);
    GizmoManager.AbortLiveInteractionWithoutApplying();
    GizmoManager.ResetVisualInteractionState();
    GizmoTargetBoneIndex = -1;
    State = nullptr;
    SelectionManager = nullptr;
}

void FSkeletalMeshPreviewViewportClient::EnsurePreviewObjects()
{
    if (bPreviewObjectsInitialized)
    {
        return;
    }

    UE_LOG_CATEGORY(AssetEditor, Info, "[SkeletalPreview] EnsurePreviewObjects: begin");

    PreviewComponent = UObjectManager::Get().CreateObject<USkeletalMeshComponent>();
    if (PoseController)
    {
        PoseController->BindPreviewComponent(PreviewComponent);
    }
    GizmoManager.SetInteractionPolicy(EGizmoInteractionPolicy::VisualOnly);
    GizmoManager.EnsureVisualComponent();
    GizmoManager.RegisterVisualToScene(&PreviewScene.GetScene());
    GizmoManager.SetSpace(EGizmoSpace::Local);

    RenderOptions.ViewMode = EViewMode::Lit_Phong;
    RenderOptions.ShowFlags.bGrid = true;
    RenderOptions.ShowFlags.bWorldAxis = true;
    RenderOptions.ShowFlags.bGizmo = true;
    RenderOptions.ShowFlags.bSceneBVH = false;
    RenderOptions.ShowFlags.bOctree = false;
    RenderOptions.ShowFlags.bWorldBound = false;
    RenderOptions.ShowFlags.bLightVisualization = false;
    RenderOptions.GridSpacing = 1.0f;
    RenderOptions.GridHalfLineCount = 20;

    ResetPreviewCamera();
    bPreviewObjectsInitialized = true;
    UE_LOG_CATEGORY(AssetEditor, Info, "[SkeletalPreview] EnsurePreviewObjects: complete");
}

void FSkeletalMeshPreviewViewportClient::ReleasePreviewObjects()
{
    PreviewScene.GetScene().GetDebugDrawQueue().Clear();

    if (PreviewProxy)
    {
        PreviewScene.RemovePrimitive(PreviewProxy);
        PreviewProxy = nullptr;
    }

    GizmoManager.ClearTarget();
    GizmoManager.ReleaseVisualComponent();

    if (PoseController)
    {
        PoseController->BindPreviewComponent(nullptr);
    }

    if (PreviewComponent)
    {
        UObjectManager::Get().DestroyObject(PreviewComponent);
        PreviewComponent = nullptr;
    }

    PreviewMesh = nullptr;
    bPreviewObjectsInitialized = false;
}

void FSkeletalMeshPreviewViewportClient::SetPreviewMesh(USkeletalMesh *InMesh)
{
    EnsurePreviewObjects();

    if (PreviewMesh == InMesh)
    {
        return;
    }

    UE_LOG_CATEGORY(AssetEditor, Info, "[SkeletalPreview] SetPreviewMesh: mesh=%s",
        InMesh ? InMesh->GetFName().ToString().c_str() : "None");

    PreviewMesh = InMesh;
    PreviewScene.GetScene().GetDebugDrawQueue().Clear();
    GizmoManager.ClearTarget();
    GizmoTargetBoneIndex = -1;
    if (PreviewComponent)
    {
        PreviewComponent->SetSkeletalMesh(PreviewMesh);
        if (PoseController)
        {
            PoseController->BindPreviewComponent(PreviewComponent);
            PoseController->InitializeFromComponentPose();
        }
    }

    RebuildPreviewProxy();
    FramePreviewMesh();
    UE_LOG_CATEGORY(AssetEditor, Info, "[SkeletalPreview] SetPreviewMesh: complete");
}

void FSkeletalMeshPreviewViewportClient::RebuildPreviewProxy()
{
    UE_LOG_CATEGORY(AssetEditor, Info, "[SkeletalPreview] RebuildPreviewProxy: begin");
    if (PreviewProxy)
    {
        PreviewScene.RemovePrimitive(PreviewProxy);
        PreviewProxy = nullptr;
    }

    if (!PreviewComponent || !PreviewMesh)
    {
        return;
    }

    // PreviewComponent는 Level World에 등록하지 않는다.
    // 대신 PreviewScene에 직접 PrimitiveSceneProxy를 등록해 renderer가 같은 DrawCommand 경로를 타게 한다.
    PreviewProxy = PreviewScene.AddPrimitive(PreviewComponent);
    UE_LOG_CATEGORY(AssetEditor, Info, "[SkeletalPreview] RebuildPreviewProxy: complete proxy=%p", PreviewProxy);
}


bool FSkeletalMeshPreviewViewportClient::CanProcessLiveViewportWork() const
{
    return CanProcessLiveContextWork() && State != nullptr;
}

bool FSkeletalMeshPreviewViewportClient::CanProcessViewportInput() const
{
    return CanProcessLiveViewportWork() && (IsHovered() || IsActive() || GizmoManager.IsDragging());
}

bool FSkeletalMeshPreviewViewportClient::ShouldBlockViewportInteractionUntilContextSettles()
{
    // This viewport ticks before the ImGui panel refreshes hover/focus/screen-rect state.
    // Immediately after tab activation, IsActive() is already true, but the viewport may still
    // be carrying the previous visible frame's rect. Swallow input until we have rendered once
    // and observed a full mouse-release boundary after activation.
    if (!bHasRenderedViewportFrameSinceActivation)
    {
        return true;
    }

    if (!bSuppressViewportInputUntilMouseRelease)
    {
        return false;
    }

    if (IsAnyPreviewMouseButtonDownForContextSwitchGuard())
    {
        return true;
    }

    bSuppressViewportInputUntilMouseRelease = false;
    return true;
}

void FSkeletalMeshPreviewViewportClient::ApplyEditorStateToViewport()
{
    if (!CanProcessLiveViewportWork())
    {
        GizmoManager.SetInteractionPolicy(EGizmoInteractionPolicy::VisualOnly);
        return;
    }

    SyncRenderOptionsFromState();
    ApplyViewportTypeToCamera();

    if (State)
    {
        ApplySkeletalMeshGizmoStateToManager(State, GizmoManager);

        // 의도치 않은 pose 변경을 막기 위해 Pose Edit Mode가 켜진 경우에만 실제 드래그를 허용한다.
        const bool bHasEditableBone = ResolveSelectedBoneIndex(State, SelectionManager) >= 0;
        const bool bAllowBoneGizmoInteraction = State->bEnablePoseEditMode && State->bShowGizmo && bHasEditableBone;
        GizmoManager.SetInteractionPolicy(bAllowBoneGizmoInteraction
            ? EGizmoInteractionPolicy::Interactive
            : EGizmoInteractionPolicy::VisualOnly);
    }
}

void FSkeletalMeshPreviewViewportClient::SyncRenderOptionsFromState()
{
    if (!State)
    {
        return;
    }

    RenderOptions.ViewMode = ToRuntimeViewMode(State->PreviewViewMode);
    RenderOptions.ViewportType = ToRuntimeViewportType(State->PreviewViewportType);

    RenderOptions.ShowFlags.bPrimitives = State->bShowPrimitives;
    RenderOptions.ShowFlags.bGrid = State->bShowGrid;
    RenderOptions.ShowFlags.bWorldAxis = State->bShowWorldAxis;
    RenderOptions.ShowFlags.bGizmo = State->bShowGizmo;
    RenderOptions.ShowFlags.bBillboardText = State->bShowBillboardText;
    RenderOptions.ShowFlags.bSkeletalMesh = State->bShowSkeletalMesh;
    RenderOptions.ShowFlags.bSceneBVH = State->bShowSceneBVH;
    RenderOptions.ShowFlags.bOctree = State->bShowOctree;
    RenderOptions.ShowFlags.bWorldBound = State->bShowWorldBound;
    RenderOptions.ShowFlags.bLightVisualization = State->bShowLightVisualization;
    RenderOptions.ShowFlags.bViewLightCulling = State->PreviewViewMode == ESkeletalMeshPreviewViewMode::LightCulling;

    RenderOptions.GridSpacing = State->GridSpacing;
    RenderOptions.GridHalfLineCount = State->GridHalfLineCount;
    RenderOptions.GridRenderSettings.LineThickness = State->GridLineThickness;
    RenderOptions.GridRenderSettings.MajorLineThickness = State->GridMajorLineThickness;
    RenderOptions.GridRenderSettings.MajorLineInterval = State->GridMajorLineInterval;
    RenderOptions.GridRenderSettings.MinorIntensity = State->GridMinorIntensity;
    RenderOptions.GridRenderSettings.MajorIntensity = State->GridMajorIntensity;
    RenderOptions.GridRenderSettings.AxisThickness = State->AxisThickness;
    RenderOptions.GridRenderSettings.AxisIntensity = State->AxisIntensity;
    RenderOptions.DebugLineThickness = State->DebugLineThickness;
    RenderOptions.ActorHelperBillboardScale = State->BillboardIconScale;
    RenderOptions.CameraMoveSensitivity = State->CameraSpeed;

    PreviewScene.SetPreviewLighting(
        State->bPreviewLighting,
        State->PreviewAmbientLightIntensity,
        State->PreviewAmbientLightColor,
        State->PreviewDirectionalLightIntensity,
        State->PreviewDirectionalLightColor,
        ComputePreviewLightDirection(State->PreviewLightYaw, State->PreviewLightPitch));
}

void FSkeletalMeshPreviewViewportClient::ApplyViewportTypeToCamera()
{
    if (!State)
    {
        return;
    }

    const bool bViewportTypeChanged = !bHasAppliedViewportType || LastAppliedViewportType != State->PreviewViewportType;
    bHasAppliedViewportType = true;
    LastAppliedViewportType = State->PreviewViewportType;

    constexpr float DegToRad = 3.14159265358979323846f / 180.0f;
    ViewCamera.SetFOV(State->CameraFOV * DegToRad);
    ViewCamera.SetOrthoWidth((std::max)(0.1f, State->CameraOrthoWidth));

    const bool bOrtho = State->PreviewViewportType != ESkeletalMeshPreviewViewportType::Perspective;
    ViewCamera.SetOrthographic(bOrtho);

    if (!bOrtho)
    {
        return;
    }

    if (State->PreviewViewportType == ESkeletalMeshPreviewViewportType::FreeOrtho)
    {
        // Free Ortho는 현재 자유비행 카메라 회전을 유지하고 projection만 ortho로 바꾼다.
        return;
    }

    if (!bViewportTypeChanged)
    {
        return;
    }

    // 고정 ortho 방향은 전환 시점에만 적용한다.
    // 매 tick마다 위치/회전을 덮어쓰면 Preview Viewport의 자유비행 입력이 즉시 무효화된다.
    const FRotator Rotation = GetFixedViewportRotation(State->PreviewViewportType);
    ViewCamera.SetWorldRotation(Rotation);
    const FVector Forward = ViewCamera.GetForwardVector();
    const float Distance = (std::max)(1.0f, OrbitDistance);
    ViewCamera.SetWorldLocation(OrbitTarget - Forward * Distance);
    GetCameraController().SyncTargetToCamera();

    State->CameraOrthoWidth = (std::max)(State->CameraOrthoWidth, OrbitDistance * 2.0f);
    ViewCamera.SetOrthoWidth(State->CameraOrthoWidth);
}

void FSkeletalMeshPreviewViewportClient::ResetPreviewCamera()
{
    OrbitTarget = FVector::ZeroVector;
    OrbitDistance = 6.0f;
    OrbitYaw = 180.0f;
    OrbitPitch = -10.0f;

    ViewCamera.SetFOV(60.0f * 3.14159265358979323846f / 180.0f);
    ViewCamera.SetOrthographic(false);

    // SkeletalMesh Editor viewport도 Level Editor와 같은 자유비행 카메라로 사용한다.
    // 초기 위치만 preview mesh를 보기 좋은 위치로 잡고, 이후 입력은 camera 자체를 이동/회전한다.
    GetCameraController().ResetLookAt(FVector(OrbitDistance, 0.0f, OrbitDistance * 0.35f), OrbitTarget);
}

void FSkeletalMeshPreviewViewportClient::FramePreviewMesh()
{
    if (!PreviewMesh || !PreviewMesh->GetSkeletalMeshAsset())
    {
        ResetPreviewCamera();
        return;
    }

    const FSkeletalMesh *MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
    if (!MeshAsset || MeshAsset->Vertices.empty())
    {
        ResetPreviewCamera();
        return;
    }

    bool bFoundFiniteVertex = false;
    FVector Min = FVector::ZeroVector;
    FVector Max = FVector::ZeroVector;
    for (const FNormalVertex &Vertex : MeshAsset->Vertices)
    {
        if (!IsFiniteVector(Vertex.pos))
        {
            continue;
        }

        if (!bFoundFiniteVertex)
        {
            Min = Vertex.pos;
            Max = Vertex.pos;
            bFoundFiniteVertex = true;
            continue;
        }

        Min.X = (std::min)(Min.X, Vertex.pos.X);
        Min.Y = (std::min)(Min.Y, Vertex.pos.Y);
        Min.Z = (std::min)(Min.Z, Vertex.pos.Z);
        Max.X = (std::max)(Max.X, Vertex.pos.X);
        Max.Y = (std::max)(Max.Y, Vertex.pos.Y);
        Max.Z = (std::max)(Max.Z, Vertex.pos.Z);
    }

    if (!bFoundFiniteVertex)
    {
        ResetPreviewCamera();
        return;
    }

    OrbitTarget = (Min + Max) * 0.5f;
    const FVector Extent = (Max - Min) * 0.5f;
    const float Radius = (std::max)(0.5f, std::sqrt(Extent.X * Extent.X + Extent.Y * Extent.Y + Extent.Z * Extent.Z));
    OrbitDistance = Radius * 2.8f;

    const FVector Forward = ViewCamera.GetForwardVector();
    GetCameraController().StartFocus(OrbitTarget, Forward, OrbitDistance, 0.18f);
}

void FSkeletalMeshPreviewViewportClient::Tick(float DeltaTime)
{
    if (!CanProcessLiveViewportWork())
    {
        GizmoManager.AbortLiveInteractionWithoutApplying();
        GizmoManager.ResetVisualInteractionState();
        return;
    }

    EnsurePreviewObjects();
    PreviewScene.GetScene().GetDebugDrawQueue().Tick(DeltaTime);
    ApplyEditorStateToViewport();

    if (bNeedsDeferredTargetSync)
    {
        if (IsAnyPreviewMouseButtonDownForContextSwitchGuard())
        {
            GizmoManager.ResetVisualInteractionState();
            return;
        }

        bNeedsDeferredTargetSync = false;
        SyncGizmoTargetFromSelection();

        // Context-switch safety: the first frame that rebuilds the gizmo target is
        // still considered a settle frame.  Do not process viewport input or gizmo
        // interaction in the same frame, otherwise a mouse/key state inherited from
        // the previous Level Editor drag can be applied immediately to this Asset
        // Editor target.
        GizmoManager.ResetVisualInteractionState();
        return;
    }
    else
    {
        SyncGizmoTargetFromSelection();
    }

    GetCameraController().SyncTargetToCamera();
    GetCameraController().TickFocus(DeltaTime);
    if (ShouldBlockViewportInteractionUntilContextSettles())
    {
        GizmoManager.ResetVisualInteractionState();
    }
    else
    {
        TickViewportInput(DeltaTime);
        TickGizmoInteraction();
    }

    if (State && State->bFramePreviewRequested)
    {
        FramePreviewMesh();
        State->bFramePreviewRequested = false;
    }

    if (PreviewComponent)
    {
        const bool bPoseEditSessionActive = PoseController && PoseController->HasActiveBoneGizmoSession();
        if (!bPoseEditSessionActive)
        {
            PreviewComponent->RefreshSkinningForEditor(DeltaTime);
            // Do not re-initialize the pose controller from the component every frame.
            // The controller is initialized when the component/mesh is bound or explicitly reset.
            // Re-reading here can make a stale preview-component pose look like it was applied on tab reactivation.
        }
    }

    GizmoManager.ApplyScreenSpaceScaling(
        ViewCamera.GetWorldLocation(),
        ViewCamera.IsOrthogonal(),
        ViewCamera.GetOrthoWidth(),
        ViewportScreenRect.Height > 0.0f
            ? ViewportScreenRect.Height
            : (Viewport ? static_cast<float>(Viewport->GetHeight()) : 0.0f));
    GizmoManager.SetAxisMask(UGizmoVisualComponent::ComputeAxisMask(
        RenderOptions.ViewportType,
        GizmoManager.GetMode()));
}

const char *FSkeletalMeshPreviewViewportClient::GetViewportTooltipBarText() const
{
    return nullptr;
}


void FSkeletalMeshPreviewViewportClient::CycleGizmoModeFromShortcut()
{
    if (!State)
    {
        return;
    }

    switch (State->GizmoMode)
    {
    case EGizmoMode::Translate:
        State->GizmoMode = EGizmoMode::Rotate;
        break;
    case EGizmoMode::Rotate:
        State->GizmoMode = EGizmoMode::Scale;
        break;
    case EGizmoMode::Scale:
    default:
        State->GizmoMode = EGizmoMode::Translate;
        break;
    }

    State->bEnablePoseEditMode = true;
    ApplySkeletalMeshGizmoStateToManager(State, GizmoManager);
}

void FSkeletalMeshPreviewViewportClient::TickViewportInput(float DeltaTime)
{
    if (!CanProcessViewportInput())
    {
        return;
    }

    ImGuiIO &IO = ImGui::GetIO();
    const bool bRightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const bool bMiddleMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    const bool bAcceptViewportInput = IsHovered() || IsActive() || GizmoManager.IsDragging();
    if (!bAcceptViewportInput)
    {
        return;
    }

    // 기존 Level Editor 뷰포트처럼 Preview Viewport도 자유비행 카메라 입력을 처리한다.
    // - RMB 드래그: Look 회전
    // - MMB 드래그: 카메라 pan
    // - 휠: 카메라 dolly
    // - RMB + WASD/QE: 카메라 자유비행 이동
    // - F: 메시 프레이밍
    // - Space: 기즈모 모드 순환
    if (IsLegacyImGuiKeyPressed('F'))
    {
        FramePreviewMesh();
    }

    if (IsLegacyImGuiKeyPressed(' '))
    {
        CycleGizmoModeFromShortcut();
    }

    // 기즈모 드래그 중에는 카메라 입력이 동시에 먹지 않게 한다.
    if (GizmoManager.IsDragging())
    {
        return;
    }

    const bool bCanFreeLook = !State || State->PreviewViewportType == ESkeletalMeshPreviewViewportType::Perspective || State->PreviewViewportType == ESkeletalMeshPreviewViewportType::FreeOrtho;
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && bCanFreeLook)
    {
        const ImVec2 Delta = IO.MouseDelta;
        GetCameraController().Rotate(Delta.x * 0.25f, Delta.y * 0.25f);
    }

    const float CameraSpeed = State ? State->CameraSpeed : 5.0f;
    const float SpeedScale = (std::max)(0.1f, CameraSpeed);
    const float DistanceScale = (std::max)(0.5f, OrbitDistance);

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 Delta = IO.MouseDelta;
        const float PanScale = DistanceScale * 0.0015f * SpeedScale;
        GetCameraController().MoveLocalImmediate(FVector(0.0f, -Delta.x * PanScale, Delta.y * PanScale));
    }

	IO.MouseWheel = FInputManager::Get().GetMouseWheelDelta();
    if (IO.MouseWheel != 0.0f && IsHovered())
    {
        const float DollyAmount = IO.MouseWheel * DistanceScale * 0.08f * SpeedScale;
        GetCameraController().MoveLocalImmediate(FVector(DollyAmount, 0.0f, 0.0f));
    }

    if (bRightMouseDown)
    {
        FVector LocalMove = FVector::ZeroVector;
        if (IsLegacyImGuiKeyDown('W'))
        {
            LocalMove.X += 1.0f;
        }
        if (IsLegacyImGuiKeyDown('S'))
        {
            LocalMove.X -= 1.0f;
        }
        if (IsLegacyImGuiKeyDown('D'))
        {
            LocalMove.Y += 1.0f;
        }
        if (IsLegacyImGuiKeyDown('A'))
        {
            LocalMove.Y -= 1.0f;
        }
        if (IsLegacyImGuiKeyDown('E'))
        {
            LocalMove.Z += 1.0f;
        }
        if (IsLegacyImGuiKeyDown('Q'))
        {
            LocalMove.Z -= 1.0f;
        }

        if (!LocalMove.IsNearlyZero())
        {
            const float MoveLength = std::sqrt(LocalMove.X * LocalMove.X + LocalMove.Y * LocalMove.Y + LocalMove.Z * LocalMove.Z);
            if (MoveLength > 0.0001f)
            {
                const float MoveAmount = DistanceScale * SpeedScale * DeltaTime;
                GetCameraController().MoveLocalImmediate(LocalMove * (MoveAmount / MoveLength));
            }
        }
    }

    ApplyViewportTypeToCamera();
}


void FSkeletalMeshPreviewViewportClient::TickGizmoInteraction()
{
    if (!CanProcessViewportInput())
    {
        return;
    }

    if (!State || !PreviewComponent || !PreviewMesh || !SelectionManager)
    {
        return;
    }

    const bool bCanInteractWithGizmo =
        GizmoManager.HasValidTarget() &&
        State->bEnablePoseEditMode &&
        State->bShowGizmo &&
        ResolveSelectedBoneIndex(State, SelectionManager) >= 0 &&
        GizmoManager.CanInteract();

    ImGuiIO& IO = ImGui::GetIO();
    FInputManager& Input = FInputManager::Get();

    const bool bLeftPressed = Input.IsMouseButtonPressed(FInputManager::MOUSE_LEFT) || ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const bool bLeftDown = Input.IsMouseButtonDown(FInputManager::MOUSE_LEFT) || ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool bLeftReleased = Input.IsMouseButtonReleased(FInputManager::MOUSE_LEFT) || ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    if (!bLeftDown)
    {
        if (GizmoManager.IsDragging())
        {
            GizmoManager.EndDrag();
        }
        else if (GizmoManager.HasValidTarget())
        {
            GizmoManager.ResetVisualInteractionState();
        }
    }

    const bool bCursorInViewport =
        IO.MousePos.x >= ViewportScreenRect.X &&
        IO.MousePos.y >= ViewportScreenRect.Y &&
        IO.MousePos.x <= ViewportScreenRect.X + ViewportScreenRect.Width &&
        IO.MousePos.y <= ViewportScreenRect.Y + ViewportScreenRect.Height;

    if (!bCursorInViewport && !GizmoManager.IsDragging())
    {
        if (GizmoManager.HasValidTarget())
        {
            GizmoManager.ResetVisualInteractionState();
        }
        return;
    }

    const float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
    const float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;

    if (VPWidth <= 0.0f || VPHeight <= 0.0f || ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
    {
        return;
    }

    // Drag 중 뷰포트 밖으로 나가도 같은 screen-rect -> render-target 변환식을 유지한다.
    const float LocalMouseX = (IO.MousePos.x - ViewportScreenRect.X) * (VPWidth / ViewportScreenRect.Width);
    const float LocalMouseY = (IO.MousePos.y - ViewportScreenRect.Y) * (VPHeight / ViewportScreenRect.Height);
    const FRay Ray = ViewCamera.DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);

    FGizmoHitProxyContext HitContext{};
    HitContext.ViewProjection = ViewCamera.GetViewProjectionMatrix();
    HitContext.CameraLocation = ViewCamera.GetWorldLocation();
    HitContext.bIsOrtho = ViewCamera.IsOrthogonal();
    HitContext.OrthoWidth = ViewCamera.GetOrthoWidth();
    HitContext.ViewportWidth = VPWidth;
    HitContext.ViewportHeight = VPHeight;
    HitContext.MouseX = LocalMouseX;
    HitContext.MouseY = LocalMouseY;
    HitContext.PickRadius = 2; // 5x5 ID buffer

    FGizmoHitProxyResult HitResult{};
    const bool bGizmoHit = bCanInteractWithGizmo && GizmoManager.HitTestHitProxy(HitContext, HitResult);

    if (bLeftPressed && bCursorInViewport)
    {
        if (bGizmoHit && HitResult.bHit)
        {
            GizmoManager.BeginDragFromHitProxy(HitResult);
        }
        else
        {
            if (GizmoManager.HasValidTarget())
            {
                GizmoManager.ResetVisualInteractionState();
            }
            if (State->bShowBones)
            {
                ApplyBoneViewportSelection(HitTestBoneSelection(Ray));
            }
        }
    }
    else if (bLeftDown && GizmoManager.IsDragging())
    {
        GizmoManager.UpdateDrag(Ray);
    }
    else if (bLeftReleased)
    {
        if (GizmoManager.IsDragging())
        {
            GizmoManager.EndDrag();
        }
        else if (GizmoManager.HasValidTarget())
        {
            GizmoManager.ResetVisualInteractionState();
        }
    }
}

int32 FSkeletalMeshPreviewViewportClient::HitTestBoneSelection(const FRay& Ray) const
{
    if (!State || !State->bShowBones || !PreviewMesh || !PreviewComponent)
    {
        return -1;
    }

    const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
    if (!MeshAsset)
    {
        return -1;
    }

    const TArray<FBoneInfo>& Bones = MeshAsset->Bones;
    const FSkeletonPose& Pose = PreviewComponent->GetCurrentPose();
    const int32 BoneCount = static_cast<int32>(Bones.size());
    if (BoneCount <= 0 || static_cast<int32>(Pose.ComponentTransforms.size()) != BoneCount)
    {
        return -1;
    }

    TArray<float> BoneSphereRadii;
    TArray<float> ConnectionBaseRadii;
    BuildBoneDebugRadii(
        Bones,
        Pose,
        State ? State->BoneDebugScale : 1.0f,
        BoneSphereRadii,
        ConnectionBaseRadii);
    const float ComponentRadiusScale = GetComponentWorldRadiusScale(PreviewComponent);

    int32 BestBoneIndex = -1;
    float BestDistance = (std::numeric_limits<float>::max)();

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const FVector BonePosition = GetBoneWorldPosition(PreviewComponent, Pose.ComponentTransforms[BoneIndex]);
        if (!IsFiniteVector(BonePosition))
        {
            continue;
        }

        const float SpherePickRadius = BoneSphereRadii[BoneIndex] * ComponentRadiusScale * 1.2f;
        float HitDistance = 0.0f;
        if (IntersectRaySphere(Ray, BonePosition, SpherePickRadius, HitDistance) && HitDistance < BestDistance)
        {
            BestDistance = HitDistance;
            BestBoneIndex = BoneIndex;
        }
    }

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        if (ParentIndex < 0 || ParentIndex >= BoneCount)
        {
            continue;
        }

        const FVector ParentPosition = GetBoneWorldPosition(PreviewComponent, Pose.ComponentTransforms[ParentIndex]);
        const FVector BonePosition = GetBoneWorldPosition(PreviewComponent, Pose.ComponentTransforms[BoneIndex]);
        if (!IsFiniteVector(ParentPosition) || !IsFiniteVector(BonePosition))
        {
            continue;
        }

        const float ConnectionPickRadius = (std::max)(ConnectionBaseRadii[BoneIndex] * ComponentRadiusScale * 1.75f,
                                                       BoneSphereRadii[BoneIndex] * ComponentRadiusScale * 0.85f);
        if (ConnectionPickRadius <= 0.0f)
        {
            continue;
        }

        float HitDistance = 0.0f;
        if (IntersectRayBoneOBB(Ray, ParentPosition, BonePosition, ConnectionPickRadius, HitDistance) && HitDistance < BestDistance)
        {
            BestDistance = HitDistance;
            BestBoneIndex = BoneIndex;
        }
    }

    return BestBoneIndex;
}

void FSkeletalMeshPreviewViewportClient::ApplyBoneViewportSelection(int32 BoneIndex)
{
    if (!CanProcessViewportInput())
    {
        return;
    }

    if (!State || !SelectionManager)
    {
        return;
    }

    ImGuiIO& IO = ImGui::GetIO();
    FInputManager& Input = FInputManager::Get();
    const bool bShiftDown = IO.KeyShift || Input.IsShiftDown();
    const bool bCtrlDown = IO.KeyCtrl || Input.IsCtrlDown();

    if (BoneIndex < 0)
    {
        if (!bShiftDown && !bCtrlDown)
        {
            SelectionManager->ClearSelection();
            SyncLegacySelectedBoneIndex(State, SelectionManager);
            SyncGizmoTargetFromSelection();
        }
        return;
    }

    if (bShiftDown)
    {
        const FSkeletalMesh* MeshAsset = PreviewMesh ? PreviewMesh->GetSkeletalMeshAsset() : nullptr;
        const TArray<int32> BoneOrder = BuildBoneSelectionOrder(MeshAsset ? static_cast<int32>(MeshAsset->Bones.size()) : 0);
        SelectionManager->SelectRange(BoneIndex, BoneOrder);
    }
    else if (bCtrlDown)
    {
        SelectionManager->ToggleBone(BoneIndex);
    }
    else
    {
        SelectionManager->SelectBone(BoneIndex);
    }

    SyncLegacySelectedBoneIndex(State, SelectionManager);
    SyncGizmoTargetFromSelection();
}

bool FSkeletalMeshPreviewViewportClient::BuildRenderRequest(FEditorViewportRenderRequest &OutRequest)
{
    if (!CanProcessLiveViewportWork())
    {
        return false;
    }

    EnsurePreviewObjects();

    if (!Viewport || !PreviewComponent || !PreviewMesh || !PreviewProxy)
    {
        return false;
    }

    UE_LOG_CATEGORY(AssetEditor, Info, "[SkeletalPreview] BuildRenderRequest: mesh=%s proxy=%p",
        PreviewMesh->GetFName().ToString().c_str(), PreviewProxy);

    ApplyEditorStateToViewport();
    // NOTE: SyncGizmoTargetFromSelection()은 여기서 호출하지 않는다.
    // BuildRenderRequest()는 Tick()과 독립적으로 렌더 루프에서 매 프레임 호출되기 때문에,
    // 여기서 호출하면 ActivateEditorContext() → bNeedsDeferredTargetSync=true로 설정된
    // 딜레이 가드를 우회하게 된다. 결과적으로 탭 전환 직후에 이전 탭(Level Editor)의
    // 선택 상태가 Asset Editor의 기즈모 타겟으로 즉시 적용되어 기즈모 입력이 블리딩된다.
    // 기즈모 타겟 동기화는 Tick()의 bNeedsDeferredTargetSync 흐름 안에서만 수행한다.
    SubmitSkeletonDebugDraw();

    // Asset Preview의 gizmo visual도 FGizmoManager가 독립 소유한다.
    // Mode 변경/mesh 교체로 render proxy가 재생성된 뒤에도 preview scene에
    // 다시 붙을 수 있도록, Level viewport와 동일하게 매 render request에서
    // idempotent registration을 보장한다.
    GizmoManager.RegisterVisualToScene(&PreviewScene.GetScene());

    // 프리뷰 카메라가 프리뷰 씬 렌더 타깃과 동일한 렌더 범위를 사용하도록 맞춘다.
    // Asset Preview 뷰포트 크기의 기준값은 ImGui 패널 사각형이다.
    if (ViewportScreenRect.Width > 0.0f && ViewportScreenRect.Height > 0.0f)
    {
        ViewCamera.OnResize(static_cast<int32>(ViewportScreenRect.Width),
                            static_cast<int32>(ViewportScreenRect.Height));
    }

    OutRequest.Viewport = Viewport;
    OutRequest.ViewInfo = ViewCamera.GetCameraState();
    OutRequest.Scene = &PreviewScene.GetScene();
    OutRequest.RenderOptions = RenderOptions;
    OutRequest.CursorProvider = this;
    OutRequest.bRenderGrid = RenderOptions.ShowFlags.bGrid;
    OutRequest.bEnableGPUOcclusion = false;
    OutRequest.bAllowLevelDebugVisuals = false;
    GizmoManager.ApplyScreenSpaceScaling(
        ViewCamera.GetWorldLocation(),
        ViewCamera.IsOrthogonal(),
        ViewCamera.GetOrthoWidth(),
        ViewportScreenRect.Height > 0.0f
            ? ViewportScreenRect.Height
            : (Viewport ? static_cast<float>(Viewport->GetHeight()) : 0.0f));
    GizmoManager.SetAxisMask(UGizmoVisualComponent::ComputeAxisMask(
        RenderOptions.ViewportType,
        GizmoManager.GetMode()));
    return true;
}

void FSkeletalMeshPreviewViewportClient::NotifyViewportPresented()
{
    if (CanProcessLiveViewportWork())
    {
        bHasRenderedViewportFrameSinceActivation = true;
    }
}

void FSkeletalMeshPreviewViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
    if (!CanProcessLiveViewportWork())
    {
        return;
    }

    // Preview Scene RenderTarget이 연결되면 공통 FEditorViewportClient 이미지 출력 경로를 그대로 사용한다.
    if (Viewport && Viewport->GetSRV())
    {
        FEditorViewportClient::RenderViewportImage(bIsActiveViewport);
        RenderFallbackOverlay();
        return;
    }

    // RenderPipeline 연결 전 또는 렌더 실패 시에도 패널이 비어 보이지 않도록 fallback을 유지한다.
    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const FRect &R = ViewportScreenRect;
    if (R.Width <= 0.0f || R.Height <= 0.0f)
    {
        return;
    }

    const ImVec2 Min(R.X, R.Y);
    const ImVec2 Max(R.X + R.Width, R.Y + R.Height);
    DrawList->AddRectFilled(Min, Max, IM_COL32(12, 12, 12, 255));
    DrawList->AddRect(Min, Max, IM_COL32(72, 72, 72, 255));

    const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
    const float AxisLength = 64.0f;
    DrawList->AddLine(Center, ImVec2(Center.x + AxisLength, Center.y), IM_COL32(220, 80, 80, 255), 2.0f);
    DrawList->AddLine(Center, ImVec2(Center.x, Center.y - AxisLength), IM_COL32(80, 220, 80, 255), 2.0f);
    DrawList->AddLine(Center, ImVec2(Center.x - AxisLength * 0.55f, Center.y + AxisLength * 0.55f), IM_COL32(80, 120, 240, 255), 2.0f);

    RenderFallbackOverlay();
}

void FSkeletalMeshPreviewViewportClient::RenderFallbackOverlay()
{
    const FRect& R = ViewportScreenRect;
    if (R.Width <= 0.0f || R.Height <= 0.0f)
    {
        return;
    }

    ImGui::SetCursorScreenPos(ImVec2(R.X + 16.0f, R.Y + 16.0f));
    ImGui::BeginGroup();

    if (!PreviewMesh)
    {
        ImGui::TextDisabled("No SkeletalMesh loaded.");
    }
    else if (!State || State->bShowMeshStatsOverlay)
    {
        ImGui::Text("Bones: %d", PreviewMesh->GetBoneCount());
        ImGui::Text("Vertices: %d", PreviewMesh->GetVertexCount());
        ImGui::Text("Indices: %d", PreviewMesh->GetIndexCount());
        if (State)
        {
            ImGui::Text("Selected Bone: %d", State->SelectedBoneIndex);
            ImGui::Text("LOD: %d", State->CurrentLODIndex);
            ImGui::Text("Show Bones: %s", State->bShowBones ? "true" : "false");
            ImGui::Text("Pose Edit Mode: %s", State->bEnablePoseEditMode ? "true" : "false");
        }
    }

    ImGui::TextDisabled("RMB Drag: Orbit / MMB Drag: Pan / Wheel: Zoom / F: Frame");
    ImGui::EndGroup();
}

void FSkeletalMeshPreviewViewportClient::SubmitSkeletonDebugDraw()
{
    if (!CanProcessLiveViewportWork())
    {
        return;
    }

    if (!State || !State->bShowBones || !PreviewMesh || !PreviewComponent)
    {
        return;
    }

    const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
    if (!MeshAsset)
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = MeshAsset->Bones;
    const FSkeletonPose& Pose = PreviewComponent->GetCurrentPose();
    const int32 BoneCount = static_cast<int32>(Bones.size());
    if (BoneCount <= 0 || static_cast<int32>(Pose.ComponentTransforms.size()) != BoneCount)
    {
        return;
    }

    FScene& Scene = PreviewScene.GetScene();
    const int32 SelectedBoneIndex = ResolveSelectedBoneIndex(State, SelectionManager);
    TArray<float> BoneSphereRadii;
    TArray<float> ConnectionBaseRadii;
    BuildBoneDebugRadii(
        Bones,
        Pose,
        State ? State->BoneDebugScale : 1.0f,
        BoneSphereRadii,
        ConnectionBaseRadii);
    const float ComponentRadiusScale = GetComponentWorldRadiusScale(PreviewComponent);
    constexpr int32 SphereSegments = 12;
    const FColor NormalSphereColor(0, 255, 0, 255);
    const FColor HighlightSphereColor(255, 220, 40, 255);
    const FColor NormalConnectionColor(80, 220, 255, 220);
    const FColor HighlightConnectionColor(255, 220, 40, 255);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const FVector BonePosition = GetBoneWorldPosition(PreviewComponent, Pose.ComponentTransforms[BoneIndex]);
        if (!IsFiniteVector(BonePosition))
        {
            continue;
        }

        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        const bool bSphereHighlighted =
            BoneIndex == SelectedBoneIndex ||
            ParentIndex == SelectedBoneIndex;

        DrawDebugSphere(
            &Scene,
            BonePosition,
            BoneSphereRadii[BoneIndex] * ComponentRadiusScale,
            SphereSegments,
            bSphereHighlighted ? HighlightSphereColor : NormalSphereColor,
            0.0f,
            false);

        if (ParentIndex < 0 || ParentIndex >= BoneCount)
        {
            continue;
        }

        const FVector ParentPosition = GetBoneWorldPosition(PreviewComponent, Pose.ComponentTransforms[ParentIndex]);
        if (!IsFiniteVector(ParentPosition))
        {
            continue;
        }

        const float ConnectionBaseRadius = ConnectionBaseRadii[BoneIndex] * ComponentRadiusScale;
        if (ConnectionBaseRadius <= 0.0f)
        {
            continue;
        }

        const bool bConnectionHighlighted =
            BoneIndex == SelectedBoneIndex ||
            ParentIndex == SelectedBoneIndex;

        DrawDebugBoneConnection(
            &Scene,
            ParentPosition,
            BonePosition,
            ConnectionBaseRadius,
            bConnectionHighlighted ? HighlightConnectionColor : NormalConnectionColor,
            0.0f,
            false);
    }
}


void FSkeletalMeshPreviewViewportClient::SyncGizmoTargetFromSelection()
{
    if (!CanProcessLiveViewportWork())
    {
        GizmoManager.AbortLiveInteractionWithoutApplying();
        GizmoTargetBoneIndex = -1;
        return;
    }

    if (!State || !PreviewComponent || !State->bShowGizmo || !SelectionManager)
    {
        GizmoManager.ClearTarget();
        GizmoTargetBoneIndex = -1;
        return;
    }

    // Do not fall back to State->SelectedBoneIndex here. That field is a legacy UI mirror
    // and can survive a tab/context switch. The live gizmo target must come only from
    // this active editor tab's SelectionManager.
    const int32 SelectedBoneIndex = SelectionManager->GetPrimaryBoneIndex();
    if (SelectedBoneIndex < 0)
    {
        GizmoManager.ClearTarget();
        GizmoTargetBoneIndex = -1;
        return;
    }

    ApplySkeletalMeshGizmoStateToManager(State, GizmoManager);

    GizmoTargetBoneIndex = SelectedBoneIndex;

    std::shared_ptr<FBoneTransformGizmoTarget> BoneTarget =
        std::make_shared<FBoneTransformGizmoTarget>(
            PreviewComponent,
            PoseController,
            SelectedBoneIndex,
            this,
            GetEditorContextActiveFlag(),
            GetEditorContextEpoch());
    std::shared_ptr<IGizmoDeltaTarget> BoneDeltaTarget = PoseController
        ? std::static_pointer_cast<IGizmoDeltaTarget>(BoneTarget)
        : nullptr;

    GizmoManager.SetTargetIfChanged(
        FGizmoTargetKey{PreviewComponent, SelectedBoneIndex},
        BoneTarget,
        BoneDeltaTarget);
}
