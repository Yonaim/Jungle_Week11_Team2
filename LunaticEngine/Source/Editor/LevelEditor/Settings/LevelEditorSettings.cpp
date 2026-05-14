#include "PCH/LunaticPCH.h"
#include "LevelEditor/Settings/LevelEditorSettings.h"
#include "Engine/Core/SimpleJsonWrapper.h"
#include "Platform/Paths.h"
#include "LevelEditor/Viewport/LevelViewportLayout.h"


#include <filesystem>
#include <fstream>

#include <algorithm>

namespace Key
{
    // Section
    constexpr const char *Viewport = "Viewport";
    constexpr const char *Paths = "Paths";

    // Viewport
    constexpr const char *CameraSpeed = "CameraSpeed";
    constexpr const char *CameraRotationSpeed = "CameraRotationSpeed";
    constexpr const char *CameraZoomSpeed = "CameraZoomSpeed";
    constexpr const char *InitViewPos = "InitViewPos";
    constexpr const char *InitLookAt = "InitLookAt";

    // Slot Render Options
    constexpr const char *ViewMode = "ViewMode";
    constexpr const char *bPrimitives = "bPrimitives";
    constexpr const char *bGrid = "bGrid";
    constexpr const char *bWorldAxis = "bWorldAxis";
    constexpr const char *bGizmo = "bGizmo";
    constexpr const char *bBillboardText = "bBillboardText";
    constexpr const char *bBoundingVolume = "bBoundingVolume";
    constexpr const char *bDebugDraw = "bDebugDraw";
    constexpr const char *bSceneBVH = "bSceneBVH";
    constexpr const char *bOctree = "bOctree";
    constexpr const char *bWorldBound = "bWorldBound";
    constexpr const char *bLightVisualization = "bLightVisualization";
    constexpr const char *bLightHitMap = "bLightHitMap";
    constexpr const char *bFog = "bFog";
    constexpr const char *bShowShadowFrustum = "bShowShadowFrustum";
    constexpr const char *bGammaCorrection = "bGammaCorrection";
    constexpr const char *GridSpacing = "GridSpacing";
    constexpr const char *GridHalfLineCount = "GridHalfLineCount";
    constexpr const char *GridLineThickness = "GridLineThickness";
    constexpr const char *GridMajorLineThickness = "GridMajorLineThickness";
    constexpr const char *GridMajorLineInterval = "GridMajorLineInterval";
    constexpr const char *GridMinorIntensity = "GridMinorIntensity";
    constexpr const char *GridMajorIntensity = "GridMajorIntensity";
    constexpr const char *GridAxisThickness = "GridAxisThickness";
    constexpr const char *GridAxisIntensity = "GridAxisIntensity";
    constexpr const char *DebugLineThickness = "DebugLineThickness";
    constexpr const char *ActorHelperBillboardScale = "ActorHelperBillboardScale";
    constexpr const char *CameraMoveSensitivity = "CameraMoveSensitivity";
    constexpr const char *CameraRotateSensitivity = "CameraRotateSensitivity";
    constexpr const char *DisplayGamma = "DisplayGamma";
    constexpr const char *GammaCorrectionBlend = "GammaCorrectionBlend";
    constexpr const char *bUseSRGBCurve = "bUseSRGBCurve";
    constexpr const char *DirectionalLightVisualizationScale = "DirectionalLightVisualizationScale";
    constexpr const char *PointLightVisualizationScale = "PointLightVisualizationScale";
    constexpr const char *SpotLightVisualizationScale = "SpotLightVisualizationScale";

    // Paths
    constexpr const char *EditorStartLevel = "EditorStartLevel";
    constexpr const char *ContentBrowserPath = "ContentBrowserPath";

    // Layout
    constexpr const char *Layout = "Layout";
    constexpr const char *LayoutType = "LayoutType";
    constexpr const char *Slots = "Slots";
    constexpr const char *ViewportType = "ViewportType";
    constexpr const char *SplitterRatios = "SplitterRatios";

