#include "PCH/LunaticPCH.h"
#include "LevelEditor/Viewport/LevelViewportLayout.h"

#include "Common/UI/Style/AccentColor.h"
#include "Common/UI/Style/EditorUIStyle.h"
#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Common/UI/Panels/Panel.h"
#include "Common/UI/Viewport/ViewportToolbar.h"
#include "Common/UI/Viewport/EditorViewportToolbar.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoVisualComponent.h"
#include "Core/ProjectSettings.h"
#include "EditorEngine.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Game/GameActors/Obstacle/SimpleObstacleActor.h"
#include "Game/Map/AMapManager.h"
#include "Game/Player/Runner.h"
#include "GameFramework/CharacterActor.h"
#include "GameFramework/DecalActor.h"
#include "GameFramework/HeightFogActor.h"
#include "GameFramework/Light/AmbientLightActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/Light/PointLightActor.h"
#include "GameFramework/Light/SpotLightActor.h"
#include "GameFramework/PawnActor.h"
#include "GameFramework/ScreenTextActor.h"
#include "GameFramework/SkeletalMeshActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "GameFramework/UIRootActor.h"
#include "GameFramework/World.h"
#include "GameFramework/WorldTextActor.h"
#include "ImGui/imgui.h"
#include "LevelEditor/Selection/SelectionManager.h"
#include "LevelEditor/Viewport/LevelEditorViewportClient.h"
#include "Common/Viewport/EditorViewportCamera.h"
#include "Math/MathUtils.h"
#include "Platform/Paths.h"
#include "Render/Pipeline/Renderer.h"
#include "Resource/ResourceManager.h"
#include "LevelEditor/Settings/LevelEditorSettings.h"
#include "UI/SSplitter.h"
#include "Viewport/Viewport.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>

namespace
{
    namespace PopupPalette
    {
        constexpr ImVec4 PopupBg = ImVec4(0.12f, 0.13f, 0.15f, 0.98f);
        constexpr ImVec4 SurfaceBg = ImVec4(0.22f, 0.23f, 0.26f, 1.0f);
        constexpr ImVec4 FieldBg = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
        constexpr ImVec4 FieldHoverBg = ImVec4(0.09f, 0.09f, 0.11f, 1.0f);
        constexpr ImVec4 FieldActiveBg = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
        constexpr ImVec4 FieldBorder = ImVec4(0.34f, 0.35f, 0.38f, 1.0f);
        constexpr ImVec4 CheckboxBg = ImVec4(0.03f, 0.03f, 0.04f, 1.0f);
        constexpr ImVec4 CheckboxHoverBg = ImVec4(0.07f, 0.07f, 0.09f, 1.0f);
        constexpr ImVec4 CheckboxActiveBg = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
        constexpr ImVec4 CheckboxCheck = UIAccentColor::Value;
    } // namespace PopupPalette

    constexpr ImVec4 PopupSectionHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
    constexpr ImVec4 CameraPopupLabelColor = ImVec4(0.87f, 0.88f, 0.90f, 1.0f);
    constexpr ImVec4 CameraPopupHintColor = ImVec4(0.60f, 0.63f, 0.68f, 1.0f);
    constexpr ImVec4 PopupMenuItemColor = ImVec4(0.18f, 0.18f, 0.20f, 0.96f);
    constexpr ImVec4 PopupMenuItemHoverColor = UIAccentColor::Value;
    constexpr ImVec4 PopupMenuItemActiveColor = UIAccentColor::Value;
    constexpr ImVec2 PopupComfortWindowPadding = ImVec2(10.0f, 10.0f);
    constexpr ImVec2 PopupComfortFramePadding = ImVec2(7.0f, 5.0f);
    constexpr ImVec2 PopupComfortItemSpacing = ImVec2(6.0f, 6.0f);
    constexpr ImVec2 PopupComfortItemInnerSpacing = ImVec2(5.0f, 4.0f);
    constexpr float  CameraPopupLabelWidth = 68.0f;
    constexpr float  CameraPopupFieldOffset = 78.0f;
    constexpr float  CameraPopupMinFieldWidth = 88.0f;
    constexpr float  CameraPopupMaxScalarFieldWidth = 112.0f;
    constexpr float  CameraPopupMaxAxisFieldWidth = 52.0f;
    constexpr ImVec4 SnapPopupBorderColor = ImVec4(0.30f, 0.31f, 0.35f, 1.0f);
    constexpr ImVec4 SnapPopupSelectedColor = UIAccentColor::WithAlpha(0.98f);
    constexpr ImVec4 SnapPopupSelectedHoverColor = UIAccentColor::Value;
    constexpr ImVec4 SnapPopupSelectedActiveColor = UIAccentColor::Value;
    constexpr ImVec4 SnapPopupSelectedTextColor = ImVec4(0.97f, 0.98f, 1.0f, 1.0f);
    constexpr ImVec4 SnapPopupSelectedCheckColor = ImVec4(0.97f, 0.98f, 1.0f, 1.0f);
    constexpr ImVec4 SnapPopupTabSelectedColor = UIAccentColor::Value;
    constexpr ImVec4 SnapPopupTabSelectedHoverColor = UIAccentColor::Value;
    constexpr ImVec4 SnapPopupTabSelectedActiveColor = UIAccentColor::Value;
    constexpr ImVec4 SnapPopupTabSelectedTextColor = ImVec4(0.97f, 0.98f, 1.0f, 1.0f);

    void DrawPopupSectionHeader(const char *Label);

    enum class ESnapPopupType : uint8
    {
        None,
        Location,
        Rotation,
        Scale
    };

    ESnapPopupType GSnapPopupTab[FLevelViewportLayout::MaxViewportSlots] = {};

    void ApplyProjectViewportSettings(FViewportRenderOptions &Opts)
    {
        const FProjectSettings &ProjectSettings = FProjectSettings::Get();
        Opts.LightCullingMode = static_cast<ELightCullingMode>(ProjectSettings.LightCulling.Mode);
        Opts.HeatMapMax = ProjectSettings.LightCulling.HeatMapMax;
        Opts.Enable25DCulling = ProjectSettings.LightCulling.bEnable25DCulling;
        Opts.SceneDepthVisMode = static_cast<int32>(ProjectSettings.SceneDepth.Mode);
        Opts.Exponent = ProjectSettings.SceneDepth.Exponent;
    }

