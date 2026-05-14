#pragma once

#include "Core/CoreTypes.h"
#include "Component/Gizmo/GizmoTypes.h"
#include "Math/Vector.h"

// Skeletal Mesh Editor에서 사용하는 프리뷰 표시 모드.
// 실제 Reference Pose / Skinned Pose 렌더링은 Runtime 담당자가 USkeletalMeshComponent와 연동하면 여기서 선택값만 넘겨주면 된다.
enum class ESkeletalMeshPreviewMode : uint8
{
    ReferencePose = 0,
    SkinnedPose = 1,
};

// Skeletal Mesh Preview Viewport의 카메라 방향 선택값.
// Level Editor toolbar와 같은 상단 toolbar를 쓰기 위해 UI 상태로 보관한다.
enum class ESkeletalMeshPreviewViewportType : uint8
{
    Perspective = 0,
    Top,
    Bottom,
    Left,
    Right,
    Front,
    Back,
    FreeOrtho,
};

// Skeletal Mesh Preview Viewport의 렌더 표시 모드.
enum class ESkeletalMeshPreviewViewMode : uint8
{
    Lit = 0,
    Unlit,
    LitGouraud,
    LitLambert,
    Wireframe,
    SceneDepth,
    WorldNormal,
    LightCulling,
};

enum class ESkeletalMeshPreviewLayout : uint8
{
    OnePane = 0,
    TwoPanesHorizontal,
    TwoPanesVertical,
    FourPanes2x2,
};

// Details 패널에서 선택한 Bone transform 기준.
// Unreal Skeletal Mesh Editor의 Bone / Reference / Mesh Relative 토글과 같은 용도다.
enum class ESkeletalMeshBoneDetailsSpace : uint8
{
    Bone = 0,          // 현재 포즈의 부모 기준 Local transform
    Reference = 1,     // 애셋 Reference/Bind pose의 부모 기준 Local transform
    MeshRelative = 2,  // 현재 포즈의 Mesh/Component 기준 transform
};

// Skeletal Mesh Viewer / Asset Editor의 현재 편집 상태.
struct FSkeletalMeshEditorState
{
    int32 SelectedBoneIndex = -1;
    int32 CurrentLODIndex = 0;
    int32 SelectedSectionIndex = -1;
    int32 SelectedMaterialSlotIndex = -1;

    bool bShowBones = true;
    bool bShowGrid = true;
    bool bShowReferencePose = true;
    bool bEnablePoseEditMode = false;

    // Level Editor viewport toolbar의 Show Flag / Snap / Camera 항목과 동일한 UI 상태.
    bool bShowPrimitives = true;
    bool bShowSkeletalMesh = true;
    bool bShowBillboardText = true;
    bool bShowWorldAxis = true;
    bool bShowGizmo = true;
    bool bShowSceneBVH = false;
    bool bShowOctree = false;
    bool bShowWorldBound = false;
    bool bShowLightVisualization = false;

    bool bEnableTranslationSnap = false;
    bool bEnableRotationSnap = false;
    bool bEnableScaleSnap = false;
    float TranslationSnapSize = 10.0f;
    float RotationSnapSize = 15.0f;
    float ScaleSnapSize = 0.25f;

    float CameraSpeed = 5.0f;
    float CameraFOV = 60.0f;
    float CameraOrthoWidth = 10.0f;

    float GridSpacing = 1.0f;
    int32 GridHalfLineCount = 100;
    float GridLineThickness = 1.0f;
    float GridMajorLineThickness = 1.5f;
    int32 GridMajorLineInterval = 10;
    float GridMinorIntensity = 0.35f;
    float GridMajorIntensity = 0.70f;
    float AxisThickness = 2.0f;
    float AxisIntensity = 1.0f;
    float DebugLineThickness = 1.0f;
    float BillboardIconScale = 1.0f;
    float BoneDebugScale = 1.0f;

    // Previewer Settings 패널: 현재 렌더러가 직접 수치 조명을 모두 소비하지 않더라도,
    // 프리뷰 씬의 조명/시각화 정책을 한 곳에서 조정하기 위한 상태다.
    bool bPreviewLighting = true;
    float PreviewDirectionalLightIntensity = 1.0f;
    float PreviewAmbientLightIntensity = 0.25f;
    FVector4 PreviewDirectionalLightColor = FVector4(1.0f, 0.96f, 0.88f, 1.0f);
    FVector4 PreviewAmbientLightColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    float PreviewLightYaw = 45.0f;
    float PreviewLightPitch = -35.0f;

    EGizmoMode GizmoMode = EGizmoMode::Translate;
    EGizmoSpace GizmoSpace = EGizmoSpace::Local;
    bool bShowMeshStatsOverlay = true;
    bool bFramePreviewRequested = false;

    ESkeletalMeshPreviewMode PreviewMode = ESkeletalMeshPreviewMode::ReferencePose;
    ESkeletalMeshPreviewViewportType PreviewViewportType = ESkeletalMeshPreviewViewportType::Perspective;
    ESkeletalMeshPreviewViewMode PreviewViewMode = ESkeletalMeshPreviewViewMode::Lit;
    ESkeletalMeshPreviewLayout PreviewLayout = ESkeletalMeshPreviewLayout::OnePane;
    ESkeletalMeshBoneDetailsSpace BoneDetailsSpace = ESkeletalMeshBoneDetailsSpace::Bone;
};