    // UI Panels
    constexpr const char *UIPanels = "UIPanels";
    constexpr const char *ShowViewport = "ShowViewport";
    constexpr const char *ShowConsole = "ShowConsole";
    constexpr const char *ShowDetailsPanel = "ShowDetailsPanel";
    constexpr const char *ShowOutlinerPanel = "ShowOutlinerPanel";
    constexpr const char *ShowPlaceActors = "ShowPlaceActors";
    constexpr const char *ShowStatsPanel = "ShowStatsPanel";
    constexpr const char *ShowContentBrowser = "ShowContentBrowser";
    constexpr const char *ShowImGuiSettings = "ShowImGuiSettings";
    constexpr const char *ShowShadowMapDebug = "ShowShadowMapDebug";

    // Perspective Camera
    constexpr const char *PerspectiveCamera = "PerspectiveCamera";
    constexpr const char *Location = "Location";
    constexpr const char *Rotation = "Rotation";
    constexpr const char *FOV = "FOV";
    constexpr const char *NearClip = "NearClip";
    constexpr const char *FarClip = "FarClip";

    // Transform Tools
    constexpr const char *TransformTools = "TransformTools";
    constexpr const char *CoordSystem = "CoordSystem";
    constexpr const char *bEnableTranslationSnap = "bEnableTranslationSnap";
    constexpr const char *TranslationSnapSize = "TranslationSnapSize";
    constexpr const char *bEnableRotationSnap = "bEnableRotationSnap";
    constexpr const char *RotationSnapSize = "RotationSnapSize";
    constexpr const char *bEnableScaleSnap = "bEnableScaleSnap";
    constexpr const char *ScaleSnapSize = "ScaleSnapSize";
} // namespace Key


void FLevelEditorSettings::ResetEditorLayoutToDefault()
{
    LayoutType = 0;
    SplitterRatios[0] = 0.5f;
    SplitterRatios[1] = 0.5f;
    SplitterRatios[2] = 0.5f;
    SplitterCount = 0;

    for (FViewportRenderOptions &Options : SlotOptions)
    {
        Options = FViewportRenderOptions{};
    }

    Panels = FLevelEditorPanelVisibility{};
}