    void PushCameraPopupFieldStyle()
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, PopupPalette::FieldBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, PopupPalette::FieldHoverBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, PopupPalette::FieldActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Border, PopupPalette::FieldBorder);
    }

    void PushCommonPopupBgColor() { ImGui::PushStyleColor(ImGuiCol_PopupBg, PopupPalette::PopupBg); }

    void PushCommonPopupMenuItemStyle()
    {
        ImGui::PushStyleColor(ImGuiCol_Header, PopupMenuItemColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, PopupMenuItemHoverColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, PopupMenuItemActiveColor);
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, PopupMenuItemHoverColor);
    }

    bool DrawCameraPopupScalarRow(const char *Id, const char *Label, float &Value, float Speed, float Min, float Max, const char *Format)
    {
        ImGui::PushID(Id);

        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, CameraPopupLabelColor);
        ImGui::TextUnformatted(Label);
        ImGui::PopStyleColor();

        ImGui::SameLine(CameraPopupFieldOffset);
        const float InputWidth =
            (std::min)(CameraPopupMaxScalarFieldWidth, (std::max)(CameraPopupMinFieldWidth, ImGui::GetContentRegionAvail().x));
        ImGui::SetNextItemWidth(InputWidth);

        PushCameraPopupFieldStyle();
        const bool bChanged = ImGui::DragFloat("##Value", &Value, Speed, Min, Max, Format);
        ImGui::PopStyleColor(4);

        ImGui::PopID();
        return bChanged;
    }

    bool DrawCameraPopupVectorRow(const char *Id, const char *Label, float Values[3], float Speed)
    {
        static const char *AxisLabels[3] = {"X", "Y", "Z"};

        ImGui::PushID(Id);

        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, CameraPopupLabelColor);
        ImGui::TextUnformatted(Label);
        ImGui::PopStyleColor();

        const float TotalSpacing = ImGui::GetStyle().ItemSpacing.x * 2.0f;
        ImGui::SameLine(CameraPopupFieldOffset);
        const float AvailableWidth = (std::max)(CameraPopupMinFieldWidth * 3.0f, ImGui::GetContentRegionAvail().x);
        const float AxisInputWidth = (std::min)(CameraPopupMaxAxisFieldWidth, (AvailableWidth - TotalSpacing) / 3.0f);
        bool        bChanged = false;

        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            ImGui::PushID(Axis);
            if (Axis > 0)
            {
                ImGui::SameLine();
            }

            ImGui::SetNextItemWidth(AxisInputWidth);
            PushCameraPopupFieldStyle();
            bChanged |= ImGui::DragFloat(AxisLabels[Axis], &Values[Axis], Speed, 0.0f, 0.0f, "%.3f");
            ImGui::PopStyleColor(4);
            ImGui::PopID();
        }

        ImGui::PopID();
        return bChanged;
    }

    void DrawCameraPopupContent(FEditorViewportCamera *Camera, FLevelEditorSettings &Settings)
    {
        DrawPopupSectionHeader("CAMERA");
        ImGui::PushStyleColor(ImGuiCol_Text, CameraPopupHintColor);
        ImGui::TextUnformatted("Tune movement and lens values, then place the camera precisely.");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, PopupPalette::SurfaceBg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PopupComfortWindowPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, PopupComfortFramePadding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, PopupComfortItemSpacing);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, PopupComfortItemInnerSpacing);
        ImGui::BeginGroup();
        {
            float CameraSpeed = Settings.CameraSpeed;
            if (DrawCameraPopupScalarRow("Speed", "Speed", CameraSpeed, 0.1f, 0.1f, 1000.0f, "%.1f"))
            {
                Settings.CameraSpeed = Clamp(CameraSpeed, 0.1f, 1000.0f);
            }

            if (Camera)
            {
                ImGui::Spacing();
                float CameraFOV_Deg = Camera->GetFOV() * RAD_TO_DEG;
                if (DrawCameraPopupScalarRow("FOV", "FOV", CameraFOV_Deg, 0.5f, 1.0f, 170.0f, "%.1f"))
                {
                    Camera->SetFOV(Clamp(CameraFOV_Deg, 1.0f, 170.0f) * DEG_TO_RAD);
                }

                float OrthoWidth = Camera->GetOrthoWidth();
                if (DrawCameraPopupScalarRow("OrthoWidth", "Ortho Width", OrthoWidth, 0.1f, 0.1f, 100000.0f, "%.1f"))
                {
                    Camera->SetOrthoWidth(Clamp(OrthoWidth, 0.1f, 100000.0f));
                }

                ImGui::Spacing();
                FVector CamPos = Camera->GetWorldLocation();
                float   CameraLocation[3] = {CamPos.X, CamPos.Y, CamPos.Z};
                if (DrawCameraPopupVectorRow("Location", "Location", CameraLocation, 0.1f))
                {
                    Camera->SetWorldLocation(FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]));
                }

                ImGui::Spacing();
                FRotator CamRot = Camera->GetRelativeRotation();
                float    CameraRotation[3] = {CamRot.Roll, CamRot.Pitch, CamRot.Yaw};
                if (DrawCameraPopupVectorRow("Rotation", "Rotation", CameraRotation, 0.1f))
                {
                    Camera->SetRelativeRotation(FRotator(CameraRotation[1], CameraRotation[2], CameraRotation[0]));
                }
            }
        }
        ImGui::EndGroup();
        ImGui::PopStyleVar(6);
        ImGui::PopStyleColor();
    }

    void DrawPopupSectionHeader(const char *Label)
    {
        FEditorUIStyle::DrawPopupSectionHeader(Label);
    }

    bool BeginPopupSection(const char *Label, ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, PopupSectionHeaderTextColor);
        const bool bOpen = ImGui::CollapsingHeader(Label, Flags);
        ImGui::PopStyleColor();
        return bOpen;
    }

    void DrawCompactPopupSectionLabel(const char *Label)
    {
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        DrawPopupSectionHeader(Label);
    }

    void DrawShowFlagsPopupContent(FViewportRenderOptions &Opts)
    {
        const ImVec2 CompactFramePadding(6.0f, 3.0f);
        const ImVec2 CompactItemSpacing(PopupComfortItemSpacing.x, 5.0f);
        const ImVec2 CompactItemInnerSpacing(4.0f, 3.0f);
        const float  CompactSliderWidth = 150.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, CompactFramePadding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, CompactItemSpacing);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, CompactItemInnerSpacing);

        ImGui::BeginGroup();
        {
            DrawCompactPopupSectionLabel("COMMON SHOW FLAGS");
            ImGui::Checkbox("Primitives", &Opts.ShowFlags.bPrimitives);
			ImGui::Checkbox("SkeletalMesh", &Opts.ShowFlags.bSkeletalMesh);
            ImGui::Checkbox("Billboard Text", &Opts.ShowFlags.bBillboardText);

            DrawCompactPopupSectionLabel("ACTOR HELPERS");
            ImGui::Checkbox("Grid", &Opts.ShowFlags.bGrid);
            if (Opts.ShowFlags.bGrid)
            {
                // 아래 값들은 그대로 Frame.RenderOptions -> BuildEditorGridCommand -> GridBuffer(b2)로 전달된다.
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Spacing", &Opts.GridSpacing, 0.1f, 10.0f, "%.1f");
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderInt("Half Line Count", &Opts.GridHalfLineCount, 10, 500);
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Grid Line", &Opts.GridRenderSettings.LineThickness, 0.0f, 4.0f, "%.2f");
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Major Line", &Opts.GridRenderSettings.MajorLineThickness, 0.0f, 6.0f, "%.2f");
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderInt("Major Interval", &Opts.GridRenderSettings.MajorLineInterval, 1, 50);
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Minor Intensity", &Opts.GridRenderSettings.MinorIntensity, 0.0f, 2.0f, "%.2f");
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Major Intensity", &Opts.GridRenderSettings.MajorIntensity, 0.0f, 2.0f, "%.2f");
            }

            ImGui::Checkbox("World Axis", &Opts.ShowFlags.bWorldAxis);
            if (Opts.ShowFlags.bWorldAxis)
            {
                // World Axis는 동일 Grid 셰이더의 Axis 전용 DrawPass를 활성화한다.
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Axis Thickness", &Opts.GridRenderSettings.AxisThickness, 0.0f, 8.0f, "%.2f");
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Axis Intensity", &Opts.GridRenderSettings.AxisIntensity, 0.0f, 2.0f, "%.2f");
            }
            ImGui::Checkbox("Gizmo", &Opts.ShowFlags.bGizmo);
            ImGui::SetNextItemWidth(CompactSliderWidth);
            ImGui::SliderFloat("Billboard Icon Scale", &Opts.ActorHelperBillboardScale, 0.1f, 5.0f, "%.2f");

            DrawCompactPopupSectionLabel("DEBUG");
            ImGui::SetNextItemWidth(CompactSliderWidth);
            ImGui::SliderFloat("Line Thickness", &Opts.DebugLineThickness, 1.0f, 12.0f, "%.1f");
            ImGui::Checkbox("Scene BVH (Green)", &Opts.ShowFlags.bSceneBVH);
            ImGui::Checkbox("Scene Octree (Cyan)", &Opts.ShowFlags.bOctree);
            ImGui::Checkbox("World Bound (Magenta)", &Opts.ShowFlags.bWorldBound);
            ImGui::Checkbox("Light Visualization", &Opts.ShowFlags.bLightVisualization);
            if (Opts.ShowFlags.bLightVisualization)
            {
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Directional Scale", &Opts.DirectionalLightVisualizationScale, 0.1f, 5.0f, "%.2f");
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Point Scale", &Opts.PointLightVisualizationScale, 0.1f, 5.0f, "%.2f");
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Spot Scale", &Opts.SpotLightVisualizationScale, 0.1f, 5.0f, "%.2f");
            }
            ImGui::Checkbox("Light Hit Map", &Opts.ShowFlags.bLightHitMap);
            ImGui::Checkbox("Shadow Frustum", &Opts.ShowFlags.bShowShadowFrustum);

            DrawCompactPopupSectionLabel("POST-PROCESSING");
            ImGui::Checkbox("Height Distance Fog", &Opts.ShowFlags.bFog);
            ImGui::Checkbox("Anti-Aliasing (FXAA)", &Opts.ShowFlags.bFXAA);
            if (Opts.ShowFlags.bFXAA)
            {
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("FXAA Edge Threshold", &Opts.EdgeThreshold, 0.06f, 0.333f, "%.3f");
                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("FXAA Edge Threshold Min", &Opts.EdgeThresholdMin, 0.0312f, 0.0833f, "%.4f");
            }
            ImGui::Checkbox("Gamma Correction", &Opts.ShowFlags.bGammaCorrection);
            if (Opts.ShowFlags.bGammaCorrection)
            {
                const char *CurveModes[] = {"sRGB Curve", "Power Gamma"};
                int32       CurveMode = Opts.bUseSRGBCurve ? 0 : 1;
                ImGui::SetNextItemWidth(CompactSliderWidth);
                if (ImGui::Combo("Output Curve", &CurveMode, CurveModes, ARRAYSIZE(CurveModes)))
                {
                    Opts.bUseSRGBCurve = CurveMode == 0;
                }

                if (!Opts.bUseSRGBCurve)
                {
                    ImGui::SetNextItemWidth(CompactSliderWidth);
                    ImGui::SliderFloat("Display Gamma", &Opts.DisplayGamma, 1.0f, 3.0f, "%.2f");
                }

                ImGui::SetNextItemWidth(CompactSliderWidth);
                ImGui::SliderFloat("Gamma Blend", &Opts.GammaCorrectionBlend, 0.0f, 1.0f, "%.2f");
            }
        }
        ImGui::EndGroup();
        ImGui::PopStyleVar(3);
    }

    enum class EToolbarIcon : int32
    {
        Menu = 0,
        Setting,
        AddActor,
        Translate,
        Rotate,
        Scale,
        WorldSpace,
        LocalSpace,
        TranslateSnap,
        RotateSnap,
        ScaleSnap,
        SnapSettings,
        CameraSettings,
        ShowFlag,
        ViewModeLit,
        ViewModeUnlit,
        ViewModeWireframe,
        ViewModeSceneDepth,
        ViewModeWorldNormal,
        ViewModeLightCulling,
        ViewportPerspective,
        ViewportTop,
        ViewportBottom,
        ViewportLeft,
        ViewportRight,
        ViewportFront,
        ViewportBack,
        ViewportFreeOrtho,
        Count
    };

    const char *GetToolbarIconResourceKey(EToolbarIcon Icon)
    {
        switch (Icon)
        {
        case EToolbarIcon::Menu:
            return "Editor.ToolIcon.Menu";
        case EToolbarIcon::Setting:
            return "Editor.ToolIcon.Setting";
        case EToolbarIcon::AddActor:
            return "Editor.ToolIcon.AddActor";
        case EToolbarIcon::Translate:
            return "Editor.ToolIcon.Translate";
        case EToolbarIcon::Rotate:
            return "Editor.ToolIcon.Rotate";
        case EToolbarIcon::Scale:
            return "Editor.ToolIcon.Scale";
        case EToolbarIcon::WorldSpace:
            return "Editor.ToolIcon.WorldSpace";
        case EToolbarIcon::LocalSpace:
            return "Editor.ToolIcon.LocalSpace";
        case EToolbarIcon::TranslateSnap:
            return "Editor.ToolIcon.TranslateSnap";
        case EToolbarIcon::RotateSnap:
            return "Editor.ToolIcon.RotateSnap";
        case EToolbarIcon::ScaleSnap:
            return "Editor.ToolIcon.ScaleSnap";
        case EToolbarIcon::SnapSettings:
            return "Editor.ToolIcon.SnapSettings";
        case EToolbarIcon::CameraSettings:
            return "Editor.ToolIcon.Camera";
        case EToolbarIcon::ShowFlag:
            return "Editor.ToolIcon.ShowFlag";
        case EToolbarIcon::ViewModeLit:
            return "Editor.ToolIcon.ViewMode.Lit";
        case EToolbarIcon::ViewModeUnlit:
            return "Editor.ToolIcon.ViewMode.Unlit";
        case EToolbarIcon::ViewModeWireframe:
            return "Editor.ToolIcon.ViewMode.Wireframe";
        case EToolbarIcon::ViewModeSceneDepth:
            return "Editor.ToolIcon.ViewMode.SceneDepth";
        case EToolbarIcon::ViewModeWorldNormal:
            return "Editor.ToolIcon.ViewMode.WorldNormal";
        case EToolbarIcon::ViewModeLightCulling:
            return "Editor.ToolIcon.ViewMode.LightCulling";
        case EToolbarIcon::ViewportPerspective:
            return "Editor.ToolIcon.Viewport.Perspective";
        case EToolbarIcon::ViewportTop:
            return "Editor.ToolIcon.Viewport.Top";
        case EToolbarIcon::ViewportBottom:
            return "Editor.ToolIcon.Viewport.Bottom";
        case EToolbarIcon::ViewportLeft:
            return "Editor.ToolIcon.Viewport.Left";
        case EToolbarIcon::ViewportRight:
            return "Editor.ToolIcon.Viewport.Right";
        case EToolbarIcon::ViewportFront:
            return "Editor.ToolIcon.Viewport.Front";
        case EToolbarIcon::ViewportBack:
            return "Editor.ToolIcon.Viewport.Back";
        case EToolbarIcon::ViewportFreeOrtho:
            return "Editor.ToolIcon.Viewport.FreeOrtho";
        default:
            return "";
        }
    }

    FString GetToolbarIconPath(EToolbarIcon Icon) { return FResourceManager::Get().ResolvePath(FName(GetToolbarIconResourceKey(Icon))); }

    ID3D11ShaderResourceView **GetToolbarIconTable()
    {
        static ID3D11ShaderResourceView *ToolbarIcons[static_cast<int32>(EToolbarIcon::Count)] = {};
        return ToolbarIcons;
    }

    bool bToolbarIconsLoaded = false;

    void ReleaseToolbarIcons()
    {
        if (!bToolbarIconsLoaded)
        {
            return;
        }

        ID3D11ShaderResourceView **ToolbarIcons = GetToolbarIconTable();
        for (int32 i = 0; i < static_cast<int32>(EToolbarIcon::Count); ++i)
        {
            if (ToolbarIcons[i])
            {
                ToolbarIcons[i]->Release();
                ToolbarIcons[i] = nullptr;
            }
        }

        bToolbarIconsLoaded = false;
    }

    void EnsureToolbarIconsLoaded(FRenderer *RendererPtr)
    {
        if (bToolbarIconsLoaded || !RendererPtr)
        {
            return;
        }

        ID3D11Device              *Device = RendererPtr->GetFD3DDevice().GetDevice();
        ID3D11ShaderResourceView **ToolbarIcons = GetToolbarIconTable();
        for (int32 i = 0; i < static_cast<int32>(EToolbarIcon::Count); ++i)
        {
            const FString FilePath = GetToolbarIconPath(static_cast<EToolbarIcon>(i));
            DirectX::CreateWICTextureFromFile(Device, FPaths::ToWide(FilePath).c_str(), nullptr, &ToolbarIcons[i]);
        }

        bToolbarIconsLoaded = true;
    }

    ImVec2 GetToolbarIconRenderSize(EToolbarIcon Icon, float FallbackSize, float MaxIconSize)
    {
        ID3D11ShaderResourceView *IconSRV = GetToolbarIconTable()[static_cast<int32>(Icon)];
        if (!IconSRV)
        {
            return ImVec2(FallbackSize, FallbackSize);
        }

        ID3D11Resource *Resource = nullptr;
        IconSRV->GetResource(&Resource);
        if (!Resource)
        {
            return ImVec2(FallbackSize, FallbackSize);
        }

        ImVec2                   IconSize(FallbackSize, FallbackSize);
        D3D11_RESOURCE_DIMENSION Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
        Resource->GetType(&Dimension);
        if (Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        {
            ID3D11Texture2D     *Texture2D = static_cast<ID3D11Texture2D *>(Resource);
            D3D11_TEXTURE2D_DESC Desc{};
            Texture2D->GetDesc(&Desc);
            IconSize = ImVec2(static_cast<float>(Desc.Width), static_cast<float>(Desc.Height));
        }
        Resource->Release();

        if (IconSize.x > MaxIconSize || IconSize.y > MaxIconSize)
        {
            const float Scale = (IconSize.x > IconSize.y) ? (MaxIconSize / IconSize.x) : (MaxIconSize / IconSize.y);
            IconSize.x *= Scale;
            IconSize.y *= Scale;
        }

        return IconSize;
    }

    bool DrawToolbarIconButton(const char *Id, EToolbarIcon Icon, const char *FallbackLabel, float FallbackSize, float MaxIconSize,
                               ImU32 IconTint = IM_COL32_WHITE)
    {
        ID3D11ShaderResourceView *IconSRV = GetToolbarIconTable()[static_cast<int32>(Icon)];
        if (!IconSRV)
        {
            return ImGui::Button(FallbackLabel);
        }

        const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, FallbackSize, MaxIconSize);
        return ImGui::ImageButton(Id, reinterpret_cast<ImTextureID>(IconSRV), IconSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                                  ImVec4(0, 0, 0, 0), ImGui::ColorConvertU32ToFloat4(IconTint));
    }

    bool DrawToolbarIconLabelButton(const char *Id, EToolbarIcon Icon, const char *Label, float Width, float Height, float FallbackSize,
                                    float MaxIconSize, ImU32 IconTint = IM_COL32_WHITE)
    {
        constexpr float LabelLeftPadding = 6.0f;
        constexpr float LabelSpacing = 4.0f;
        constexpr float DropdownArrowReserve = 20.0f;
        constexpr float LabelRightPadding = 1.0f;

        const bool bClicked = ImGui::Button(Id, ImVec2(Width, Height));

        ImDrawList  *DrawList = ImGui::GetWindowDrawList();
        const ImVec2 RectMin = ImGui::GetItemRectMin();
        const ImVec2 RectMax = ImGui::GetItemRectMax();

        const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, FallbackSize, MaxIconSize);
        const float  IconY = RectMin.y + ((RectMax.y - RectMin.y) - IconSize.y) * 0.5f;
        const float  IconX = RectMin.x + LabelLeftPadding;

        if (ID3D11ShaderResourceView *IconSRV = GetToolbarIconTable()[static_cast<int32>(Icon)])
        {
            DrawList->AddImage(reinterpret_cast<ImTextureID>(IconSRV), ImVec2(IconX, IconY), ImVec2(IconX + IconSize.x, IconY + IconSize.y),
                               ImVec2(0, 0), ImVec2(1, 1), IconTint);
        }

        const ImVec2 TextSize = ImGui::CalcTextSize(Label);
        const float  TextX = IconX + IconSize.x + LabelSpacing;
        const float  TextY = RectMin.y + ((RectMax.y - RectMin.y) - TextSize.y) * 0.5f;
        const float  TextClipMaxX = RectMax.x - DropdownArrowReserve - LabelRightPadding;
        if (TextClipMaxX > TextX)
        {
            DrawList->PushClipRect(ImVec2(TextX, RectMin.y), ImVec2(TextClipMaxX, RectMax.y), true);
            DrawList->AddText(ImVec2(TextX, TextY), ImGui::GetColorU32(ImGuiCol_Text), Label);
            DrawList->PopClipRect();
        }

        return bClicked;
    }

    bool DrawToolbarIconDropdownButton(const char *Id, EToolbarIcon Icon, float Width, float Height, float FallbackSize, float MaxIconSize,
                                       ImU32 IconTint = IM_COL32_WHITE)
    {
        const bool bClicked = ImGui::Button(Id, ImVec2(Width, Height));

        ImDrawList  *DrawList = ImGui::GetWindowDrawList();
        const ImVec2 RectMin = ImGui::GetItemRectMin();
        const ImVec2 RectMax = ImGui::GetItemRectMax();
        const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, FallbackSize, MaxIconSize);
        const float  ArrowReserve = 12.0f;
        const float  LeftPadding = 8.0f;
        const float  AvailableWidth = (RectMax.x - RectMin.x) - ArrowReserve - LeftPadding;
        const float  IconX = RectMin.x + LeftPadding + (std::max)(0.0f, (AvailableWidth - IconSize.x) * 0.5f);
        const float  IconY = RectMin.y + ((RectMax.y - RectMin.y) - IconSize.y) * 0.5f;

        if (ID3D11ShaderResourceView *IconSRV = GetToolbarIconTable()[static_cast<int32>(Icon)])
        {
            DrawList->AddImage(reinterpret_cast<ImTextureID>(IconSRV), ImVec2(IconX, IconY), ImVec2(IconX + IconSize.x, IconY + IconSize.y),
                               ImVec2(0, 0), ImVec2(1, 1), IconTint);
        }

        return bClicked;
    }

    bool DrawSelectedToolbarIconDropdownButton(const char *Id, EToolbarIcon Icon, bool bSelected, float Width, float Height,
                                               float FallbackSize, float MaxIconSize, ImU32 SelectedTint = UIAccentColor::ToU32())
    {
        const ImU32 Tint = bSelected ? SelectedTint : IM_COL32_WHITE;
        return DrawToolbarIconDropdownButton(Id, Icon, Width, Height, FallbackSize, MaxIconSize, Tint);
    }

    float GetToolbarIconLabelButtonWidth(EToolbarIcon Icon, const char *Label, float FallbackSize, float MaxIconSize)
    {
        constexpr float LabelLeftPadding = 6.0f;
        constexpr float LabelSpacing = 4.0f;
        constexpr float DropdownArrowReserve = 20.0f;
        constexpr float LabelRightPadding = 1.0f;

        const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, FallbackSize, MaxIconSize);
        const float  TextWidth = (Label && Label[0] != '\0') ? ImGui::CalcTextSize(Label).x : 0.0f;
        const float  EffectiveLabelSpacing = (TextWidth > 0.0f) ? LabelSpacing : 0.0f;
        return LabelLeftPadding + IconSize.x + EffectiveLabelSpacing + TextWidth + DropdownArrowReserve + LabelRightPadding;
    }

    float GetToolbarIconDropdownButtonWidth(EToolbarIcon Icon, float FallbackSize, float MaxIconSize)
    {
        constexpr float LeftPadding = 8.0f;
        constexpr float DropdownArrowReserve = 12.0f;
        constexpr float RightPadding = 8.0f;

        const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, FallbackSize, MaxIconSize);
        return LeftPadding + IconSize.x + DropdownArrowReserve + RightPadding;
    }

    void DrawToolbarDropdownArrowForLastItem()
    {
        ImDrawList  *DrawList = ImGui::GetWindowDrawList();
        const ImVec2 RectMin = ImGui::GetItemRectMin();
        const ImVec2 RectMax = ImGui::GetItemRectMax();
        const float  ArrowWidth = 7.0f;
        const float  ArrowHeight = 4.0f;
        const float  ArrowRightPadding = 7.0f;
        const float  ArrowBottomPadding = 7.0f;
        const ImVec2 Center(RectMax.x - ArrowRightPadding - ArrowWidth * 0.5f, RectMax.y - ArrowBottomPadding - ArrowHeight * 0.5f);
        const ImU32  ArrowColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);

        DrawList->AddTriangleFilled(ImVec2(Center.x - ArrowWidth * 0.5f, Center.y - ArrowHeight * 0.5f),
                                    ImVec2(Center.x + ArrowWidth * 0.5f, Center.y - ArrowHeight * 0.5f),
                                    ImVec2(Center.x, Center.y + ArrowHeight * 0.5f), ArrowColor);
    }

    EToolbarIcon GetViewModeToolbarIcon(EViewMode ViewMode)
    {
        switch (ViewMode)
        {
        case EViewMode::Unlit:
            return EToolbarIcon::ViewModeUnlit;
        case EViewMode::Wireframe:
            return EToolbarIcon::ViewModeWireframe;
        case EViewMode::SceneDepth:
            return EToolbarIcon::ViewModeSceneDepth;
        case EViewMode::WorldNormal:
            return EToolbarIcon::ViewModeWorldNormal;
        case EViewMode::LightCulling:
            return EToolbarIcon::ViewModeLightCulling;
        case EViewMode::Lit_Gouraud:
        case EViewMode::Lit_Lambert:
        case EViewMode::Lit_Phong:
        default:
            return EToolbarIcon::ViewModeLit;
        }
    }

    EToolbarIcon GetViewportTypeToolbarIcon(ELevelViewportType ViewportType)
    {
        switch (ViewportType)
        {
        case ELevelViewportType::Top:
            return EToolbarIcon::ViewportTop;
        case ELevelViewportType::Bottom:
            return EToolbarIcon::ViewportBottom;
        case ELevelViewportType::Left:
            return EToolbarIcon::ViewportLeft;
        case ELevelViewportType::Right:
            return EToolbarIcon::ViewportRight;
        case ELevelViewportType::Front:
            return EToolbarIcon::ViewportFront;
        case ELevelViewportType::Back:
            return EToolbarIcon::ViewportBack;
        case ELevelViewportType::FreeOrthographic:
            return EToolbarIcon::ViewportFreeOrtho;
        case ELevelViewportType::Perspective:
        default:
            return EToolbarIcon::ViewportPerspective;
        }
    }

    void PushToolbarButtonStyle()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.36f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.35f, 0.39f, 0.9f));
    }

    void PopToolbarButtonStyle()
    {
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }

    void ShowItemTooltip(const char *Tooltip)
    {
        if (Tooltip && Tooltip[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(Tooltip);
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }
    }

    void DrawSnapPopupOptions(const char *Label, bool &bEnabled, float &Value, const float *Options, int32 OptionCount, const char *Format)
    {
        ImGui::PushID(Label);
        ImGui::PushStyleColor(ImGuiCol_Text, CameraPopupLabelColor);
        ImGui::TextUnformatted(Label);
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 3.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(4.0f, 3.0f));
        ImGui::Checkbox("Enabled", &bEnabled);
        ImGui::Spacing();

        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::InputFloat("Custom Size", &Value, 0.0f, 0.0f, "%.5g"))
        {
            Value = (std::max)(Value, 0.00001f);
        }
        ImGui::Spacing();

        for (int32 Index = 0; Index < OptionCount; ++Index)
        {
            char ChoiceLabel[32];
            snprintf(ChoiceLabel, sizeof(ChoiceLabel), Format, Options[Index]);
            const float Delta = Value - Options[Index];
            const bool  bSelected = (Delta < 0.0f ? -Delta : Delta) < 0.0001f;
            if (bSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_FrameBg, SnapPopupSelectedColor);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, SnapPopupSelectedHoverColor);
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, SnapPopupSelectedActiveColor);
                ImGui::PushStyleColor(ImGuiCol_Text, SnapPopupSelectedTextColor);
                ImGui::PushStyleColor(ImGuiCol_CheckMark, SnapPopupSelectedCheckColor);
            }

            if (ImGui::RadioButton(ChoiceLabel, bSelected))
            {
                Value = Options[Index];
            }

            if (bSelected)
            {
                ImGui::PopStyleColor(5);
            }
        }

        ImGui::PopStyleVar(2);
        ImGui::PopID();
    }

    void RenderSnapPopupContent(int32 SlotIndex, FLevelEditorSettings &Settings, float FallbackSize, float MaxIconSize)
    {
        static const float TranslationSnapSizes[] = {1.0f, 5.0f, 10.0f, 50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f};
        static const float RotationSnapSizes[] = {5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 60.0f, 90.0f, 120.0f};
        static const float ScaleSnapSizes[] = {0.03125f, 0.0625f, 0.1f, 0.125f, 0.25f, 0.5f, 1.0f, 10.0f};

        if (GSnapPopupTab[SlotIndex] == ESnapPopupType::None)
        {
            GSnapPopupTab[SlotIndex] = ESnapPopupType::Location;
        }

        const float ButtonHeight = 26.0f;
        const float LocationWidth = GetToolbarIconLabelButtonWidth(EToolbarIcon::TranslateSnap, "Location", FallbackSize, MaxIconSize);
        const float RotationWidth = GetToolbarIconLabelButtonWidth(EToolbarIcon::RotateSnap, "Rotation", FallbackSize, MaxIconSize);
        const float ScaleWidth = GetToolbarIconLabelButtonWidth(EToolbarIcon::ScaleSnap, "Scale", FallbackSize, MaxIconSize);

        auto DrawSnapTabButton = [&](const char *Id, EToolbarIcon Icon, const char *Label, ESnapPopupType Type, float Width)
        {
            const bool bSelected = GSnapPopupTab[SlotIndex] == Type;
            if (bSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, SnapPopupTabSelectedColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, SnapPopupTabSelectedHoverColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, SnapPopupTabSelectedActiveColor);
                ImGui::PushStyleColor(ImGuiCol_Text, SnapPopupTabSelectedTextColor);
                ImGui::PushStyleColor(ImGuiCol_TextDisabled, SnapPopupTabSelectedTextColor);
            }

            if (DrawToolbarIconLabelButton(Id, Icon, Label, Width, ButtonHeight, FallbackSize, MaxIconSize,
                                           bSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32_WHITE))
            {
                GSnapPopupTab[SlotIndex] = Type;
            }
            DrawToolbarDropdownArrowForLastItem();

            if (bSelected)
            {
                ImGui::PopStyleColor(5);
            }
        };

        DrawSnapTabButton("##SnapTabLocation", EToolbarIcon::TranslateSnap, "Location", ESnapPopupType::Location, LocationWidth);
        ImGui::SameLine(0.0f, 8.0f);
        DrawSnapTabButton("##SnapTabRotation", EToolbarIcon::RotateSnap, "Rotation", ESnapPopupType::Rotation, RotationWidth);
        ImGui::SameLine(0.0f, 8.0f);
        DrawSnapTabButton("##SnapTabScale", EToolbarIcon::ScaleSnap, "Scale", ESnapPopupType::Scale, ScaleWidth);

        FEditorUIStyle::DrawPopupSeparator(4.0f, 6.0f);

        switch (GSnapPopupTab[SlotIndex])
        {
        case ESnapPopupType::Location:
            DrawSnapPopupOptions("Location", Settings.bEnableTranslationSnap, Settings.TranslationSnapSize, TranslationSnapSizes, 9,
                                 "%.0f");
            break;
        case ESnapPopupType::Rotation:
            DrawSnapPopupOptions("Rotation", Settings.bEnableRotationSnap, Settings.RotationSnapSize, RotationSnapSizes, 8, "%.0f deg");
            break;
        case ESnapPopupType::Scale:
            DrawSnapPopupOptions("Scale", Settings.bEnableScaleSnap, Settings.ScaleSnapSize, ScaleSnapSizes, 8, "%.5g");
            break;
        default:
            break;
        }
    }

    void RenderSnapToolbarButton(int32 SlotIndex, FLevelEditorSettings &Settings, float Width, float FallbackSize, float MaxIconSize)
    {
        char ButtonId[64];
        char PopupId[64];
        snprintf(ButtonId, sizeof(ButtonId), "##SnapSettings_%d", SlotIndex);
        snprintf(PopupId, sizeof(PopupId), "SnapPopup_%d", SlotIndex);

        Width = (std::max)(Width, GetToolbarIconDropdownButtonWidth(EToolbarIcon::SnapSettings, FallbackSize, MaxIconSize));

        const bool bAnySnapEnabled = Settings.bEnableTranslationSnap || Settings.bEnableRotationSnap || Settings.bEnableScaleSnap;
        if (DrawSelectedToolbarIconDropdownButton(ButtonId, EToolbarIcon::SnapSettings, bAnySnapEnabled, Width, 26.0f, FallbackSize,
                                                  MaxIconSize))
        {
            if (GSnapPopupTab[SlotIndex] == ESnapPopupType::None)
            {
                GSnapPopupTab[SlotIndex] = ESnapPopupType::Location;
            }
            ImGui::OpenPopup(PopupId);
        }
        DrawToolbarDropdownArrowForLastItem();
        ShowItemTooltip("Snap Settings");

        ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_Appearing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        PushCommonPopupBgColor();
        ImGui::PushStyleColor(ImGuiCol_Border, SnapPopupBorderColor);
        if (ImGui::BeginPopup(PopupId))
        {
            RenderSnapPopupContent(SlotIndex, Settings, FallbackSize, MaxIconSize);
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

    bool DrawSearchInputWithIcon(const char *Id, const char *Hint, char *Buffer, size_t BufferSize, float Width)
    {
        ImGuiStyle &Style = ImGui::GetStyle();
        ImGui::SetNextItemWidth(Width);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 11.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Style.FramePadding.x + 20.0f, Style.FramePadding.y));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.42f, 0.42f, 0.45f, 0.90f));
        const std::string PaddedHint = std::string("   ") + Hint;
        const bool        bChanged = ImGui::InputTextWithHint(Id, PaddedHint.c_str(), Buffer, BufferSize);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        if (ID3D11ShaderResourceView *SearchIcon =
                FResourceManager::Get().FindLoadedTexture(FResourceManager::Get().ResolvePath(FName("Editor.Icon.Search"))).Get())
        {
            const ImVec2 Min = ImGui::GetItemRectMin();
            const float  IconSize = ImGui::GetFrameHeight() - 12.0f;
            const float  IconY = Min.y + (ImGui::GetFrameHeight() - IconSize) * 0.5f;
            ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(SearchIcon), ImVec2(Min.x + 7.0f, IconY),
                                                 ImVec2(Min.x + 7.0f + IconSize, IconY + IconSize), ImVec2(1.0f, 0.0f), ImVec2(0.0f, 1.0f),
                                                 IM_COL32(210, 210, 210, 255));
        }

        return bChanged;
    }

    float GetViewportPaneToolbarHeight(float PaneWidth) { return FViewportToolbar::GetHeight(PaneWidth); }

    const char* GetBasicShapeFileStemFromKey(const char* MeshKey)
    {
        if (!MeshKey) return nullptr;
        if (std::strcmp(MeshKey, "Default.Mesh.BasicShape.Cube") == 0) return "Cube";
        if (std::strcmp(MeshKey, "Default.Mesh.BasicShape.Sphere") == 0) return "Sphere";
        if (std::strcmp(MeshKey, "Default.Mesh.BasicShape.Cylinder") == 0) return "Cylinder";
        if (std::strcmp(MeshKey, "Default.Mesh.BasicShape.Cone") == 0) return "Cone";
        if (std::strcmp(MeshKey, "Default.Mesh.BasicShape.Plane") == 0) return "Plane";
        if (std::strcmp(MeshKey, "Default.Mesh.BasicShape.SphereLowpoly") == 0) return "SphereLowpoly";
        return nullptr;
    }

    FString FindBasicShapeObjPath(const char* MeshKey)
    {
        const char* Stem = GetBasicShapeFileStemFromKey(MeshKey);
        if (!Stem) return "";

        const std::filesystem::path RootPath(FPaths::RootDir());
        const std::filesystem::path ContentPath(FPaths::ContentDir());
        const std::filesystem::path EngineContentPath(FPaths::EngineContentDir());
        const std::filesystem::path AssetPath(FPaths::AssetDir());
        const std::wstring StemW = FPaths::ToWide(Stem);
        const std::wstring ObjFileName = StemW + L".obj";

        const std::filesystem::path Candidates[] = {
            std::filesystem::path(FPaths::EngineBasicShapeSourceDir()) / ObjFileName,
            ContentPath / L"Meshes" / L"BasicShape" / ObjFileName,
            AssetPath / L"BasicShape" / ObjFileName,
            RootPath / L"BasicShape" / ObjFileName,
        };

        for (const std::filesystem::path& Candidate : Candidates)
        {
            if (std::filesystem::exists(Candidate))
            {
                return FPaths::ToUtf8(Candidate.lexically_relative(RootPath).generic_wstring());
            }
        }

        // Last-resort scan. This keeps Place Actors working even when the resource scan
        // file only registered cooked .uasset files and the original BasicShape .obj files
        // live in a slightly different content subdirectory.
        const std::filesystem::path ScanRoots[] = { std::filesystem::path(FPaths::EngineSourceDir()), EngineContentPath, ContentPath, AssetPath };
        for (const std::filesystem::path& ScanRoot : ScanRoots)
        {
            std::error_code ErrorCode;
            if (!std::filesystem::exists(ScanRoot, ErrorCode) || !std::filesystem::is_directory(ScanRoot, ErrorCode))
            {
                continue;
            }

            std::filesystem::directory_options Options = std::filesystem::directory_options::skip_permission_denied;
            std::filesystem::recursive_directory_iterator It(ScanRoot, Options, ErrorCode);
            const std::filesystem::recursive_directory_iterator End;
            for (; It != End; It.increment(ErrorCode))
            {
                if (ErrorCode)
                {
                    ErrorCode.clear();
                    continue;
                }

                const std::filesystem::directory_entry& Entry = *It;
                if (!Entry.is_regular_file(ErrorCode))
                {
                    ErrorCode.clear();
                    continue;
                }
                if (_wcsicmp(Entry.path().filename().c_str(), ObjFileName.c_str()) != 0) continue;

                std::filesystem::path Parent = Entry.path().parent_path();
                bool bInsideBasicShapeDirectory = false;
                while (!Parent.empty())
                {
                    if (_wcsicmp(Parent.filename().c_str(), L"BasicShape") == 0)
                    {
                        bInsideBasicShapeDirectory = true;
                        break;
                    }
                    const std::filesystem::path Next = Parent.parent_path();
                    if (Next == Parent) break;
                    Parent = Next;
                }

                if (bInsideBasicShapeDirectory)
                {
                    return FPaths::ToUtf8(Entry.path().lexically_relative(RootPath).generic_wstring());
                }
            }
        }

        return "";
    }

    bool DoesProjectRelativePathExist(const FString& Path)
    {
        if (Path.empty())
        {
            return false;
        }

        std::filesystem::path Candidate(FPaths::ToWide(Path));
        if (!Candidate.is_absolute())
        {
            Candidate = std::filesystem::path(FPaths::RootDir()) / Candidate;
        }
        return std::filesystem::exists(Candidate.lexically_normal());
    }

    FString GetRegisteredMeshPath(const char *MeshKey)
    {
        // Component assignment policy: scene components should reference cooked .uasset files.
        // Built-in shapes now live under Asset/Content/BasicShape. Until real .uasset
        // files are added there, keep the raw BasicShape OBJ fallback alive for editor spawning.
        if (const FMeshResource *MeshResource = FResourceManager::Get().FindMesh(FName(MeshKey)))
        {
            if (DoesProjectRelativePathExist(MeshResource->Path))
            {
                return MeshResource->Path;
            }
        }

        if (FString BasicShapeObjPath = FindBasicShapeObjPath(MeshKey); !BasicShapeObjPath.empty())
        {
            return BasicShapeObjPath;
        }

        if (const FMeshResource *MeshResource = FResourceManager::Get().FindMesh(FName(MeshKey)))
        {
            return MeshResource->Path;
        }

        return "";
    }
} // namespace

