#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"

#include "Common/UI/Viewport/EditorViewportToolbar.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "Render/Pipeline/Renderer.h"
#include "ImGui/imgui.h"

#include <cstdio>

namespace
{
    FEditorViewportToolbar::EToolMode ToCommonToolMode(EGizmoMode Mode)
    {
        switch (Mode)
        {
        case EGizmoMode::Rotate: return FEditorViewportToolbar::EToolMode::Rotate;
        case EGizmoMode::Scale: return FEditorViewportToolbar::EToolMode::Scale;
        case EGizmoMode::Translate:
        default: return FEditorViewportToolbar::EToolMode::Translate;
        }
    }

    EGizmoMode ToGizmoMode(FEditorViewportToolbar::EToolMode Mode)
    {
        switch (Mode)
        {
        case FEditorViewportToolbar::EToolMode::Rotate: return EGizmoMode::Rotate;
        case FEditorViewportToolbar::EToolMode::Scale: return EGizmoMode::Scale;
        case FEditorViewportToolbar::EToolMode::Translate:
        default: return EGizmoMode::Translate;
        }
    }

    const char *GetViewportTypeLabel(ESkeletalMeshPreviewViewportType Type)
    {
        return FEditorViewportToolbar::GetViewportTypeLabelByIndex(static_cast<int32>(Type));
    }

    FEditorViewportToolbar::EIcon GetViewportTypeIcon(ESkeletalMeshPreviewViewportType Type)
    {
        return FEditorViewportToolbar::GetViewportTypeIconByIndex(static_cast<int32>(Type));
    }

    const char *GetViewModeLabel(ESkeletalMeshPreviewViewMode Mode)
    {
        return FEditorViewportToolbar::GetViewModeLabelByIndex(static_cast<int32>(Mode));
    }

    FEditorViewportToolbar::EIcon GetViewModeIcon(ESkeletalMeshPreviewViewMode Mode)
    {
        return FEditorViewportToolbar::GetViewModeIconByIndex(static_cast<int32>(Mode));
    }

    void DrawViewportTypeOption(FSkeletalMeshEditorState &State, const char *Label, ESkeletalMeshPreviewViewportType Type)
    {
        if (FEditorViewportToolbar::DrawIconSelectable(Label, GetViewportTypeIcon(Type), Label, State.PreviewViewportType == Type, 220.0f))
        {
            State.PreviewViewportType = Type;
            ImGui::CloseCurrentPopup();
        }
    }

    void DrawViewModeOption(FSkeletalMeshEditorState &State, const char *Label, ESkeletalMeshPreviewViewMode Mode)
    {
        if (FEditorViewportToolbar::DrawIconSelectable(Label, GetViewModeIcon(Mode), Label, State.PreviewViewMode == Mode, 260.0f))
        {
            State.PreviewViewMode = Mode;
            ImGui::CloseCurrentPopup();
        }
    }
}