void FLevelEditorSettings::SaveToFile(const FString &Path) const
{
    using namespace json;

    JSON Root = Object();

    // Viewport
    JSON Viewport = Object();
    Viewport[Key::CameraSpeed] = CameraSpeed;
    Viewport[Key::CameraRotationSpeed] = CameraRotationSpeed;
    Viewport[Key::CameraZoomSpeed] = CameraZoomSpeed;

    JSON InitPos = Array(InitViewPos.X, InitViewPos.Y, InitViewPos.Z);
    Viewport[Key::InitViewPos] = InitPos;

    JSON LookAt = Array(InitLookAt.X, InitLookAt.Y, InitLookAt.Z);
    Viewport[Key::InitLookAt] = LookAt;

    Root[Key::Viewport] = Viewport;

    // Paths
    JSON PathsObj = Object();
    PathsObj[Key::EditorStartLevel] = FPaths::NormalizePath(EditorStartLevel);
    PathsObj[Key::ContentBrowserPath] = FPaths::NormalizePath(ContentBrowserPath);
    Root[Key::Paths] = PathsObj;

    // Layout
    JSON LayoutObj = Object();
    LayoutObj[Key::LayoutType] = LayoutType;

    JSON  SlotsArr = Array();
    int32 SlotCount = FLevelViewportLayout::GetSlotCount(static_cast<EViewportLayout>(LayoutType));
    for (int32 i = 0; i < SlotCount; ++i)
    {
        JSON                          SlotObj = Object();
        const FViewportRenderOptions &Opts = SlotOptions[i];
        SlotObj[Key::ViewMode] = static_cast<int32>(Opts.ViewMode);
        SlotObj[Key::ViewportType] = static_cast<int32>(Opts.ViewportType);
        SlotObj[Key::bPrimitives] = Opts.ShowFlags.bPrimitives;
        SlotObj[Key::bGrid] = Opts.ShowFlags.bGrid;
        SlotObj[Key::bWorldAxis] = Opts.ShowFlags.bWorldAxis;
        SlotObj[Key::bGizmo] = Opts.ShowFlags.bGizmo;
        SlotObj[Key::bBillboardText] = Opts.ShowFlags.bBillboardText;
        SlotObj[Key::bBoundingVolume] = Opts.ShowFlags.bBoundingVolume;
        SlotObj[Key::bDebugDraw] = Opts.ShowFlags.bDebugDraw;
        SlotObj[Key::bSceneBVH] = Opts.ShowFlags.bSceneBVH;
        SlotObj[Key::bOctree] = Opts.ShowFlags.bOctree;
        SlotObj[Key::bWorldBound] = Opts.ShowFlags.bWorldBound;
        SlotObj[Key::bLightVisualization] = Opts.ShowFlags.bLightVisualization;
        SlotObj[Key::bLightHitMap] = Opts.ShowFlags.bLightHitMap;
        SlotObj[Key::bFog] = Opts.ShowFlags.bFog;
        SlotObj[Key::bShowShadowFrustum] = Opts.ShowFlags.bShowShadowFrustum;
        SlotObj[Key::bGammaCorrection] = Opts.ShowFlags.bGammaCorrection;
        SlotObj[Key::GridSpacing] = Opts.GridSpacing;
        SlotObj[Key::GridHalfLineCount] = Opts.GridHalfLineCount;
        SlotObj[Key::GridLineThickness] = Opts.GridRenderSettings.LineThickness;
        SlotObj[Key::GridMajorLineThickness] = Opts.GridRenderSettings.MajorLineThickness;
        SlotObj[Key::GridMajorLineInterval] = Opts.GridRenderSettings.MajorLineInterval;
        SlotObj[Key::GridMinorIntensity] = Opts.GridRenderSettings.MinorIntensity;
        SlotObj[Key::GridMajorIntensity] = Opts.GridRenderSettings.MajorIntensity;
        SlotObj[Key::GridAxisThickness] = Opts.GridRenderSettings.AxisThickness;
        SlotObj[Key::GridAxisIntensity] = Opts.GridRenderSettings.AxisIntensity;
        SlotObj[Key::DebugLineThickness] = Opts.DebugLineThickness;
        SlotObj[Key::ActorHelperBillboardScale] = Opts.ActorHelperBillboardScale;
        SlotObj[Key::CameraMoveSensitivity] = Opts.CameraMoveSensitivity;
        SlotObj[Key::CameraRotateSensitivity] = Opts.CameraRotateSensitivity;
        SlotObj[Key::DisplayGamma] = Opts.DisplayGamma;
        SlotObj[Key::GammaCorrectionBlend] = Opts.GammaCorrectionBlend;
        SlotObj[Key::bUseSRGBCurve] = Opts.bUseSRGBCurve;
        SlotObj[Key::DirectionalLightVisualizationScale] = Opts.DirectionalLightVisualizationScale;
        SlotObj[Key::PointLightVisualizationScale] = Opts.PointLightVisualizationScale;
        SlotObj[Key::SpotLightVisualizationScale] = Opts.SpotLightVisualizationScale;
        SlotsArr.append(SlotObj);
    }
    LayoutObj[Key::Slots] = SlotsArr;

    JSON RatiosArr = Array();
    for (int32 i = 0; i < SplitterCount; ++i)
    {
        RatiosArr.append(SplitterRatios[i]);
    }
    LayoutObj[Key::SplitterRatios] = RatiosArr;
    Root[Key::Layout] = LayoutObj;

    // UI Panels
    JSON PanelsObj = Object();
    PanelsObj[Key::ShowViewport] = Panels.bViewport;
    PanelsObj[Key::ShowConsole] = Panels.bConsole;
    PanelsObj[Key::ShowDetailsPanel] = Panels.bDetails;
    PanelsObj[Key::ShowOutlinerPanel] = Panels.bOutliner;
    PanelsObj[Key::ShowPlaceActors] = Panels.bPlaceActors;
    PanelsObj[Key::ShowStatsPanel] = Panels.bStats;
    PanelsObj[Key::ShowContentBrowser] = Panels.bContentBrowser;
    PanelsObj[Key::ShowImGuiSettings] = Panels.bImGuiSettings;
    PanelsObj[Key::ShowShadowMapDebug] = Panels.bShadowMapDebug;
    Root[Key::UIPanels] = PanelsObj;
    // Perspective Camera
    JSON CamObj = Object();
    CamObj[Key::Location] = Array(PerspCamLocation.X, PerspCamLocation.Y, PerspCamLocation.Z);
    CamObj[Key::Rotation] = Array(PerspCamRotation.Roll, PerspCamRotation.Pitch, PerspCamRotation.Yaw);
    CamObj[Key::FOV] = PerspCamFOV;
    CamObj[Key::NearClip] = PerspCamNearClip;
    CamObj[Key::FarClip] = PerspCamFarClip;
    Root[Key::PerspectiveCamera] = CamObj;

    JSON TransformObj = Object();
    TransformObj[Key::CoordSystem] = static_cast<int32>(CoordSystem);
    TransformObj[Key::bEnableTranslationSnap] = bEnableTranslationSnap;
    TransformObj[Key::TranslationSnapSize] = TranslationSnapSize;
    TransformObj[Key::bEnableRotationSnap] = bEnableRotationSnap;
    TransformObj[Key::RotationSnapSize] = RotationSnapSize;
    TransformObj[Key::bEnableScaleSnap] = bEnableScaleSnap;
    TransformObj[Key::ScaleSnapSize] = ScaleSnapSize;
    Root[Key::TransformTools] = TransformObj;

    // Ensure directory exists
    std::filesystem::path FilePath(FPaths::ToWide(Path));
    if (FilePath.has_parent_path())
    {
        std::filesystem::create_directories(FilePath.parent_path());
    }

    std::ofstream File(FilePath);
    if (File.is_open())
    {
        File << Root;
    }
}