// ─── 레이아웃별 슬롯 수 ─────────────────────────────────────

int32 FLevelViewportLayout::GetSlotCount(EViewportLayout Layout)
{
    switch (Layout)
    {
    case EViewportLayout::OnePane:
        return 1;
    case EViewportLayout::TwoPanesHoriz:
    case EViewportLayout::TwoPanesVert:
        return 2;
    case EViewportLayout::ThreePanesLeft:
    case EViewportLayout::ThreePanesRight:
    case EViewportLayout::ThreePanesTop:
    case EViewportLayout::ThreePanesBottom:
        return 3;
    default:
        return 4;
    }
}

// ─── 레이아웃 아이콘 리소스 매핑 ────────────────────────────

const char *GetLayoutIconResourceKey(EViewportLayout Layout)
{
    switch (Layout)
    {
    case EViewportLayout::OnePane:
        return "Editor.Layout.OnePane";
    case EViewportLayout::TwoPanesHoriz:
        return "Editor.Layout.TwoPanesHoriz";
    case EViewportLayout::TwoPanesVert:
        return "Editor.Layout.TwoPanesVert";
    case EViewportLayout::ThreePanesLeft:
        return "Editor.Layout.ThreePanesLeft";
    case EViewportLayout::ThreePanesRight:
        return "Editor.Layout.ThreePanesRight";
    case EViewportLayout::ThreePanesTop:
        return "Editor.Layout.ThreePanesTop";
    case EViewportLayout::ThreePanesBottom:
        return "Editor.Layout.ThreePanesBottom";
    case EViewportLayout::FourPanes2x2:
        return "Editor.Layout.FourPanes2x2";
    case EViewportLayout::FourPanesLeft:
        return "Editor.Layout.FourPanesLeft";
    case EViewportLayout::FourPanesRight:
        return "Editor.Layout.FourPanesRight";
    case EViewportLayout::FourPanesTop:
        return "Editor.Layout.FourPanesTop";
    case EViewportLayout::FourPanesBottom:
        return "Editor.Layout.FourPanesBottom";
    default:
        return "";
    }
}