void FSkeletalMeshEditorToolbar::RenderViewportToolbar(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, FRenderer *Renderer)
{
    FEditorViewportToolbar::FDesc Desc;
    Desc.IdPrefix = "SkeletalMeshViewportToolbar";
    Desc.ToolbarWidth = ImGui::GetWindowWidth();
    Desc.Renderer = Renderer;
    Desc.bEnabled = Mesh != nullptr;
    Desc.ToolMode = ToCommonToolMode(State.GizmoMode);
    Desc.CoordSpace = State.GizmoSpace == EGizmoSpace::World ? FEditorViewportToolbar::ECoordSpace::World
                                                             : FEditorViewportToolbar::ECoordSpace::Local;
    Desc.bSnapEnabled = State.bEnableTranslationSnap || State.bEnableRotationSnap || State.bEnableScaleSnap;
    Desc.ViewportTypeIcon = GetViewportTypeIcon(State.PreviewViewportType);
    Desc.ViewportTypeLabel = GetViewportTypeLabel(State.PreviewViewportType);
    Desc.ViewModeIcon = GetViewModeIcon(State.PreviewViewMode);
    Desc.ViewModeLabel = GetViewModeLabel(State.PreviewViewMode);

    Desc.OnToolModeChanged = [&](FEditorViewportToolbar::EToolMode Mode)
    {
        State.bEnablePoseEditMode = true;
        State.GizmoMode = ToGizmoMode(Mode);
    };
    Desc.OnCoordSpaceToggled = [&]()
    {
        State.GizmoSpace = State.GizmoSpace == EGizmoSpace::World ? EGizmoSpace::Local : EGizmoSpace::World;
    };

    Desc.DrawSnapPopup = [&]()
    {
        FEditorViewportToolbar::DrawCommonSnapPopup(State.bEnableTranslationSnap, State.TranslationSnapSize,
                                                   State.bEnableRotationSnap, State.RotationSnapSize,
                                                   State.bEnableScaleSnap, State.ScaleSnapSize);
    };

    Desc.DrawCameraPopup = [&]()
    {
        FEditorViewportToolbar::DrawPopupSectionHeader("CAMERA");
        ImGui::TextDisabled("Preview camera settings");
        ImGui::Spacing();
        FEditorViewportToolbar::DrawScalarFloat("Speed", State.CameraSpeed, 0.1f, 0.1f, 1000.0f, "%.1f");
        FEditorViewportToolbar::DrawScalarFloat("FOV", State.CameraFOV, 0.5f, 1.0f, 170.0f, "%.1f");
        FEditorViewportToolbar::DrawScalarFloat("Ortho Width", State.CameraOrthoWidth, 0.1f, 0.1f, 100000.0f, "%.1f");
        FEditorViewportToolbar::DrawPopupSeparator(4.0f, 6.0f);
        if (ImGui::MenuItem("Frame Selected"))
        {
            State.bFramePreviewRequested = true;
        }
        if (ImGui::MenuItem("Perspective", nullptr, State.PreviewViewportType == ESkeletalMeshPreviewViewportType::Perspective))
        {
            State.PreviewViewportType = ESkeletalMeshPreviewViewportType::Perspective;
        }
    };

    Desc.DrawViewportTypePopup = [&]()
    {
        FEditorViewportToolbar::DrawPopupSectionHeader("PERSPECTIVE");
        DrawViewportTypeOption(State, "Perspective", ESkeletalMeshPreviewViewportType::Perspective);
        FEditorViewportToolbar::DrawPopupSectionHeader("ORTHOGRAPHIC");
        DrawViewportTypeOption(State, "Top", ESkeletalMeshPreviewViewportType::Top);
        DrawViewportTypeOption(State, "Bottom", ESkeletalMeshPreviewViewportType::Bottom);
        DrawViewportTypeOption(State, "Left", ESkeletalMeshPreviewViewportType::Left);
        DrawViewportTypeOption(State, "Right", ESkeletalMeshPreviewViewportType::Right);
        DrawViewportTypeOption(State, "Front", ESkeletalMeshPreviewViewportType::Front);
        DrawViewportTypeOption(State, "Back", ESkeletalMeshPreviewViewportType::Back);
        DrawViewportTypeOption(State, "Free Ortho", ESkeletalMeshPreviewViewportType::FreeOrtho);
    };

    Desc.DrawViewModePopup = [&]()
    {
        FEditorViewportToolbar::DrawPopupSectionHeader("VIEW MODE");
        DrawViewModeOption(State, "Lit", ESkeletalMeshPreviewViewMode::Lit);
        DrawViewModeOption(State, "Unlit", ESkeletalMeshPreviewViewMode::Unlit);
        DrawViewModeOption(State, "Wireframe", ESkeletalMeshPreviewViewMode::Wireframe);
        DrawViewModeOption(State, "Lit Gouraud", ESkeletalMeshPreviewViewMode::LitGouraud);
        DrawViewModeOption(State, "Lit Lambert", ESkeletalMeshPreviewViewMode::LitLambert);
        DrawViewModeOption(State, "Scene Depth", ESkeletalMeshPreviewViewMode::SceneDepth);
        DrawViewModeOption(State, "World Normal", ESkeletalMeshPreviewViewMode::WorldNormal);
        DrawViewModeOption(State, "Light Culling", ESkeletalMeshPreviewViewMode::LightCulling);
    };

    Desc.DrawShowPopup = [&]()
    {
        FEditorViewportToolbar::DrawPopupSectionHeader("SHOW FLAGS");

        if (ImGui::CollapsingHeader("Common", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Primitives", &State.bShowPrimitives);
            ImGui::Checkbox("SkeletalMesh", &State.bShowSkeletalMesh);
            ImGui::Checkbox("Billboard Text", &State.bShowBillboardText);
        }

        if (ImGui::CollapsingHeader("Grid / Axis", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Grid", &State.bShowGrid);
            if (State.bShowGrid)
            {
                FEditorViewportToolbar::DrawSliderFloat("Spacing", State.GridSpacing, 0.1f, 10.0f, "%.1f");
                FEditorViewportToolbar::DrawSliderInt("Half Line Count", State.GridHalfLineCount, 10, 500);
                FEditorViewportToolbar::DrawSliderFloat("Grid Line", State.GridLineThickness, 0.0f, 4.0f);
                FEditorViewportToolbar::DrawSliderFloat("Major Line", State.GridMajorLineThickness, 0.0f, 6.0f);
                FEditorViewportToolbar::DrawSliderInt("Major Interval", State.GridMajorLineInterval, 1, 50);
                FEditorViewportToolbar::DrawSliderFloat("Minor Intensity", State.GridMinorIntensity, 0.0f, 2.0f);
                FEditorViewportToolbar::DrawSliderFloat("Major Intensity", State.GridMajorIntensity, 0.0f, 2.0f);
            }

            ImGui::Checkbox("World Axis", &State.bShowWorldAxis);
            if (State.bShowWorldAxis)
            {
                FEditorViewportToolbar::DrawSliderFloat("Axis Thickness", State.AxisThickness, 0.0f, 8.0f);
                FEditorViewportToolbar::DrawSliderFloat("Axis Intensity", State.AxisIntensity, 0.0f, 2.0f);
            }
        }

        if (ImGui::CollapsingHeader("Skeletal Preview", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Gizmo", &State.bShowGizmo);
            ImGui::Checkbox("Bones", &State.bShowBones);
            ImGui::Checkbox("Reference Pose", &State.bShowReferencePose);
            ImGui::Checkbox("Mesh Stats", &State.bShowMeshStatsOverlay);
            FEditorViewportToolbar::DrawSliderFloat("Billboard Icon Scale", State.BillboardIconScale, 0.1f, 5.0f);
            FEditorViewportToolbar::DrawSliderFloat("Bone Debug Scale", State.BoneDebugScale, 0.1f, 5.0f);
        }

        if (ImGui::CollapsingHeader("Debug"))
        {
            FEditorViewportToolbar::DrawSliderFloat("Line Thickness", State.DebugLineThickness, 1.0f, 12.0f, "%.1f");
            ImGui::Checkbox("Scene BVH (Green)", &State.bShowSceneBVH);
            ImGui::Checkbox("Scene Octree (Cyan)", &State.bShowOctree);
            ImGui::Checkbox("World Bound (Magenta)", &State.bShowWorldBound);
            ImGui::Checkbox("Light Visualization", &State.bShowLightVisualization);
        }
    };

    Desc.DrawLayoutPopup = [&]()
    {
        FEditorViewportToolbar::DrawPopupSectionHeader("VIEWPORT LAYOUT");
        auto DrawLayoutItem = [&](const char *Label, ESkeletalMeshPreviewLayout Layout)
        {
            if (ImGui::MenuItem(Label, nullptr, State.PreviewLayout == Layout))
            {
                State.PreviewLayout = Layout;
                ImGui::CloseCurrentPopup();
            }
        };
        DrawLayoutItem("One Pane", ESkeletalMeshPreviewLayout::OnePane);
        DrawLayoutItem("Two Panes - Horizontal", ESkeletalMeshPreviewLayout::TwoPanesHorizontal);
        DrawLayoutItem("Two Panes - Vertical", ESkeletalMeshPreviewLayout::TwoPanesVertical);
        DrawLayoutItem("Four Panes 2 x 2", ESkeletalMeshPreviewLayout::FourPanes2x2);
        FEditorViewportToolbar::DrawPopupSeparator(4.0f, 6.0f);
        ImGui::TextDisabled("Skeletal Mesh Editor viewport still renders a single pane until split layout is wired.");
    };

    FEditorViewportToolbar::RenderViewportToolbar(Desc);
}