void FLevelEditorSettings::LoadFromFile(const FString &Path)
{
    using namespace json;

    std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
    if (!File.is_open())
    {
        return;
    }

    FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());

    JSON Root = JSON::Load(Content);

    // Viewport
    if (Root.hasKey(Key::Viewport))
    {
        JSON Viewport = Root[Key::Viewport];

        if (Viewport.hasKey(Key::CameraSpeed))
            CameraSpeed = static_cast<float>(Viewport[Key::CameraSpeed].ToFloat());
        if (Viewport.hasKey(Key::CameraRotationSpeed))
            CameraRotationSpeed = static_cast<float>(Viewport[Key::CameraRotationSpeed].ToFloat());
        if (Viewport.hasKey(Key::CameraZoomSpeed))
            CameraZoomSpeed = static_cast<float>(Viewport[Key::CameraZoomSpeed].ToFloat());

        if (Viewport.hasKey(Key::InitViewPos))
        {
            JSON Pos = Viewport[Key::InitViewPos];
            InitViewPos =
                FVector(static_cast<float>(Pos[0].ToFloat()), static_cast<float>(Pos[1].ToFloat()), static_cast<float>(Pos[2].ToFloat()));
        }

        if (Viewport.hasKey(Key::InitLookAt))
        {
            JSON Look = Viewport[Key::InitLookAt];
            InitLookAt = FVector(static_cast<float>(Look[0].ToFloat()), static_cast<float>(Look[1].ToFloat()),
                                 static_cast<float>(Look[2].ToFloat()));
        }
    }

    // Paths
    if (Root.hasKey(Key::Paths))
    {
        JSON PathsObj = Root[Key::Paths];

        if (PathsObj.hasKey(Key::EditorStartLevel))
            EditorStartLevel = FPaths::NormalizePath(PathsObj[Key::EditorStartLevel].ToString());
        if (PathsObj.hasKey(Key::ContentBrowserPath))
            ContentBrowserPath = FPaths::NormalizePath(PathsObj[Key::ContentBrowserPath].ToString());
    }

    // Layout
    if (Root.hasKey(Key::Layout))
    {
        JSON LayoutObj = Root[Key::Layout];

        if (LayoutObj.hasKey(Key::LayoutType))
            LayoutType = LayoutObj[Key::LayoutType].ToInt();

        if (LayoutObj.hasKey(Key::Slots))
        {
            JSON SlotsArr = LayoutObj[Key::Slots];
            for (int32 i = 0; i < static_cast<int32>(SlotsArr.length()) && i < 4; ++i)
            {
                JSON                    S = SlotsArr[i];
                FViewportRenderOptions &Opts = SlotOptions[i];
                if (S.hasKey(Key::ViewMode))
                    Opts.ViewMode = static_cast<EViewMode>(S[Key::ViewMode].ToInt());
                if (S.hasKey(Key::ViewportType))
                    Opts.ViewportType = static_cast<ELevelViewportType>(S[Key::ViewportType].ToInt());
                if (S.hasKey(Key::bPrimitives))
                    Opts.ShowFlags.bPrimitives = S[Key::bPrimitives].ToBool();
                if (S.hasKey(Key::bGrid))
                    Opts.ShowFlags.bGrid = S[Key::bGrid].ToBool();
                if (S.hasKey(Key::bWorldAxis))
                    Opts.ShowFlags.bWorldAxis = S[Key::bWorldAxis].ToBool();
                if (S.hasKey(Key::bGizmo))
                    Opts.ShowFlags.bGizmo = S[Key::bGizmo].ToBool();
                if (S.hasKey(Key::bBillboardText))
                    Opts.ShowFlags.bBillboardText = S[Key::bBillboardText].ToBool();
                if (S.hasKey(Key::bBoundingVolume))
                    Opts.ShowFlags.bBoundingVolume = S[Key::bBoundingVolume].ToBool();
                if (S.hasKey(Key::bDebugDraw))
                    Opts.ShowFlags.bDebugDraw = S[Key::bDebugDraw].ToBool();
                if (S.hasKey(Key::bSceneBVH))
                    Opts.ShowFlags.bSceneBVH = S[Key::bSceneBVH].ToBool();
                if (S.hasKey(Key::bOctree))
                    Opts.ShowFlags.bOctree = S[Key::bOctree].ToBool();
                if (S.hasKey(Key::bWorldBound))
                    Opts.ShowFlags.bWorldBound = S[Key::bWorldBound].ToBool();
                if (S.hasKey(Key::bLightVisualization))
                    Opts.ShowFlags.bLightVisualization = S[Key::bLightVisualization].ToBool();
                if (S.hasKey(Key::bLightHitMap))
                    Opts.ShowFlags.bLightHitMap = S[Key::bLightHitMap].ToBool();
                if (S.hasKey(Key::bFog))
                    Opts.ShowFlags.bFog = S[Key::bFog].ToBool();
                if (S.hasKey(Key::bShowShadowFrustum))
                    Opts.ShowFlags.bShowShadowFrustum = S[Key::bShowShadowFrustum].ToBool();
                if (S.hasKey(Key::bGammaCorrection))
                    Opts.ShowFlags.bGammaCorrection = S[Key::bGammaCorrection].ToBool();
                if (S.hasKey(Key::GridSpacing))
                    Opts.GridSpacing = static_cast<float>(S[Key::GridSpacing].ToFloat());
                if (S.hasKey(Key::GridHalfLineCount))
                    Opts.GridHalfLineCount = S[Key::GridHalfLineCount].ToInt();
                if (S.hasKey(Key::GridLineThickness))
                    Opts.GridRenderSettings.LineThickness = std::clamp(static_cast<float>(S[Key::GridLineThickness].ToFloat()), 0.0f, 8.0f);
                if (S.hasKey(Key::GridMajorLineThickness))
                    Opts.GridRenderSettings.MajorLineThickness =
                        std::clamp(static_cast<float>(S[Key::GridMajorLineThickness].ToFloat()), 0.0f, 12.0f);
                if (S.hasKey(Key::GridMajorLineInterval))
                    Opts.GridRenderSettings.MajorLineInterval = std::clamp<int32>(S[Key::GridMajorLineInterval].ToInt(), 1, 100);
                if (S.hasKey(Key::GridMinorIntensity))
                    Opts.GridRenderSettings.MinorIntensity =
                        std::clamp(static_cast<float>(S[Key::GridMinorIntensity].ToFloat()), 0.0f, 2.0f);
                if (S.hasKey(Key::GridMajorIntensity))
                    Opts.GridRenderSettings.MajorIntensity =
                        std::clamp(static_cast<float>(S[Key::GridMajorIntensity].ToFloat()), 0.0f, 2.0f);
                if (S.hasKey(Key::GridAxisThickness))
                    Opts.GridRenderSettings.AxisThickness =
                        std::clamp(static_cast<float>(S[Key::GridAxisThickness].ToFloat()), 0.0f, 12.0f);
                if (S.hasKey(Key::GridAxisIntensity))
                    Opts.GridRenderSettings.AxisIntensity = std::clamp(static_cast<float>(S[Key::GridAxisIntensity].ToFloat()), 0.0f, 2.0f);
                if (S.hasKey(Key::DebugLineThickness))
                    Opts.DebugLineThickness = static_cast<float>(S[Key::DebugLineThickness].ToFloat());
                if (S.hasKey(Key::ActorHelperBillboardScale))
                    Opts.ActorHelperBillboardScale = static_cast<float>(S[Key::ActorHelperBillboardScale].ToFloat());
                if (S.hasKey(Key::CameraMoveSensitivity))
                    Opts.CameraMoveSensitivity = static_cast<float>(S[Key::CameraMoveSensitivity].ToFloat());
                if (S.hasKey(Key::CameraRotateSensitivity))
                    Opts.CameraRotateSensitivity = static_cast<float>(S[Key::CameraRotateSensitivity].ToFloat());
                if (S.hasKey(Key::DisplayGamma))
                    Opts.DisplayGamma = static_cast<float>(S[Key::DisplayGamma].ToFloat());
                if (S.hasKey(Key::GammaCorrectionBlend))
                    Opts.GammaCorrectionBlend = static_cast<float>(S[Key::GammaCorrectionBlend].ToFloat());
                if (S.hasKey(Key::bUseSRGBCurve))
                    Opts.bUseSRGBCurve = S[Key::bUseSRGBCurve].ToBool();
                if (S.hasKey(Key::DirectionalLightVisualizationScale))
                    Opts.DirectionalLightVisualizationScale = static_cast<float>(S[Key::DirectionalLightVisualizationScale].ToFloat());
                if (S.hasKey(Key::PointLightVisualizationScale))
                    Opts.PointLightVisualizationScale = static_cast<float>(S[Key::PointLightVisualizationScale].ToFloat());
                if (S.hasKey(Key::SpotLightVisualizationScale))
                    Opts.SpotLightVisualizationScale = static_cast<float>(S[Key::SpotLightVisualizationScale].ToFloat());
            }
        }

        if (LayoutObj.hasKey(Key::SplitterRatios))
        {
            JSON RatiosArr = LayoutObj[Key::SplitterRatios];
            SplitterCount = static_cast<int32>(RatiosArr.length());
            if (SplitterCount > 3)
                SplitterCount = 3;
            for (int32 i = 0; i < SplitterCount; ++i)
            {
                SplitterRatios[i] = static_cast<float>(RatiosArr[i].ToFloat());
            }
        }
    }

    // UI Panels
    if (Root.hasKey(Key::UIPanels))
    {
        JSON P = Root[Key::UIPanels];
        if (P.hasKey(Key::ShowViewport))
            Panels.bViewport = P[Key::ShowViewport].ToBool();
        if (P.hasKey(Key::ShowConsole))
            Panels.bConsole = P[Key::ShowConsole].ToBool();
        if (P.hasKey(Key::ShowDetailsPanel))
            Panels.bDetails = P[Key::ShowDetailsPanel].ToBool();
        if (P.hasKey(Key::ShowOutlinerPanel))
            Panels.bOutliner = P[Key::ShowOutlinerPanel].ToBool();
        if (P.hasKey(Key::ShowPlaceActors))
            Panels.bPlaceActors = P[Key::ShowPlaceActors].ToBool();
        if (P.hasKey(Key::ShowStatsPanel))
            Panels.bStats = P[Key::ShowStatsPanel].ToBool();
        else if (P.hasKey("ShowStatProfiler"))
            Panels.bStats = P["ShowStatProfiler"].ToBool();
        if (P.hasKey(Key::ShowContentBrowser))
            Panels.bContentBrowser = P[Key::ShowContentBrowser].ToBool();
        if (P.hasKey(Key::ShowImGuiSettings))
            Panels.bImGuiSettings = P[Key::ShowImGuiSettings].ToBool();
        if (P.hasKey(Key::ShowShadowMapDebug))
            Panels.bShadowMapDebug = P[Key::ShowShadowMapDebug].ToBool();
    }

    else if (Root.hasKey("UIPanels"))
    {
        JSON LegacyPanels = Root["UIPanels"];
        if (LegacyPanels.hasKey(Key::ShowViewport))
            Panels.bViewport = LegacyPanels[Key::ShowViewport].ToBool();
        if (LegacyPanels.hasKey(Key::ShowConsole))
            Panels.bConsole = LegacyPanels[Key::ShowConsole].ToBool();
        if (LegacyPanels.hasKey("ShowPropertyWindow"))
            Panels.bDetails = LegacyPanels["ShowPropertyWindow"].ToBool();
        if (LegacyPanels.hasKey("ShowSceneManager"))
            Panels.bOutliner = LegacyPanels["ShowSceneManager"].ToBool();
        if (LegacyPanels.hasKey(Key::ShowPlaceActors))
            Panels.bPlaceActors = LegacyPanels[Key::ShowPlaceActors].ToBool();
        if (LegacyPanels.hasKey("ShowStatProfiler"))
            Panels.bStats = LegacyPanels["ShowStatProfiler"].ToBool();
        if (LegacyPanels.hasKey(Key::ShowContentBrowser))
            Panels.bContentBrowser = LegacyPanels[Key::ShowContentBrowser].ToBool();
        if (LegacyPanels.hasKey(Key::ShowImGuiSettings))
            Panels.bImGuiSettings = LegacyPanels[Key::ShowImGuiSettings].ToBool();
        if (LegacyPanels.hasKey(Key::ShowShadowMapDebug))
            Panels.bShadowMapDebug = LegacyPanels[Key::ShowShadowMapDebug].ToBool();
    }
    // Perspective Camera
    if (Root.hasKey(Key::PerspectiveCamera))
    {
        JSON CamObj = Root[Key::PerspectiveCamera];
        if (CamObj.hasKey(Key::Location))
        {
            JSON L = CamObj[Key::Location];
            PerspCamLocation =
                FVector(static_cast<float>(L[0].ToFloat()), static_cast<float>(L[1].ToFloat()), static_cast<float>(L[2].ToFloat()));
        }
        if (CamObj.hasKey(Key::Rotation))
        {
            JSON R = CamObj[Key::Rotation];
            // JSON 포맷: [Roll, Pitch, Yaw] (FVector X,Y,Z 호환)
            float Roll = static_cast<float>(R[0].ToFloat());
            float Pitch = static_cast<float>(R[1].ToFloat());
            float Yaw = static_cast<float>(R[2].ToFloat());
            PerspCamRotation = FRotator(Pitch, Yaw, Roll);
        }
        if (CamObj.hasKey(Key::FOV))
            PerspCamFOV = static_cast<float>(CamObj[Key::FOV].ToFloat());
        if (CamObj.hasKey(Key::NearClip))
            PerspCamNearClip = static_cast<float>(CamObj[Key::NearClip].ToFloat());
        if (CamObj.hasKey(Key::FarClip))
            PerspCamFarClip = static_cast<float>(CamObj[Key::FarClip].ToFloat());
    }

    if (Root.hasKey(Key::TransformTools))
    {
        JSON TransformObj = Root[Key::TransformTools];
        if (TransformObj.hasKey(Key::CoordSystem))
            CoordSystem = static_cast<EEditorCoordSystem>(TransformObj[Key::CoordSystem].ToInt());
        if (TransformObj.hasKey(Key::bEnableTranslationSnap))
            bEnableTranslationSnap = TransformObj[Key::bEnableTranslationSnap].ToBool();
        if (TransformObj.hasKey(Key::TranslationSnapSize))
            TranslationSnapSize = static_cast<float>(TransformObj[Key::TranslationSnapSize].ToFloat());
        if (TransformObj.hasKey(Key::bEnableRotationSnap))
            bEnableRotationSnap = TransformObj[Key::bEnableRotationSnap].ToBool();
        if (TransformObj.hasKey(Key::RotationSnapSize))
            RotationSnapSize = static_cast<float>(TransformObj[Key::RotationSnapSize].ToFloat());
        if (TransformObj.hasKey(Key::bEnableScaleSnap))
            bEnableScaleSnap = TransformObj[Key::bEnableScaleSnap].ToBool();
        if (TransformObj.hasKey(Key::ScaleSnapSize))
            ScaleSnapSize = static_cast<float>(TransformObj[Key::ScaleSnapSize].ToFloat());
    }
}