const char *GetLayoutDisplayName(EViewportLayout Layout)
{
    switch (Layout)
    {
    case EViewportLayout::OnePane:
        return "One Pane";
    case EViewportLayout::TwoPanesHoriz:
        return "Two Panes Horizontal";
    case EViewportLayout::TwoPanesVert:
        return "Two Panes Vertical";
    case EViewportLayout::ThreePanesLeft:
        return "Three Panes Left";
    case EViewportLayout::ThreePanesRight:
        return "Three Panes Right";
    case EViewportLayout::ThreePanesTop:
        return "Three Panes Top";
    case EViewportLayout::ThreePanesBottom:
        return "Three Panes Bottom";
    case EViewportLayout::FourPanes2x2:
        return "Four Panes 2x2";
    case EViewportLayout::FourPanesLeft:
        return "Four Panes Left";
    case EViewportLayout::FourPanesRight:
        return "Four Panes Right";
    case EViewportLayout::FourPanesTop:
        return "Four Panes Top";
    case EViewportLayout::FourPanesBottom:
        return "Four Panes Bottom";
    default:
        return "Viewport Layout";
    }
}

// ─── 레이아웃 아이콘 로드 / 해제 ───────────────────────────

void FLevelViewportLayout::LoadLayoutIcons(ID3D11Device *Device)
{
    if (!Device)
        return;

    for (int32 i = 0; i < static_cast<int32>(EViewportLayout::MAX); ++i)
    {
        const EViewportLayout Layout = static_cast<EViewportLayout>(i);
        const FString         Path = FResourceManager::Get().ResolvePath(FName(GetLayoutIconResourceKey(Layout)));
        DirectX::CreateWICTextureFromFile(Device, FPaths::ToWide(Path).c_str(), nullptr, &LayoutIcons[i]);
    }
}

void FLevelViewportLayout::ReleaseLayoutIcons()
{
    for (int32 i = 0; i < static_cast<int32>(EViewportLayout::MAX); ++i)
    {
        if (LayoutIcons[i])
        {
            LayoutIcons[i]->Release();
            LayoutIcons[i] = nullptr;
        }
    }
}

// Init / Release

void FLevelViewportLayout::Init(UEditorEngine *InEditor, FWindowsWindow *InWindow, FRenderer &InRenderer,
                                FSelectionManager *InSelectionManager)
{
    Editor = InEditor;
    Window = InWindow;
    RendererPtr = &InRenderer;
    SelectionManager = InSelectionManager;

    // 아이콘 로드
    LoadLayoutIcons(InRenderer.GetFD3DDevice().GetDevice());
    PlayToolbar.Init(InEditor, InRenderer.GetFD3DDevice().GetDevice());

    // Play/Stop 툴바 초기화

    // LevelViewportClient 생성 (단일 뷰포트)
    auto *LevelVC = new FLevelEditorViewportClient();
    LevelVC->SetOverlayStatSystem(&Editor->GetOverlayStatSystem());
    LevelVC->SetSettings(&FLevelEditorSettings::Get());
    LevelVC->Init(Window);
    LevelVC->SetViewportSize(Window->GetWidth(), Window->GetHeight());
    LevelVC->SetSelectionManager(SelectionManager);

    auto *VP = new FViewport();
    VP->Initialize(InRenderer.GetFD3DDevice().GetDevice(), static_cast<uint32>(Window->GetWidth()),
                   static_cast<uint32>(Window->GetHeight()));
    VP->SetClient(LevelVC);
    LevelVC->SetViewport(VP);

    LevelVC->InitializeCameraState();
    LevelVC->ResetCamera();
    ApplyProjectViewportSettings(LevelVC->GetRenderOptions());

    AllViewportClients.push_back(LevelVC);
    LevelViewportClients.push_back(LevelVC);
    SetActiveViewport(LevelVC);

    ViewportWindows[0] = new SWindow();
    LevelVC->SetLayoutWindow(ViewportWindows[0]);
    ActiveSlotCount = 1;
    CurrentLayout = EViewportLayout::OnePane;
}

void FLevelViewportLayout::Release()
{
    SSplitter::DestroyTree(RootSplitter);
    RootSplitter = nullptr;
    DraggingSplitter = nullptr;

    for (int32 i = 0; i < MaxViewportSlots; ++i)
    {
        delete ViewportWindows[i];
        ViewportWindows[i] = nullptr;
    }

    ActiveViewportClient = nullptr;
    for (FEditorViewportClient *VC : AllViewportClients)
    {
        if (FViewport *VP = VC->GetViewport())
        {
            VP->Release();
            delete VP;
        }
        delete VC;
    }
    AllViewportClients.clear();
    LevelViewportClients.clear();

    ReleaseLayoutIcons();
    ReleaseToolbarIcons();
    PlayToolbar.Release();
}

float FLevelViewportLayout::GetFrameToolbarHeight() const
{
    return PlayToolbar.GetDesiredHeight();
}

void FLevelViewportLayout::RenderFrameToolbar(float Width)
{
    PlayToolbar.Render(Width);
}

// ─── 활성 뷰포트 ────────────────────────────────────────────

void FLevelViewportLayout::SetActiveViewport(FLevelEditorViewportClient *InClient)
{
    if (ActiveViewportClient)
    {
        ActiveViewportClient->SetActive(false);
    }
    ActiveViewportClient = InClient;
    if (ActiveViewportClient)
    {
        ActiveViewportClient->SetActive(true);
        // Editor viewport cameras are not UCameraComponent instances and are not registered as World active cameras.
    }
}

void FLevelViewportLayout::ResetViewport(UWorld *InWorld)
{
    for (FLevelEditorViewportClient *VC : LevelViewportClients)
    {
        VC->InitializeCameraState();
        VC->ResetCamera();

        // 카메라 재생성 후 현재 뷰포트 크기로 AspectRatio 동기화
        if (FViewport *VP = VC->GetViewport())
        {
            FEditorViewportCamera *Cam = VC->GetCamera();
            if (Cam && VP->GetWidth() > 0 && VP->GetHeight() > 0)
            {
                Cam->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
            }
        }

        // 기존 뷰포트 타입(Ortho 방향 등)을 새 카메라에 재적용
        VC->SetViewportType(VC->GetRenderOptions().ViewportType);
    }
    // Editor viewport cameras are kept outside UWorld. PIE creates/uses real UCameraComponent instances separately.
}

void FLevelViewportLayout::DestroyAllCameras()
{
    for (FLevelEditorViewportClient *VC : LevelViewportClients)
    {
        VC->ReleaseCameraState();
    }
}

void FLevelViewportLayout::DetachSceneResourcesForWorldChange()
{
    for (FLevelEditorViewportClient *VC : LevelViewportClients)
    {
        if (VC)
        {
            VC->DetachSceneResourcesForWorldChange();
        }
    }
}

void FLevelViewportLayout::DisableWorldAxisForPIE()
{
    if (bHasSavedWorldAxisVisibility)
    {
        for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
        {
            LevelViewportClients[i]->GetRenderOptions().ShowFlags.bGrid = false;
            LevelViewportClients[i]->GetRenderOptions().ShowFlags.bWorldAxis = false;
        }
        return;
    }

    for (int32 i = 0; i < MaxViewportSlots; ++i)
    {
        SavedGridVisibility[i] = false;
        SavedWorldAxisVisibility[i] = false;
    }

    for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
    {
        FViewportRenderOptions &Opts = LevelViewportClients[i]->GetRenderOptions();
        SavedGridVisibility[i] = Opts.ShowFlags.bGrid;
        SavedWorldAxisVisibility[i] = Opts.ShowFlags.bWorldAxis;
        Opts.ShowFlags.bGrid = false;
        Opts.ShowFlags.bWorldAxis = false;
    }

    bHasSavedWorldAxisVisibility = true;
}

void FLevelViewportLayout::RestoreWorldAxisAfterPIE()
{
    if (!bHasSavedWorldAxisVisibility)
    {
        return;
    }

    for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
    {
        LevelViewportClients[i]->GetRenderOptions().ShowFlags.bGrid = SavedGridVisibility[i];
        LevelViewportClients[i]->GetRenderOptions().ShowFlags.bWorldAxis = SavedWorldAxisVisibility[i];
    }

    bHasSavedWorldAxisVisibility = false;
}

// ─── 뷰포트 슬롯 관리 ───────────────────────────────────────

void FLevelViewportLayout::EnsureViewportSlots(int32 RequiredCount)
{
    // 현재 슬롯보다 더 필요하면 추가 생성
    while (static_cast<int32>(LevelViewportClients.size()) < RequiredCount)
    {
        int32 Idx = static_cast<int32>(LevelViewportClients.size());

        auto *LevelVC = new FLevelEditorViewportClient();
        LevelVC->SetOverlayStatSystem(&Editor->GetOverlayStatSystem());
        LevelVC->SetSettings(&FLevelEditorSettings::Get());
        LevelVC->Init(Window);
        LevelVC->SetViewportSize(Window->GetWidth(), Window->GetHeight());
            LevelVC->SetSelectionManager(SelectionManager);

        auto *VP = new FViewport();
        VP->Initialize(RendererPtr->GetFD3DDevice().GetDevice(), static_cast<uint32>(Window->GetWidth()),
                       static_cast<uint32>(Window->GetHeight()));
        VP->SetClient(LevelVC);
        LevelVC->SetViewport(VP);

        LevelVC->InitializeCameraState();
        LevelVC->ResetCamera();
        ApplyProjectViewportSettings(LevelVC->GetRenderOptions());

        AllViewportClients.push_back(LevelVC);
        LevelViewportClients.push_back(LevelVC);

        ViewportWindows[Idx] = new SWindow();
        LevelVC->SetLayoutWindow(ViewportWindows[Idx]);
    }
}

void FLevelViewportLayout::ShrinkViewportSlots(int32 RequiredCount)
{
    while (static_cast<int32>(LevelViewportClients.size()) > RequiredCount)
    {
        FLevelEditorViewportClient *VC = LevelViewportClients.back();
        int32                       Idx = static_cast<int32>(LevelViewportClients.size()) - 1;
        LevelViewportClients.pop_back();

        for (auto It = AllViewportClients.begin(); It != AllViewportClients.end(); ++It)
        {
            if (*It == VC)
            {
                AllViewportClients.erase(It);
                break;
            }
        }

        if (ActiveViewportClient == VC)
            SetActiveViewport(LevelViewportClients[0]);

        if (FViewport *VP = VC->GetViewport())
        {
            VP->Release();
            delete VP;
        }
        VC->ReleaseCameraState();
        delete VC;

        delete ViewportWindows[Idx];
        ViewportWindows[Idx] = nullptr;
    }
}

// ─── SSplitter 트리 빌드 ─────────────────────────────────────

SSplitter *FLevelViewportLayout::BuildSplitterTree(EViewportLayout Layout)
{
    SWindow **W = ViewportWindows;

    switch (Layout)
    {
    case EViewportLayout::OnePane:
        return nullptr; // 트리 불필요

    case EViewportLayout::TwoPanesHoriz:
    {
        // H 분할 [0] | [1]
        auto *Root = new SSplitterH();
        Root->SetSideLT(W[0]);
        Root->SetSideRB(W[1]);
        return Root;
    }
    case EViewportLayout::TwoPanesVert:
    {
        // V 분할 [0] / [1]
        auto *Root = new SSplitterV();
        Root->SetSideLT(W[0]);
        Root->SetSideRB(W[1]);
        return Root;
    }
    case EViewportLayout::ThreePanesLeft:
    {
        // H 분할 [0] | V([1]/[2])
        auto *RightV = new SSplitterV();
        RightV->SetSideLT(W[1]);
        RightV->SetSideRB(W[2]);
        auto *Root = new SSplitterH();
        Root->SetSideLT(W[0]);
        Root->SetSideRB(RightV);
        return Root;
    }
    case EViewportLayout::ThreePanesRight:
    {
        // H 분할 V([0]/[1]) | [2]
        auto *LeftV = new SSplitterV();
        LeftV->SetSideLT(W[0]);
        LeftV->SetSideRB(W[1]);
        auto *Root = new SSplitterH();
        Root->SetSideLT(LeftV);
        Root->SetSideRB(W[2]);
        return Root;
    }
    case EViewportLayout::ThreePanesTop:
    {
        // V 분할 [0] / H([1]|[2])
        auto *BottomH = new SSplitterH();
        BottomH->SetSideLT(W[1]);
        BottomH->SetSideRB(W[2]);
        auto *Root = new SSplitterV();
        Root->SetSideLT(W[0]);
        Root->SetSideRB(BottomH);
        return Root;
    }
    case EViewportLayout::ThreePanesBottom:
    {
        // V 분할 H([0]|[1]) / [2]
        auto *TopH = new SSplitterH();
        TopH->SetSideLT(W[0]);
        TopH->SetSideRB(W[1]);
        auto *Root = new SSplitterV();
        Root->SetSideLT(TopH);
        Root->SetSideRB(W[2]);
        return Root;
    }
    case EViewportLayout::FourPanes2x2:
    {
        // H 분할 V([0]/[2]) | V([1]/[3])
        auto *LeftV = new SSplitterV();
        LeftV->SetSideLT(W[0]);
        LeftV->SetSideRB(W[2]);
        auto *RightV = new SSplitterV();
        RightV->SetSideLT(W[1]);
        RightV->SetSideRB(W[3]);
        auto *Root = new SSplitterH();
        Root->SetSideLT(LeftV);
        Root->SetSideRB(RightV);
        return Root;
    }
    case EViewportLayout::FourPanesLeft:
    {
        // H 분할 [0] | V([1] / V([2]/[3]))
        auto *InnerV = new SSplitterV();
        InnerV->SetSideLT(W[2]);
        InnerV->SetSideRB(W[3]);
        auto *RightV = new SSplitterV();
        RightV->SetRatio(0.333f);
        RightV->SetSideLT(W[1]);
        RightV->SetSideRB(InnerV);
        auto *Root = new SSplitterH();
        Root->SetSideLT(W[0]);
        Root->SetSideRB(RightV);
        return Root;
    }
    case EViewportLayout::FourPanesRight:
    {
        // H 분할 V([0] / V([1]/[2])) | [3]
        auto *InnerV = new SSplitterV();
        InnerV->SetSideLT(W[1]);
        InnerV->SetSideRB(W[2]);
        auto *LeftV = new SSplitterV();
        LeftV->SetRatio(0.333f);
        LeftV->SetSideLT(W[0]);
        LeftV->SetSideRB(InnerV);
        auto *Root = new SSplitterH();
        Root->SetSideLT(LeftV);
        Root->SetSideRB(W[3]);
        return Root;
    }
    case EViewportLayout::FourPanesTop:
    {
        // V 분할 [0] / H([1] | H([2]|[3]))
        auto *InnerH = new SSplitterH();
        InnerH->SetSideLT(W[2]);
        InnerH->SetSideRB(W[3]);
        auto *BottomH = new SSplitterH();
        BottomH->SetRatio(0.333f);
        BottomH->SetSideLT(W[1]);
        BottomH->SetSideRB(InnerH);
        auto *Root = new SSplitterV();
        Root->SetSideLT(W[0]);
        Root->SetSideRB(BottomH);
        return Root;
    }
    case EViewportLayout::FourPanesBottom:
    {
        // V 분할 H([0] | H([1]|[2])) / [3]
        auto *InnerH = new SSplitterH();
        InnerH->SetSideLT(W[1]);
        InnerH->SetSideRB(W[2]);
        auto *TopH = new SSplitterH();
        TopH->SetRatio(0.333f);
        TopH->SetSideLT(W[0]);
        TopH->SetSideRB(InnerH);
        auto *Root = new SSplitterV();
        Root->SetSideLT(TopH);
        Root->SetSideRB(W[3]);
        return Root;
    }
    default:
        return nullptr;
    }
}

int32 FLevelViewportLayout::GetActiveViewportSlotIndex() const
{
    for (int32 i = 0; i < static_cast<int32>(LevelViewportClients.size()); ++i)
    {
        if (LevelViewportClients[i] == ActiveViewportClient)
        {
            return i;
        }
    }
    return 0;
}

bool FLevelViewportLayout::ShouldRenderViewportClient(const FLevelEditorViewportClient *ViewportClient) const
{
    if (!ViewportClient)
    {
        return false;
    }

    for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
    {
        if (LevelViewportClients[i] == ViewportClient)
        {
            return true;
        }
    }

    return false;
}

void FLevelViewportLayout::SwapViewportSlots(int32 SlotA, int32 SlotB)
{
    if (SlotA == SlotB)
    {
        return;
    }

    if (SlotA < 0 || SlotB < 0 || SlotA >= MaxViewportSlots || SlotB >= MaxViewportSlots ||
        SlotA >= static_cast<int32>(LevelViewportClients.size()) || SlotB >= static_cast<int32>(LevelViewportClients.size()))
    {
        return;
    }

    std::swap(LevelViewportClients[SlotA], LevelViewportClients[SlotB]);
    std::swap(ViewportWindows[SlotA], ViewportWindows[SlotB]);

    if (LevelViewportClients[SlotA])
    {
        LevelViewportClients[SlotA]->SetLayoutWindow(ViewportWindows[SlotA]);
    }
    if (LevelViewportClients[SlotB])
    {
        LevelViewportClients[SlotB]->SetLayoutWindow(ViewportWindows[SlotB]);
    }
}

void FLevelViewportLayout::RestoreMaximizedViewportToOriginalSlot()
{
    if (MaximizedOriginalSlotIndex <= 0)
    {
        return;
    }

    SwapViewportSlots(0, MaximizedOriginalSlotIndex);
    MaximizedOriginalSlotIndex = 0;
}

bool FLevelViewportLayout::SubtreeContainsWindow(SWindow *Node, SWindow *TargetWindow) const
{
    if (!Node || !TargetWindow)
    {
        return false;
    }

    if (Node == TargetWindow)
    {
        return true;
    }

    SSplitter *Splitter = SSplitter::AsSplitter(Node);
    return Splitter &&
           (SubtreeContainsWindow(Splitter->GetSideLT(), TargetWindow) || SubtreeContainsWindow(Splitter->GetSideRB(), TargetWindow));
}

bool FLevelViewportLayout::ConfigureCollapseToSlot(SSplitter *Node, SWindow *TargetWindow, bool bAnimate)
{
    if (!Node || !TargetWindow)
    {
        return false;
    }

    const bool bTargetInLT = SubtreeContainsWindow(Node->GetSideLT(), TargetWindow);
    const bool bTargetInRB = SubtreeContainsWindow(Node->GetSideRB(), TargetWindow);
    if (!bTargetInLT && !bTargetInRB)
    {
        return false;
    }

    Node->SetTargetRatio(bTargetInLT ? 1.0f : 0.0f, bAnimate);
    if (SSplitter *Child = SSplitter::AsSplitter(bTargetInLT ? Node->GetSideLT() : Node->GetSideRB()))
    {
        ConfigureCollapseToSlot(Child, TargetWindow, bAnimate);
    }

    return true;
}

void FLevelViewportLayout::BeginSplitToOnePaneTransition(int32 SlotIndex)
{
    FinishLayoutTransition(true);

    if (!RootSplitter || SlotIndex < 0 || SlotIndex >= static_cast<int32>(LevelViewportClients.size()) || SlotIndex >= MaxViewportSlots ||
        !ViewportWindows[SlotIndex])
    {
        MaximizedOriginalSlotIndex = 0;
        bSuppressLayoutTransitionAnimation = true;
        SetLayout(EViewportLayout::OnePane);
        bSuppressLayoutTransitionAnimation = false;
        return;
    }

    LastSplitLayout = CurrentLayout;
    MaximizedOriginalSlotIndex = SlotIndex;
    TransitionSourceSlot = SlotIndex;
    TransitionTargetLayout = EViewportLayout::OnePane;
    TransitionRestoreRatioCount = 0;
    SetActiveViewport(LevelViewportClients[SlotIndex]);

    TArray<SSplitter *> Splitters;
    SSplitter::CollectSplitters(RootSplitter, Splitters);
    TransitionRestoreRatioCount = (std::min)(static_cast<int32>(Splitters.size()), 3);
    for (int32 i = 0; i < TransitionRestoreRatioCount; ++i)
    {
        TransitionRestoreRatios[i] = Splitters[i]->GetRatio();
    }

    LayoutTransition = EViewportLayoutTransition::SplitToOnePane;
    DraggingSplitter = nullptr;
    if (!ConfigureCollapseToSlot(RootSplitter, ViewportWindows[SlotIndex], true))
    {
        FinishLayoutTransition(true);
    }
}

void FLevelViewportLayout::BeginOnePaneToSplitTransition(EViewportLayout TargetLayout)
{
    FinishLayoutTransition(true);
    if (TargetLayout == EViewportLayout::OnePane)
    {
        return;
    }

    TransitionTargetLayout = TargetLayout;
    const int32 TargetSlotCount = GetSlotCount(TargetLayout);
    const int32 ExpandSourceSlot =
        (MaximizedOriginalSlotIndex >= 0 && MaximizedOriginalSlotIndex < TargetSlotCount) ? MaximizedOriginalSlotIndex : 0;
    TransitionSourceSlot = ExpandSourceSlot;

    bSuppressLayoutTransitionAnimation = true;
    SetLayout(TargetLayout);
    bSuppressLayoutTransitionAnimation = false;

    if (!RootSplitter || !ViewportWindows[ExpandSourceSlot])
    {
        return;
    }

    TArray<SSplitter *> Splitters;
    SSplitter::CollectSplitters(RootSplitter, Splitters);
    const int32 RestoreCount = (std::min)(static_cast<int32>(Splitters.size()), 3);
    float       TargetRatios[3] = {0.5f, 0.5f, 0.5f};
    for (int32 i = 0; i < RestoreCount; ++i)
    {
        TargetRatios[i] = (i < TransitionRestoreRatioCount) ? TransitionRestoreRatios[i] : Splitters[i]->GetRatio();
    }

    ConfigureCollapseToSlot(RootSplitter, ViewportWindows[ExpandSourceSlot], false);
    for (int32 i = 0; i < RestoreCount; ++i)
    {
        Splitters[i]->SetTargetRatio(TargetRatios[i], true);
    }

    LayoutTransition = EViewportLayoutTransition::OnePaneToSplit;
    DraggingSplitter = nullptr;
}

void FLevelViewportLayout::FinishLayoutTransition(bool bSnapToEnd)
{
    if (LayoutTransition == EViewportLayoutTransition::None)
    {
        return;
    }

    const EViewportLayoutTransition FinishedTransition = LayoutTransition;
    LayoutTransition = EViewportLayoutTransition::None;
    DraggingSplitter = nullptr;

    if (RootSplitter)
    {
        TArray<SSplitter *> Splitters;
        SSplitter::CollectSplitters(RootSplitter, Splitters);
        for (SSplitter *Splitter : Splitters)
        {
            if (Splitter)
            {
                Splitter->StopAnimation(bSnapToEnd);
            }
        }
    }

    if (FinishedTransition == EViewportLayoutTransition::SplitToOnePane)
    {
        bSuppressLayoutTransitionAnimation = true;
        SetLayout(EViewportLayout::OnePane);
        bSuppressLayoutTransitionAnimation = false;
    }
}

bool FLevelViewportLayout::UpdateLayoutTransition(float DeltaTime)
{
    if (LayoutTransition == EViewportLayoutTransition::None || !RootSplitter)
    {
        return false;
    }

    bool                bAnyAnimating = false;
    TArray<SSplitter *> Splitters;
    SSplitter::CollectSplitters(RootSplitter, Splitters);
    for (SSplitter *Splitter : Splitters)
    {
        if (Splitter && Splitter->UpdateAnimation(DeltaTime))
        {
            bAnyAnimating = true;
        }
    }

    if (!bAnyAnimating)
    {
        FinishLayoutTransition(false);
        return false;
    }

    return true;
}

// ─── 레이아웃 전환 ──────────────────────────────────────────

void FLevelViewportLayout::SetLayout(EViewportLayout NewLayout)
{
    if (NewLayout == CurrentLayout)
        return;

    if (!bSuppressLayoutTransitionAnimation)
    {
        if (LayoutTransition != EViewportLayoutTransition::None)
        {
            FinishLayoutTransition(true);
            if (NewLayout == CurrentLayout)
            {
                return;
            }
        }

        if (CurrentLayout != EViewportLayout::OnePane && NewLayout == EViewportLayout::OnePane)
        {
            BeginSplitToOnePaneTransition(GetActiveViewportSlotIndex());
            return;
        }

        if (CurrentLayout == EViewportLayout::OnePane && NewLayout != EViewportLayout::OnePane)
        {
            BeginOnePaneToSplitTransition(NewLayout);
            return;
        }
    }

    const bool bLeavingOnePane = (CurrentLayout == EViewportLayout::OnePane && NewLayout != EViewportLayout::OnePane);
    const bool bEnteringOnePane = (CurrentLayout != EViewportLayout::OnePane && NewLayout == EViewportLayout::OnePane);

    // 기존 트리 해제
    SSplitter::DestroyTree(RootSplitter);
    RootSplitter = nullptr;
    DraggingSplitter = nullptr;

    int32 RequiredSlots = GetSlotCount(NewLayout);
    int32 OldSlotCount = static_cast<int32>(LevelViewportClients.size());

    // 슬롯 수 조정
    if (RequiredSlots > OldSlotCount)
        EnsureViewportSlots(RequiredSlots);
    else if (RequiredSlots < OldSlotCount && NewLayout != EViewportLayout::OnePane)
        ShrinkViewportSlots(RequiredSlots);

    if (bEnteringOnePane)
    {
        if (MaximizedOriginalSlotIndex < 0 || MaximizedOriginalSlotIndex >= static_cast<int32>(LevelViewportClients.size()) ||
            MaximizedOriginalSlotIndex >= MaxViewportSlots)
        {
            MaximizedOriginalSlotIndex = 0;
        }
        SwapViewportSlots(0, MaximizedOriginalSlotIndex);
    }
    else if (bLeavingOnePane)
    {
        RestoreMaximizedViewportToOriginalSlot();
    }

    // 분할 전환 시 새로 추가된 슬롯은 Top, Front, Right를 기본값으로 설정
    if (NewLayout != EViewportLayout::OnePane)
    {
        constexpr ELevelViewportType DefaultTypes[] = {ELevelViewportType::Top, ELevelViewportType::Front, ELevelViewportType::Right};
        // 기존 슬롯은 유지하고, 새로 늘어난 슬롯에만 적용
        int32 StartIdx = OldSlotCount;
        for (int32 i = StartIdx; i < RequiredSlots && (i - 1) < 3; ++i)
        {
            LevelViewportClients[i]->SetViewportType(DefaultTypes[i - 1]);
        }
    }

    // 새 트리 빌드
    RootSplitter = BuildSplitterTree(NewLayout);
    ActiveSlotCount = RequiredSlots;
    CurrentLayout = NewLayout;
    if (CurrentLayout != EViewportLayout::OnePane)
    {
        LastSplitLayout = CurrentLayout;
    }
}

void FLevelViewportLayout::ToggleViewportSplit(int32 SourceSlotIndex)
{
    if (LayoutTransition != EViewportLayoutTransition::None)
    {
        return;
    }
    if (CurrentLayout == EViewportLayout::OnePane)
    {
        const EViewportLayout TargetLayout =
            (LastSplitLayout != EViewportLayout::OnePane) ? LastSplitLayout : EViewportLayout::FourPanes2x2;
        SetLayout(TargetLayout);
    }
    else
    {
        const int32 SlotIndex = (SourceSlotIndex >= 0 && SourceSlotIndex < static_cast<int32>(LevelViewportClients.size()) &&
                                 SourceSlotIndex < MaxViewportSlots)
                                    ? SourceSlotIndex
                                    : GetActiveViewportSlotIndex();
        SetActiveViewport(LevelViewportClients[SlotIndex]);
        SetLayout(EViewportLayout::OnePane);
    }
}

// ─── Viewport UI 렌더링 ─────────────────────────────────────

void FLevelViewportLayout::RenderViewportUI(float DeltaTime)
{
    bMouseOverViewport = false;

    if (!Editor || !Editor->IsLevelEditorContextActive())
    {
        for (FLevelEditorViewportClient* VC : LevelViewportClients)
        {
            if (VC)
            {
                VC->SetHovered(false);
            }
        }
        return;
    }

    UpdateLayoutTransition(DeltaTime);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    constexpr const char *PanelIconKey = "Editor.Icon.Panel.Viewport";
    FPanelDesc PanelDesc;
    PanelDesc.DisplayName = "Viewport";
    PanelDesc.StableId = "LevelViewport";
    PanelDesc.IconKey = PanelIconKey;
    PanelDesc.WindowFlags = ImGuiWindowFlags_None;
    PanelDesc.bClosable = true;
    PanelDesc.bOpen = &FLevelEditorSettings::Get().Panels.bViewport;
    PanelDesc.bApplySideInset = false;
    PanelDesc.bApplyBottomInset = false;
    const bool bIsOpen = FPanel::Begin(PanelDesc);
    if (!bIsOpen)
    {
        FPanel::End();
        ImGui::PopStyleVar();
        return;
    }

    ImVec2 ContentPos = ImGui::GetCursorScreenPos();
    ImVec2 ContentSize = ImGui::GetContentRegionAvail();

    if (ImGui::GetDragDropPayload())
    {
        ImGui::SetCursorScreenPos(ContentPos);
        ImGui::Selectable("##ViewportArea", false, 0, ContentSize);
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ObjectContentItem"))
            {
                FContentItem ContentItem = *reinterpret_cast<const FContentItem *>(payload->Data);

                Editor->BeginTrackedSceneChange();
                AStaticMeshActor *NewActor =
                    Cast<AStaticMeshActor>(FObjectFactory::Get().Create(AStaticMeshActor::StaticClass()->GetName(), Editor->GetWorld()));
                NewActor->InitDefaultComponents(FPaths::ToUtf8(ContentItem.Path));
                Editor->GetWorld()->AddActor(NewActor);
                Editor->CommitTrackedSceneChange();
            }
            ImGui::EndDragDropTarget();
        }
    }

    if (ContentSize.x > 0 && ContentSize.y > 0)
    {
        // Play/Stop/Undo/Redo frame toolbar는 document tab bar 아래의 Level Editor frame 영역으로 이동했다.
        // 이 viewport panel은 이제 순수 viewport layout과 pane toolbar만 담당한다.
        for (FLevelEditorViewportClient *VC : LevelViewportClients)
        {
            if (VC)
            {
                ApplyProjectViewportSettings(VC->GetRenderOptions());
            }
        }

        FRect ContentRect = {ContentPos.x, ContentPos.y, ContentSize.x, ContentSize.y};
        auto  IsSlotVisibleEnough = [&](int32 SlotIndex) -> bool
        {
            if (SlotIndex < 0 || SlotIndex >= MaxViewportSlots || !ViewportWindows[SlotIndex])
            {
                return false;
            }
            const FRect &R = ViewportWindows[SlotIndex]->GetRect();
            return R.Width > 1.0f && R.Height > 1.0f;
        };

        // SSplitter 레이아웃 계산
        if (RootSplitter)
        {
            RootSplitter->ComputeLayout(ContentRect);
        }
        else if (ViewportWindows[0])
        {
            ViewportWindows[0]->SetRect(ContentRect);
        }

        // 각 ViewportClient에 Rect 반영 + 이미지 렌더
        for (int32 i = 0; i < ActiveSlotCount; ++i)
        {
            if (i < static_cast<int32>(LevelViewportClients.size()) && IsSlotVisibleEnough(i))
            {
                ViewportToolbarRects[i] = ViewportWindows[i]->GetRect();
                FRect       RenderRect = ViewportToolbarRects[i];
                const float ViewportToolbarHeight = GetViewportPaneToolbarHeight(RenderRect.Width);
                RenderRect.Y += ViewportToolbarHeight;
                RenderRect.Height = (std::max)(0.0f, RenderRect.Height - ViewportToolbarHeight);
                ViewportWindows[i]->SetRect(RenderRect);
                FLevelEditorViewportClient *VC = LevelViewportClients[i];
                VC->UpdateLayoutRect();
                VC->RenderViewportImage(VC == ActiveViewportClient);
            }
        }

        // 각 뷰포트 패인 상단에 툴바 렌더
        for (int32 i = 0; i < ActiveSlotCount; ++i)
        {
            const bool bShowPaneToolbar =
                IsSlotVisibleEnough(i) && (LayoutTransition == EViewportLayoutTransition::None || i == TransitionSourceSlot);
            if (bShowPaneToolbar)
            {
                RenderViewportToolbar(i);
            }
        }

        // 분할 바 렌더 (구분선 표시)
        if (RootSplitter)
        {
            TArray<SSplitter *> AllSplitters;
            SSplitter::CollectSplitters(RootSplitter, AllSplitters);

            ImDrawList *DrawList = ImGui::GetWindowDrawList();
            ImU32       BarColor = IM_COL32(0, 0, 0, 255);

            for (SSplitter *S : AllSplitters)
            {
                const FRect &Bar = S->GetSplitBarRect();
                DrawList->AddRectFilled(ImVec2(Bar.X, Bar.Y), ImVec2(Bar.X + Bar.Width, Bar.Y + Bar.Height), BarColor);
            }
        }

        // 입력 처리
        if (ImGui::IsWindowHovered())
        {
            ImVec2 MousePos = ImGui::GetIO().MousePos;
            FPoint MP = {MousePos.x, MousePos.y};

            for (int32 i = 0; i < ActiveSlotCount; ++i)
            {
                bool bSlotHovered = IsSlotVisibleEnough(i) && ViewportWindows[i]->IsHover(MP);
                if (i < static_cast<int32>(LevelViewportClients.size()))
                {
                    LevelViewportClients[i]->SetHovered(bSlotHovered);
                }

                if (bSlotHovered)
                {
                    bMouseOverViewport = true;
                }
            }

            const bool bLockViewportResolution = FProjectSettings::Get().Game.bLockWindowResolution;

            // 분할 바 드래그
            if (RootSplitter && LayoutTransition == EViewportLayoutTransition::None && !bLockViewportResolution)
            {
                if (ImGui::IsMouseClicked(0))
                {
                    DraggingSplitter = SSplitter::FindSplitterAtBar(RootSplitter, MP);
                }

                if (ImGui::IsMouseReleased(0))
                {
                    DraggingSplitter = nullptr;
                }

                if (DraggingSplitter)
                {
                    const FRect &DR = DraggingSplitter->GetRect();
                    if (DraggingSplitter->GetOrientation() == ESplitOrientation::Horizontal)
                    {
                        float NewRatio = (MousePos.x - DR.X) / DR.Width;
                        DraggingSplitter->SetRatio(Clamp(NewRatio, 0.15f, 0.85f));
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                    }
                    else
                    {
                        float NewRatio = (MousePos.y - DR.Y) / DR.Height;
                        DraggingSplitter->SetRatio(Clamp(NewRatio, 0.15f, 0.85f));
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    }
                }
                else
                {
                    // 호버 커서 변경
                    SSplitter *Hovered = SSplitter::FindSplitterAtBar(RootSplitter, MP);
                    if (Hovered)
                    {
                        if (Hovered->GetOrientation() == ESplitOrientation::Horizontal)
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        else
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    }
                }
            }
            else if (bLockViewportResolution)
            {
                DraggingSplitter = nullptr;
            }

            // 활성 뷰포트 전환 (분할 바 드래그 중이 아닐 때)
            if (!DraggingSplitter && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
            {
                for (int32 i = 0; i < ActiveSlotCount; ++i)
                {
                    if (i < static_cast<int32>(LevelViewportClients.size()) && IsSlotVisibleEnough(i) && ViewportWindows[i]->IsHover(MP))
                    {
                        if (LevelViewportClients[i] != ActiveViewportClient)
                            SetActiveViewport(LevelViewportClients[i]);
                        break;
                    }
                }
            }

            HandleViewportContextMenuInput(MP);
        }
    }

    RenderViewportPlaceActorPopup();

    FPanel::End();
    ImGui::PopStyleVar();
}

// ─── 각 뷰포트 패인 툴바 ──────────────────────────

void FLevelViewportLayout::RenderMainToolbar(float ToolbarLeft, float ToolbarTop)
{
    (void)ToolbarLeft;
    (void)ToolbarTop;
    return;

    if (!Editor)
    {
        return;
    }

        EnsureToolbarIconsLoaded(RendererPtr);

    constexpr float ToolbarHeight = 40.0f;
    constexpr float IconSize = 18.0f;
    constexpr float ButtonPadding = (ToolbarHeight - IconSize) * 0.5f;
    constexpr float ButtonSpacing = 6.0f;
    constexpr float PlayStopButtonWidth = 24.0f;
    constexpr float GroupSpacing = 14.0f;
    constexpr float ToolbarFallbackIconSize = 16.0f;
    constexpr float ToolbarMaxIconSize = 18.0f;

    ImGui::SetCursorScreenPos(
        ImVec2(ToolbarLeft + ButtonPadding + (PlayStopButtonWidth * 2.0f) + ButtonSpacing + GroupSpacing, ToolbarTop));

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.11f, 0.92f));
    ImGui::BeginChild("##MainToolbarBar", ImVec2(540.0f, ToolbarHeight), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPos(ImVec2(10.0f, ButtonPadding));
    PushToolbarButtonStyle();

    auto DrawGizmoIcon = [&](const char *Id, EToolbarIcon Icon, EGizmoMode TargetMode, const char *FallbackLabel) -> bool
    {
        const EGizmoMode CurrentMode = ActiveViewportClient ? ActiveViewportClient->GetGizmoManager().GetMode() : EGizmoMode::Translate;
        const bool bSelected = (CurrentMode == TargetMode);
        if (bSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, UIAccentColor::Value);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UIAccentColor::Value);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIAccentColor::Value);
        }
        const bool bClicked = DrawToolbarIconButton(Id, Icon, FallbackLabel, ToolbarFallbackIconSize, ToolbarMaxIconSize);
        if (bSelected)
        {
            ImGui::PopStyleColor(3);
        }
        return bClicked;
    };

    // 상단 툴바에서도 Place Actor 컨텍스트 메뉴를 바로 열 수 있게 한다.
    if (DrawToolbarIconButton("##SharedAddActorIcon", EToolbarIcon::AddActor, "Add", ToolbarFallbackIconSize, ToolbarMaxIconSize))
    {
        const FPoint MousePos = {ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y};
        ContextMenuState.PendingPopupPos = MousePos;
        ContextMenuState.PendingPopupSlot = 0;
        ContextMenuState.PendingSpawnSlot = 0;
        ContextMenuState.PendingSpawnPos = MousePos;
        for (int32 i = 0; i < ActiveSlotCount; ++i)
        {
            if (LevelViewportClients[i] == ActiveViewportClient)
            {
                ContextMenuState.PendingPopupSlot = i;
                ContextMenuState.PendingSpawnSlot = i;
                if (ViewportWindows[i])
                {
                    const FRect &ViewRect = ViewportWindows[i]->GetRect();
                    ContextMenuState.PendingSpawnPos = {ViewRect.X + ViewRect.Width * 0.5f, ViewRect.Y + ViewRect.Height * 0.5f};
                }
                break;
            }
        }
    }
    ShowItemTooltip("Place Actor");

    ImGui::SameLine(0.0f, GroupSpacing);
    if (DrawGizmoIcon("##SharedTranslateToolIcon", EToolbarIcon::Translate, EGizmoMode::Translate, "Translate"))
    {
        if (ActiveViewportClient)
        {
            Editor->SetEditorGizmoMode(EGizmoMode::Translate);
        }
    }
    ShowItemTooltip("Translate");
    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawGizmoIcon("##SharedRotateToolIcon", EToolbarIcon::Rotate, EGizmoMode::Rotate, "Rotate"))
    {
        if (ActiveViewportClient)
        {
            Editor->SetEditorGizmoMode(EGizmoMode::Rotate);
        }
    }
    ShowItemTooltip("Rotate");
    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawGizmoIcon("##SharedScaleToolIcon", EToolbarIcon::Scale, EGizmoMode::Scale, "Scale"))
    {
        if (ActiveViewportClient)
        {
            Editor->SetEditorGizmoMode(EGizmoMode::Scale);
        }
    }
    ShowItemTooltip("Scale");

    FLevelEditorSettings &Settings = Editor->GetSettings();

    ImGui::SameLine(0.0f, GroupSpacing);
    const bool bWorldCoord = Settings.CoordSystem == EEditorCoordSystem::World;
    if (DrawToolbarIconButton("##SharedCoordSystemIcon", bWorldCoord ? EToolbarIcon::WorldSpace : EToolbarIcon::LocalSpace,
                              bWorldCoord ? "World" : "Local", ToolbarFallbackIconSize, ToolbarMaxIconSize))
    {
        Editor->ToggleCoordSystem();
    }
    ShowItemTooltip(bWorldCoord ? "World Space" : "Local Space");

    // 스냅 토글과 수치 변경을 같은 줄에서 처리하고 즉시 Gizmo 설정에 반영합니다.
    auto DrawSnapControl = [&](const char *Id, EToolbarIcon Icon, const char *FallbackLabel, bool &bEnabled, float &Value, float MinValue)
    {
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::PushID(Id);
        const bool bWasEnabled = bEnabled;
        if (bWasEnabled)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, UIAccentColor::Value);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UIAccentColor::Value);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIAccentColor::Value);
        }
        if (DrawToolbarIconButton("##SnapToggle", Icon, FallbackLabel, ToolbarFallbackIconSize, ToolbarMaxIconSize))
        {
            bEnabled = !bEnabled;
        }
        ShowItemTooltip(FallbackLabel);
        if (bWasEnabled)
        {
            ImGui::PopStyleColor(3);
        }
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::SetNextItemWidth(48.0f);
        if (ImGui::InputFloat("##Value", &Value, 0.0f, 0.0f, "%.2f") && Value < MinValue)
        {
            Value = MinValue;
        }
        ImGui::PopID();
    };

    DrawSnapControl("TranslateSnap", EToolbarIcon::TranslateSnap, "Translation Snap", Settings.bEnableTranslationSnap,
                    Settings.TranslationSnapSize, 0.001f);
    DrawSnapControl("RotateSnap", EToolbarIcon::RotateSnap, "Rotation Snap", Settings.bEnableRotationSnap, Settings.RotationSnapSize,
                    0.001f);
    DrawSnapControl("ScaleSnap", EToolbarIcon::ScaleSnap, "Scale Snap", Settings.bEnableScaleSnap, Settings.ScaleSnapSize, 0.001f);

    Editor->ApplyTransformSettingsToGizmo();
    PopToolbarButtonStyle();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void FLevelViewportLayout::RenderViewportToolbar(int32 SlotIndex)
{
    if (SlotIndex >= MaxViewportSlots || !ViewportWindows[SlotIndex])
    {
        return;
    }

    FRect PaneRect = ViewportToolbarRects[SlotIndex];
    if (PaneRect.Width <= 0.0f || PaneRect.Height <= 0.0f)
    {
        PaneRect = ViewportWindows[SlotIndex]->GetRect();
    }
    if (PaneRect.Width <= 0.0f || PaneRect.Height <= 0.0f)
    {
        return;
    }

    const float PaneToolbarHeight = GetViewportPaneToolbarHeight(PaneRect.Width);

    char ToolbarID[64];
    snprintf(ToolbarID, sizeof(ToolbarID), "##PaneToolbar_%d", SlotIndex);

    if (!FViewportToolbar::BeginInRect(ToolbarID, PaneRect, PaneToolbarHeight))
    {
        return;
    }

    ImGui::PushID(SlotIndex);

    if (SlotIndex < static_cast<int32>(LevelViewportClients.size()))
    {
        FLevelEditorViewportClient *VC = LevelViewportClients[SlotIndex];
        FViewportRenderOptions &Opts = VC->GetRenderOptions();
        FEditorViewportCamera *Camera = VC->GetCamera();
        FLevelEditorSettings &Settings = Editor->GetSettings();

        FEditorViewportToolbar::FDesc Desc;
        Desc.IdPrefix = "LevelViewportToolbar";
        Desc.ToolbarWidth = PaneRect.Width;
        Desc.Renderer = RendererPtr;
        Desc.bEnabled = Editor != nullptr && VC != nullptr;
        // 각 pane toolbar는 자기 ViewportClient의 gizmo state를 표시한다.
        // ActiveViewportClient를 읽으면 다른 pane의 toolbar 렌더 순서에 따라 선택 표시가 꼬일 수 있다.
        const EGizmoMode CurrentGizmoMode = VC ? VC->GetGizmoManager().GetMode() : EGizmoMode::Translate;
        Desc.ToolMode = CurrentGizmoMode == EGizmoMode::Rotate
                            ? FEditorViewportToolbar::EToolMode::Rotate
                            : (CurrentGizmoMode == EGizmoMode::Scale
                                   ? FEditorViewportToolbar::EToolMode::Scale
                                   : FEditorViewportToolbar::EToolMode::Translate);
        Desc.CoordSpace = Settings.CoordSystem == EEditorCoordSystem::World ? FEditorViewportToolbar::ECoordSpace::World
                                                                            : FEditorViewportToolbar::ECoordSpace::Local;
        Desc.bSnapEnabled = Settings.bEnableTranslationSnap || Settings.bEnableRotationSnap || Settings.bEnableScaleSnap;
        Desc.ViewportTypeIcon = FEditorViewportToolbar::GetViewportTypeIconByIndex(static_cast<int32>(Opts.ViewportType));
        Desc.ViewportTypeLabel = FEditorViewportToolbar::GetViewportTypeLabelByIndex(static_cast<int32>(Opts.ViewportType));
        Desc.ViewModeIcon = FEditorViewportToolbar::GetViewModeIconByIndex(static_cast<int32>(Opts.ViewMode));
        Desc.ViewModeLabel = FEditorViewportToolbar::GetViewModeLabelByIndex(static_cast<int32>(Opts.ViewMode));

        Desc.OnToolModeChanged = [this, VC, &Settings](FEditorViewportToolbar::EToolMode Mode)
        {
            if (!VC)
            {
                return;
            }

            // 툴바를 누른 pane을 먼저 active viewport로 만든 뒤, 그 viewport의 manager만 갱신한다.
            // 이전 코드처럼 ActiveViewportClient를 직접 쓰면 비활성 pane toolbar 클릭 시 다른 viewport의
            // manager가 바뀌거나, 다음 프레임에 mode 표시가 되돌아가는 문제가 생긴다.
            SetActiveViewport(VC);

            EGizmoMode NewMode = EGizmoMode::Translate;
            switch (Mode)
            {
            case FEditorViewportToolbar::EToolMode::Rotate:
                NewMode = EGizmoMode::Rotate;
                break;
            case FEditorViewportToolbar::EToolMode::Scale:
                NewMode = EGizmoMode::Scale;
                break;
            case FEditorViewportToolbar::EToolMode::Translate:
            default:
                NewMode = EGizmoMode::Translate;
                break;
            }

            Editor->SetEditorGizmoMode(NewMode);
        };

        Desc.OnCoordSpaceToggled = [&]()
        {
            Editor->ToggleCoordSystem();
        };

        Desc.DrawSnapPopup = [&]()
        {
            FEditorViewportToolbar::DrawCommonSnapPopup(Settings.bEnableTranslationSnap, Settings.TranslationSnapSize,
                                                       Settings.bEnableRotationSnap, Settings.RotationSnapSize,
                                                       Settings.bEnableScaleSnap, Settings.ScaleSnapSize);
        };

        Desc.DrawCameraPopup = [&]()
        {
            DrawCameraPopupContent(Camera, Settings);
        };

        Desc.DrawViewportTypePopup = [&]()
        {
            auto DrawViewportTypeOption = [&](const char *Label, ELevelViewportType Type)
            {
                const int32 TypeIndex = static_cast<int32>(Type);
                if (FEditorViewportToolbar::DrawIconSelectable(Label, FEditorViewportToolbar::GetViewportTypeIconByIndex(TypeIndex),
                                                               Label, Opts.ViewportType == Type, 220.0f))
                {
                    VC->SetViewportType(Type);
                    ImGui::CloseCurrentPopup();
                }
            };

            FEditorViewportToolbar::DrawPopupSectionHeader("PERSPECTIVE");
            DrawViewportTypeOption("Perspective", ELevelViewportType::Perspective);
            FEditorViewportToolbar::DrawPopupSectionHeader("ORTHOGRAPHIC");
            DrawViewportTypeOption("Top", ELevelViewportType::Top);
            DrawViewportTypeOption("Bottom", ELevelViewportType::Bottom);
            DrawViewportTypeOption("Left", ELevelViewportType::Left);
            DrawViewportTypeOption("Right", ELevelViewportType::Right);
            DrawViewportTypeOption("Front", ELevelViewportType::Front);
            DrawViewportTypeOption("Back", ELevelViewportType::Back);
            DrawViewportTypeOption("Free Ortho", ELevelViewportType::FreeOrthographic);
        };

        Desc.DrawViewModePopup = [&]()
        {
            auto DrawViewModeOption = [&](const char *Label, EViewMode Mode)
            {
                const int32 ModeIndex = static_cast<int32>(Mode);
                if (FEditorViewportToolbar::DrawIconSelectable(Label, FEditorViewportToolbar::GetViewModeIconByIndex(ModeIndex),
                                                               Label, Opts.ViewMode == Mode, 260.0f))
                {
                    Opts.ViewMode = Mode;
                    ImGui::CloseCurrentPopup();
                }
            };

            FEditorViewportToolbar::DrawPopupSectionHeader("VIEW MODE");
            DrawViewModeOption("Lit", EViewMode::Lit_Phong);
            DrawViewModeOption("Unlit", EViewMode::Unlit);
            DrawViewModeOption("Wireframe", EViewMode::Wireframe);
            DrawViewModeOption("Lit Gouraud", EViewMode::Lit_Gouraud);
            DrawViewModeOption("Lit Lambert", EViewMode::Lit_Lambert);
            DrawViewModeOption("Scene Depth", EViewMode::SceneDepth);
            DrawViewModeOption("World Normal", EViewMode::WorldNormal);
            DrawViewModeOption("Light Culling", EViewMode::LightCulling);
        };

        Desc.DrawShowPopup = [&]()
        {
            DrawShowFlagsPopupContent(Opts);
        };

        Desc.DrawLayoutPopup = [&]()
        {
            constexpr int32 LayoutCount = static_cast<int32>(EViewportLayout::MAX);
            constexpr int32 Columns = 4;
            constexpr float IconSize = 32.0f;

            FEditorViewportToolbar::DrawPopupSectionHeader("VIEWPORT LAYOUT");
            for (int32 i = 0; i < LayoutCount; ++i)
            {
                ImGui::PushID(i);
                const bool bSelected = (static_cast<EViewportLayout>(i) == CurrentLayout);
                if (bSelected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, UIAccentColor::Value);
                }

                bool bClicked = false;
                if (LayoutIcons[i])
                {
                    bClicked = ImGui::ImageButton("##icon", reinterpret_cast<ImTextureID>(LayoutIcons[i]), ImVec2(IconSize, IconSize));
                }
                else
                {
                    char Label[4];
                    snprintf(Label, sizeof(Label), "%d", i);
                    bClicked = ImGui::Button(Label, ImVec2(IconSize + 8, IconSize + 8));
                }
                ShowItemTooltip(GetLayoutDisplayName(static_cast<EViewportLayout>(i)));

                if (bSelected)
                {
                    ImGui::PopStyleColor();
                }

                if (bClicked)
                {
                    SetLayout(static_cast<EViewportLayout>(i));
                    ImGui::CloseCurrentPopup();
                }

                if ((i + 1) % Columns != 0 && i + 1 < LayoutCount)
                {
                    ImGui::SameLine();
                }

                ImGui::PopID();
            }

            FEditorViewportToolbar::DrawPopupSeparator(4.0f, 6.0f);
            const bool bIsTransitioning = (LayoutTransition != EViewportLayoutTransition::None);
            ImGui::BeginDisabled(bIsTransitioning);
            if (ImGui::MenuItem((CurrentLayout == EViewportLayout::OnePane) ? "Split Viewports" : "Merge Viewports"))
            {
                ToggleViewportSplit(SlotIndex);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
        };

        FEditorViewportToolbar::RenderViewportToolbar(Desc);
        if (Editor && VC == ActiveViewportClient)
        {
            Editor->ApplyTransformSettingsToGizmo();
        }
    }

    ImGui::PopID();
    FViewportToolbar::EndInRect();
}

void FLevelViewportLayout::HandleViewportContextMenuInput(const FPoint &MousePos)
{
    if (LayoutTransition != EViewportLayoutTransition::None)
    {
        return;
    }

    constexpr float RightClickPopupThresholdSq = 16.0f;
    auto            IsSlotVisibleEnough = [&](int32 SlotIndex) -> bool
    {
        if (SlotIndex < 0 || SlotIndex >= MaxViewportSlots || !ViewportWindows[SlotIndex])
        {
            return false;
        }
        const FRect &R = ViewportWindows[SlotIndex]->GetRect();
        return R.Width > 1.0f && R.Height > 1.0f;
    };

    for (int32 i = 0; i < ActiveSlotCount; ++i)
    {
        if (!IsSlotVisibleEnough(i))
        {
            continue;
        }

        if (ImGui::IsMouseClicked(1) && ViewportWindows[i]->IsHover(MousePos))
        {
            ContextMenuState.bTrackingRightClick[i] = true;
            ContextMenuState.RightClickTravelSq[i] = 0.0f;
            ContextMenuState.RightClickPressPos[i] = MousePos;
        }

        if (!ContextMenuState.bTrackingRightClick[i])
        {
            continue;
        }

        const float DX = MousePos.X - ContextMenuState.RightClickPressPos[i].X;
        const float DY = MousePos.Y - ContextMenuState.RightClickPressPos[i].Y;
        const float TravelSq = DX * DX + DY * DY;
        if (TravelSq > ContextMenuState.RightClickTravelSq[i])
        {
            ContextMenuState.RightClickTravelSq[i] = TravelSq;
        }
    }

    if (!ImGui::IsMouseReleased(1))
    {
        return;
    }

    for (int32 i = 0; i < ActiveSlotCount; ++i)
    {
        if (!IsSlotVisibleEnough(i) || !ContextMenuState.bTrackingRightClick[i])
        {
            continue;
        }

        const bool bReleasedOverSameSlot = ViewportWindows[i]->IsHover(MousePos);
        const bool bClickCandidate = bReleasedOverSameSlot && ContextMenuState.RightClickTravelSq[i] <= RightClickPopupThresholdSq &&
                                     !FInputManager::Get().IsMouseButtonDown(FInputManager::MOUSE_RIGHT) &&
                                     !FInputManager::Get().IsMouseButtonReleased(FInputManager::MOUSE_RIGHT);
        const ImGuiIO &IO = ImGui::GetIO();
        const bool     bNoModifiers = !IO.KeyCtrl && !IO.KeyShift && !IO.KeyAlt && !IO.KeySuper;

        // 드래그가 거의 없는 우클릭은 popup 요청으로 처리합니다.
        if (bClickCandidate && bNoModifiers)
        {
            ContextMenuState.PendingPopupSlot = i;
            ContextMenuState.PendingSpawnSlot = i;
            ContextMenuState.PendingPopupPos = MousePos;
            ContextMenuState.PendingSpawnPos = ContextMenuState.RightClickPressPos[i];
        }

        ContextMenuState.bTrackingRightClick[i] = false;
        ContextMenuState.RightClickTravelSq[i] = 0.0f;
    }
}

void FLevelViewportLayout::RenderViewportPlaceActorPopup()
{
    constexpr const char *PopupId = "##ViewportPlaceActorPopup";

    if (ContextMenuState.PendingPopupSlot >= 0)
    {
        if (ContextMenuState.PendingPopupSlot < static_cast<int32>(LevelViewportClients.size()))
        {
            SetActiveViewport(LevelViewportClients[ContextMenuState.PendingPopupSlot]);
        }

        ImGui::SetNextWindowPos(ImVec2(ContextMenuState.PendingPopupPos.X, ContextMenuState.PendingPopupPos.Y));
        ImGui::OpenPopup(PopupId);
        ContextMenuState.PendingPopupSlot = -1;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    PushCommonPopupBgColor();
    if (!ImGui::BeginPopup(PopupId))
    {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        return;
    }

    if (ImGui::BeginMenu("Place Actor"))
    {
        // 기존 Control Panel의 spawn 기능을 뷰포트 기준 배치 메뉴로 옮긴다.
        const FPoint SpawnPos = ContextMenuState.PendingSpawnPos;
        const int32  SpawnSlot = ContextMenuState.PendingSpawnSlot;

        auto PlaceActorMenuItem = [&](const char *Label, EViewportPlaceActorType Type)
        {
            if (!ImGui::MenuItem(Label))
            {
                return;
            }

            FVector Location(0.0f, 0.0f, 0.0f);
            if (TryComputePlacementLocation(SpawnSlot, SpawnPos, Location))
            {
                SpawnActorFromViewportMenu(Type, Location);
            }
        };

        PlaceActorMenuItem("Cube", EViewportPlaceActorType::Cube);
        PlaceActorMenuItem("Actor", EViewportPlaceActorType::Actor);
        PlaceActorMenuItem("Pawn", EViewportPlaceActorType::Pawn);
        PlaceActorMenuItem("Runner", EViewportPlaceActorType::Runner);
        PlaceActorMenuItem("Character", EViewportPlaceActorType::Character);
        PlaceActorMenuItem("Static Mesh", EViewportPlaceActorType::StaticMeshActor);
        PlaceActorMenuItem("World Text", EViewportPlaceActorType::WorldText);
        PlaceActorMenuItem("Screen Text", EViewportPlaceActorType::ScreenText);
        PlaceActorMenuItem("UI Root", EViewportPlaceActorType::UIRoot);
        PlaceActorMenuItem("Sphere", EViewportPlaceActorType::Sphere);
        PlaceActorMenuItem("Cylinder", EViewportPlaceActorType::Cylinder);
        PlaceActorMenuItem("Cone", EViewportPlaceActorType::Cone);
        PlaceActorMenuItem("Plane", EViewportPlaceActorType::Plane);
        PlaceActorMenuItem("Decal", EViewportPlaceActorType::Decal);
        PlaceActorMenuItem("Height Fog", EViewportPlaceActorType::HeightFog);
        PlaceActorMenuItem("Ambient Light", EViewportPlaceActorType::AmbientLight);
        PlaceActorMenuItem("Directional Light", EViewportPlaceActorType::DirectionalLight);
        PlaceActorMenuItem("Point Light", EViewportPlaceActorType::PointLight);
        PlaceActorMenuItem("Spot Light", EViewportPlaceActorType::SpotLight);
        PlaceActorMenuItem("Map", EViewportPlaceActorType::MapManager);

        ImGui::EndMenu();
    }

    const bool bCanDelete = SelectionManager && !SelectionManager->IsEmpty();
    AActor    *PrimarySelection = SelectionManager ? SelectionManager->GetPrimarySelection() : nullptr;
    bool       bLockMovement = PrimarySelection ? PrimarySelection->IsActorMovementLocked() : false;
    const bool bCanFocus = PrimarySelection && ActiveViewportClient;

    if (!bCanFocus)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::MenuItem("Focus", "F"))
    {
        ActiveViewportClient->FocusActor(PrimarySelection);
        ImGui::CloseCurrentPopup();
    }
    if (!bCanFocus)
    {
        ImGui::EndDisabled();
    }

    if (!PrimarySelection)
    {
        ImGui::BeginDisabled();
    }
    ImGui::PushStyleColor(ImGuiCol_FrameBg, PopupPalette::CheckboxBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, PopupPalette::CheckboxHoverBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, PopupPalette::CheckboxActiveBg);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, PopupPalette::CheckboxCheck);
    if (ImGui::Checkbox("Lock Actor Movement", &bLockMovement))
    {
        if (PrimarySelection)
        {
            PrimarySelection->SetActorMovementLocked(bLockMovement);
        }
    }
    ImGui::PopStyleColor(4);
    if (!PrimarySelection)
    {
        ImGui::EndDisabled();
    }

    if (!bCanDelete)
    {
        ImGui::BeginDisabled();
    }
    // 삭제는 뷰포트 컨텍스트 동작이 정리되기 전까지 비활성화합니다.
    // if (ImGui::MenuItem("Delete"))
    //{
    //	SelectionManager->DeleteSelectedActors();
    //}
    if (!bCanDelete)
    {
        ImGui::EndDisabled();
    }

    ImGui::EndPopup();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

bool FLevelViewportLayout::TryComputePlacementLocation(int32 SlotIndex, const FPoint &ClientPos, FVector &OutLocation) const
{
    if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(LevelViewportClients.size()) || SlotIndex >= MaxViewportSlots ||
        !ViewportWindows[SlotIndex])
    {
        return false;
    }

    FLevelEditorViewportClient *ViewportClient = LevelViewportClients[SlotIndex];
    if (!ViewportClient || !ViewportClient->GetCamera())
    {
        return false;
    }

    const FRect &ViewRect = ViewportWindows[SlotIndex]->GetRect();
    const float  VPWidth = ViewportClient->GetViewport() ? static_cast<float>(ViewportClient->GetViewport()->GetWidth()) : ViewRect.Width;
    const float VPHeight = ViewportClient->GetViewport() ? static_cast<float>(ViewportClient->GetViewport()->GetHeight()) : ViewRect.Height;
    if (VPWidth <= 0.0f || VPHeight <= 0.0f)
    {
        return false;
    }

    const float LocalX = Clamp(ClientPos.X - ViewRect.X, 0.0f, VPWidth - 1.0f);
    const float LocalY = Clamp(ClientPos.Y - ViewRect.Y, 0.0f, VPHeight - 1.0f);
    // 클릭 좌표를 월드 레이로 바꿔 카메라 앞 기본 배치 위치를 계산합니다.
    const FRay    Ray = ViewportClient->GetCamera()->DeprojectScreenToWorld(LocalX, LocalY, VPWidth, VPHeight);
    const FVector RayDirection = Ray.Direction.Normalized();

    constexpr float SpawnDistanceFromCamera = 10.0f;
    OutLocation = Ray.Origin + Ray.Direction * SpawnDistanceFromCamera;

    if (Editor)
    {
        if (UWorld *World = Editor->GetWorld())
        {
            FRayHitResult HitResult{};
            AActor       *HitActor = nullptr;
            if (World->RaycastPrimitives(Ray, HitResult, HitActor))
            {
                OutLocation = Ray.Origin + RayDirection * HitResult.Distance;
            }
        }
    }

    return true;
}

AActor *FLevelViewportLayout::SpawnActorFromViewportMenu(EViewportPlaceActorType Type, const FVector &Location)
{
    if (!Editor)
    {
        return nullptr;
    }

    UWorld *World = Editor->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    AActor *SpawnedActor = nullptr;
    FVector SpawnLocation = Location;

    switch (Type)
    {
    case EViewportPlaceActorType::Actor:
    {
        SpawnedActor = World->SpawnActor<AActor>();
        break;
    }
    case EViewportPlaceActorType::Pawn:
    {
        APawnActor *Actor = World->SpawnActor<APawnActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
        }
        break;
    }
    case EViewportPlaceActorType::Runner:
    {
        ARunner *Actor = World->SpawnActor<ARunner>();
        if (Actor)
        {
            SpawnedActor = Actor;
        }
        break;
    }
    case EViewportPlaceActorType::Character:
    {
        ACharacterActor *Actor = World->SpawnActor<ACharacterActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
        }
        break;
    }
    case EViewportPlaceActorType::StaticMeshActor:
    {
        AStaticMeshActor *Actor = World->SpawnActor<AStaticMeshActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
        }
        break;
    }
    case EViewportPlaceActorType::SkeletalMeshActor:
    {
        ASkeletalMeshActor *Actor = World->SpawnActor<ASkeletalMeshActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
        }
        break;
    }
    case EViewportPlaceActorType::WorldText:
    {
        AWorldTextActor *Actor = World->SpawnActor<AWorldTextActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
            SpawnLocation.Z += 1.0f;
        }
        break;
    }
    case EViewportPlaceActorType::ScreenText:
    {
        AScreenTextActor *Actor = World->SpawnActor<AScreenTextActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
            SpawnLocation.Z += 1.0f;
        }
        break;
    }
    case EViewportPlaceActorType::UIRoot:
    {
        AUIRootActor *Actor = World->SpawnActor<AUIRootActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
            SpawnLocation.Z += 1.0f;
        }
        break;
    }
    case EViewportPlaceActorType::Cube:
    {
        AStaticMeshActor *Actor = World->SpawnActor<AStaticMeshActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents(GetRegisteredMeshPath("Default.Mesh.BasicShape.Cube"));
            SpawnedActor = Actor;
        }
        break;
    }
    case EViewportPlaceActorType::Sphere:
    {
        AStaticMeshActor *Actor = World->SpawnActor<AStaticMeshActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents(GetRegisteredMeshPath("Default.Mesh.BasicShape.Sphere"));
            SpawnedActor = Actor;
        }
        break;
    }
    case EViewportPlaceActorType::Cylinder:
    {
        AStaticMeshActor *Actor = World->SpawnActor<AStaticMeshActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents(GetRegisteredMeshPath("Default.Mesh.BasicShape.Cylinder"));
            SpawnedActor = Actor;
        }
        break;
    }
    case EViewportPlaceActorType::Cone:
    {
        AStaticMeshActor *Actor = World->SpawnActor<AStaticMeshActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents(GetRegisteredMeshPath("Default.Mesh.BasicShape.Cone"));
            SpawnedActor = Actor;
        }
        break;
    }
    case EViewportPlaceActorType::Plane:
    {
        AStaticMeshActor *Actor = World->SpawnActor<AStaticMeshActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents(GetRegisteredMeshPath("Default.Mesh.BasicShape.Plane"));
            SpawnedActor = Actor;
        }
        break;
    }
    case EViewportPlaceActorType::Decal:
    {
        ADecalActor *Actor = World->SpawnActor<ADecalActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
        }
        SpawnLocation.Z += 1.0f;
        break;
    }
    case EViewportPlaceActorType::HeightFog:
    {
        AHeightFogActor *Actor = World->SpawnActor<AHeightFogActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
            SpawnLocation.Z += 1.0f;
        }
        break;
    }
    case EViewportPlaceActorType::AmbientLight:
    {
        AAmbientLightActor *Actor = World->SpawnActor<AAmbientLightActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
            SpawnLocation.Z += 1.0f;
        }
        break;
    }
    case EViewportPlaceActorType::DirectionalLight:
    {
        ADirectionalLightActor *Actor = World->SpawnActor<ADirectionalLightActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            Actor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
            SpawnedActor = Actor;
            SpawnLocation.Z += 1.0f;
        }
        break;
    }
    case EViewportPlaceActorType::PointLight:
    {
        APointLightActor *Actor = World->SpawnActor<APointLightActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
            SpawnLocation.Z += 1.0f;
        }
        break;
    }
    case EViewportPlaceActorType::SpotLight:
    {
        ASpotLightActor *Actor = World->SpawnActor<ASpotLightActor>();
        if (Actor)
        {
            Actor->InitDefaultComponents();
            SpawnedActor = Actor;
            SpawnLocation.Z += 1.0f;
        }
        break;
    }
    case EViewportPlaceActorType::MapManager:
    {
        AMapManager *Actor = World->SpawnActor<AMapManager>();
        if (Actor)
        {
            SpawnedActor = Actor;
        }
        break;
    }
    default:
        break;
    }

    if (!SpawnedActor)
    {
        return nullptr;
    }

    SpawnedActor->EnsureEditorBillboardForActor();

    // 배치 직후 선택 상태와 공간 구조를 즉시 갱신합니다.
    SpawnedActor->SetActorLocation(SpawnLocation);
    World->InsertActorToOctree(SpawnedActor);
    if (SelectionManager)
    {
        SelectionManager->Select(SpawnedActor);
    }

    return SpawnedActor;
}

AActor *FLevelViewportLayout::SpawnPlaceActor(EViewportPlaceActorType Type, const FVector &Location)
{
    if (!Editor)
    {
        return SpawnActorFromViewportMenu(Type, Location);
    }

    Editor->BeginTrackedSceneChange();
    AActor *SpawnedActor = SpawnActorFromViewportMenu(Type, Location);
    if (SpawnedActor)
    {
        Editor->CommitTrackedSceneChange();
    }
    else
    {
        Editor->CancelTrackedSceneChange();
    }
    return SpawnedActor;
}

// ─── FLevelEditorSettings ↔ 뷰포트 상태 동기화 ──────────────────

void FLevelViewportLayout::SaveToSettings()
{
    FLevelEditorSettings &S = FLevelEditorSettings::Get();

    S.LayoutType = static_cast<int32>(CurrentLayout);

    // 뷰포트별 렌더 옵션 저장
    for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
    {
        S.SlotOptions[i] = LevelViewportClients[i]->GetRenderOptions();
    }

    // Splitter 비율 저장
    if (LayoutTransition != EViewportLayoutTransition::None && TransitionRestoreRatioCount > 0)
    {
        S.SplitterCount = TransitionRestoreRatioCount;
        if (S.SplitterCount > 3)
            S.SplitterCount = 3;
        for (int32 i = 0; i < S.SplitterCount; ++i)
        {
            S.SplitterRatios[i] = TransitionRestoreRatios[i];
        }
    }
    else if (RootSplitter)
    {
        TArray<SSplitter *> AllSplitters;
        SSplitter::CollectSplitters(RootSplitter, AllSplitters);
        S.SplitterCount = static_cast<int32>(AllSplitters.size());
        if (S.SplitterCount > 3)
            S.SplitterCount = 3;
        for (int32 i = 0; i < S.SplitterCount; ++i)
        {
            S.SplitterRatios[i] = AllSplitters[i]->GetRatio();
        }
    }
    else
    {
        S.SplitterCount = 0;
    }

    // Perspective 카메라(slot 0) 저장
    if (!LevelViewportClients.empty())
    {
        FEditorViewportCamera *Cam = LevelViewportClients[0]->GetCamera();
        if (Cam)
        {
            S.PerspCamLocation = Cam->GetWorldLocation();
            S.PerspCamRotation = Cam->GetRelativeRotation();
            const FMinimalViewInfo &CS = Cam->GetCameraState();
            S.PerspCamFOV = CS.FOV * (180.0f / 3.14159265358979f); // rad ??deg
            S.PerspCamNearClip = CS.NearZ;
            S.PerspCamFarClip = CS.FarZ;
        }
    }
}

void FLevelViewportLayout::LoadFromSettings()
{
    const FLevelEditorSettings &S = FLevelEditorSettings::Get();

    // 레이아웃 전환 (슬롯 생성 + 트리 빌드)
    EViewportLayout NewLayout = static_cast<EViewportLayout>(S.LayoutType);
    if (NewLayout >= EViewportLayout::MAX)
        NewLayout = EViewportLayout::OnePane;

    // OnePane이 아니면 레이아웃 적용 (Initialize에서 이미 OnePane으로 생성됨)
    if (NewLayout != EViewportLayout::OnePane)
    {
        // SetLayout을 거치지 않고 직접 적용
        SSplitter::DestroyTree(RootSplitter);
        RootSplitter = nullptr;
        DraggingSplitter = nullptr;

        int32 RequiredSlots = GetSlotCount(NewLayout);
        EnsureViewportSlots(RequiredSlots);

        RootSplitter = BuildSplitterTree(NewLayout);
        ActiveSlotCount = RequiredSlots;
        CurrentLayout = NewLayout;
    }

    // 뷰포트별 렌더 옵션 적용
    for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
    {
        FLevelEditorViewportClient *VC = LevelViewportClients[i];
        VC->GetRenderOptions() = S.SlotOptions[i];
        ApplyProjectViewportSettings(VC->GetRenderOptions());

        // ViewportType에 따라 카메라 ortho/방향 설정
        VC->SetViewportType(S.SlotOptions[i].ViewportType);
    }

    // Splitter 鍮꾩쑉 蹂듭썝
    if (RootSplitter)
    {
        TArray<SSplitter *> AllSplitters;
        SSplitter::CollectSplitters(RootSplitter, AllSplitters);
        for (int32 i = 0; i < S.SplitterCount && i < static_cast<int32>(AllSplitters.size()); ++i)
        {
            AllSplitters[i]->SetRatio(S.SplitterRatios[i]);
        }
    }

    // Perspective 카메라(slot 0) 복원
    if (!LevelViewportClients.empty())
    {
        FEditorViewportCamera *Cam = LevelViewportClients[0]->GetCamera();
        if (Cam)
        {
            Cam->SetRelativeLocation(S.PerspCamLocation);
            Cam->SetRelativeRotation(S.PerspCamRotation);

            FMinimalViewInfo CS = Cam->GetCameraState();
            CS.FOV = S.PerspCamFOV * (3.14159265358979f / 180.0f); // deg ??rad
            CS.NearZ = S.PerspCamNearClip;
            CS.FarZ = S.PerspCamFarClip;
            Cam->SetCameraState(CS);
        }
    }
}
