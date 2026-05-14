#include "PCH/LunaticPCH.h"
#include "EditorEngine.h"
#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Common/Gizmo/GizmoManager.h"
#include "Common/Viewport/EditorViewportClient.h"
#include "Common/UI/Notifications/NotificationToast.h"

#include "Audio/AudioManager.h"
#include "Common/File/EditorFileUtils.h"
#include "Component/CameraComponent.h"
#include "Core/Notification.h"
#include "Core/ProjectSettings.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Platform/DirectoryWatcher.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/AActor.h"
#include "LevelEditor/History/SceneHistoryBuilder.h"
#include "LevelEditor/PIE/LevelPIEManager.h"
#include "LevelEditor/Render/EditorRenderPipeline.h"
#include "LevelEditor/Viewport/LevelEditorViewportClient.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshAssetManager.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Profiling/StartupProfiler.h"
#include "Texture/Texture2D.h"
#include <filesystem>
#include <fstream>
#include <set>

IMPLEMENT_CLASS(UEditorEngine, UEngine)

namespace
{
void DeactivateEditorViewportClient(FEditorViewportClient* Client)
{
    if (Client)
    {
        Client->DeactivateEditorContext();
    }
}

void DeactivateLevelViewportClients(const TArray<FLevelEditorViewportClient*>& Clients)
{
    for (FLevelEditorViewportClient* Client : Clients)
    {
        DeactivateEditorViewportClient(Client);
    }
}

void ActivateLevelViewportClient(FLevelEditorViewportClient* Client)
{
    if (Client)
    {
        Client->ActivateEditorContext();
    }
}

void ApplyLevelTransformSettingsToGizmo(FGizmoManager& Manager, const FLevelEditorSettings& Settings, EGizmoMode Mode)
{
    const bool bForceLocalForScale = Mode == EGizmoMode::Scale;
    const EGizmoSpace Space = bForceLocalForScale || Settings.CoordSystem != EEditorCoordSystem::World
                                  ? EGizmoSpace::Local
                                  : EGizmoSpace::World;

    Manager.SetMode(Mode);
    Manager.SetSpace(Space);
    Manager.SetSnapSettings(Settings.bEnableTranslationSnap, Settings.TranslationSnapSize,
                            Settings.bEnableRotationSnap, Settings.RotationSnapSize,
                            Settings.bEnableScaleSnap, Settings.ScaleSnapSize);
    Manager.SyncVisualFromTarget();
}


void DeactivateAllExceptActiveLevelViewport(FLevelViewportLayout* Layout)
{
    if (!Layout)
    {
        return;
    }

    FLevelEditorViewportClient* ActiveClient = Layout->GetActiveViewport();
    for (FLevelEditorViewportClient* Client : Layout->GetLevelViewportClients())
    {
        if (!Client)
        {
            continue;
        }

        if (Client == ActiveClient)
        {
            // IMPORTANT: Do not call ActivateEditorContext() every frame.
            // Activation resets transient gizmo interaction state; doing that from
            // UEditorEngine::Tick() makes the active gizmo unable to start/continue drag.
            // Only activate when this viewport was actually inactive.
            if (!Client->IsEditorContextActive())
            {
                Client->ActivateEditorContext();
            }
        }
        else
        {
            if (Client->IsEditorContextActive())
            {
                Client->DeactivateEditorContext();
            }
        }
    }
}
} // namespace

void UEditorEngine::Init(FWindowsWindow *InWindow)
{

    GIsEditor = true;

    // 엔진 공통 초기화 (Renderer, D3D, 싱글턴 등)
    UEngine::Init(InWindow);

    if (InWindow)
    {
        FInputManager::Get().SetOwnerWindow(InWindow->GetHWND());
    }

    {
        SCOPE_STARTUP_STAT("MeshAssetManager::ScanMeshAssets");
        FMeshAssetManager::ScanMeshAssets();
        FMeshAssetManager::ScanMeshSourceFiles();
    }

    {
        SCOPE_STARTUP_STAT("MaterialManager::ScanAssets");
        FMaterialManager::Get().ScanMaterialAssets();
    }
    UTexture2D::ScanTextureAssets();

    // 에디터 전용 초기화
    FLevelEditorSettings::Get().LoadFromFile(FLevelEditorSettings::GetDefaultSettingsPath());
    // 사용자 설정에서 카메라/경로 정보는 가져오되, Editor 레이아웃과 패널 상태는 매 실행마다 기본값으로 시작한다.
    FLevelEditorSettings::Get().ResetEditorLayoutToDefault();
    FProjectSettings::Get().LoadFromFile(FProjectSettings::GetDefaultPath());

    AssetEditorManager.Initialize(this, &Renderer);
    AssetImportManager.Init(this);

    // 기본 월드 생성 — 모든 서브시스템 초기화의 기반
    CreateWorldContext(EWorldType::Editor, FName("Default"));
    SetActiveWorld(WorldList[0].ContextHandle);
    GetWorld()->InitWorld();

    {
        SCOPE_STARTUP_STAT("LevelEditor::Initialize");
        LevelEditor.Initialize(this, Window, Renderer);
    }

    ImGuiSystem.Initialize(Window, &Renderer.GetFD3DDevice());
    LevelEditorWindow.Create(Window, Renderer, this, &LevelEditor);

    {
        SCOPE_STARTUP_STAT("Editor::LoadStartLevel");
        LevelEditor.GetSceneManager().LoadStartLevel();
    }

    // Initial editor context must be explicitly activated after the Level viewport clients exist.
    // ActiveEditorContextType defaults to LevelEditor, but the viewport clients start with
    // bEditorContextActive=false; without this, the first Level viewport render request is rejected.
    SetActiveEditorContext(EEditorContextType::LevelEditor);
    ApplyTransformSettingsToGizmo();

    // 에디터 렌더 파이프라인
    {
        SCOPE_STARTUP_STAT("EditorRenderPipeline::Create");
        SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));
    }
}

void UEditorEngine::Shutdown()
{
    // 에디터 해제 (엔진보다 먼저)
    LevelEditor.GetViewportLayout().SaveToSettings();
    LevelEditorWindow.SaveToSettings();
    FProjectSettings::Get().SaveToFile(FProjectSettings::GetDefaultPath());
    FLevelEditorSettings::Get().SaveToFile(FLevelEditorSettings::GetDefaultSettingsPath());
    CloseScene();
    FDirectoryWatcher::Get().Shutdown();
    AssetEditorManager.Shutdown();
    AssetImportManager.Shutdown();
    LevelEditor.Shutdown();
    LevelEditorWindow.Release();
    ImGuiSystem.Shutdown();

    // 엔진 공통 해제 (Renderer, D3D 등)
    UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);
    // 윈도우 리사이즈 시에는 ImGui 패널이 실제 크기를 결정하므로
    // FViewport RT는 SSplitter 레이아웃에서 지연 리사이즈로 처리됨
}

void UEditorEngine::Tick(float DeltaTime)
{
    if (!PendingSceneLoadReference.empty())
    {
        const FString SceneToLoad = PendingSceneLoadReference;
        PendingSceneLoadReference.clear();
        LoadScene(SceneToLoad);
    }
    // 절대 격리 안전장치: 매 프레임 현재 활성 editor context가 아닌 쪽의 viewport/gizmo를 강제로 끊는다.
    // 설계적으로 예쁘지는 않지만, Level/Asset 탭이 동시에 열린 상태에서 hidden tab의 stale gizmo target이
    // 나중에 transform을 적용하는 문제를 즉시 차단한다.
    if (IsAssetEditorContextActive())
    {
        if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
        {
            DeactivateLevelViewportClients(ActiveLayout->GetLevelViewportClients());
        }
    }
    else
    {
        AssetEditorManager.ForceDeactivateAllViewportClients();
        DeactivateAllExceptActiveLevelViewport(LevelEditorWindow.GetActiveLevelViewportLayout());
    }

    // Asset Editor 컨텍스트가 활성화되어 있어도 Level 월드와 에디터 서브시스템은 계속 Tick된다.
    // 활성 컨텍스트는 뷰포트 입력/렌더 포커스만 가지며, 에디터 월드 자체를 멈추지는 않는다.
    LevelEditor.Tick(DeltaTime);

    ApplyTransformSettingsToGizmo();
    FDirectoryWatcher::Get().ProcessChanges();
    AssetImportManager.Tick();
    if (UTexture2D::HasPendingTextureRefresh())
    {
        UTexture2D::RefreshChangedTextures(Renderer.GetFD3DDevice().GetDevice());
    }
    FNotificationManager::Get().Tick(DeltaTime);
    FAudioManager::Get().Update();

    // Asset Editor는 Level Editor로 돌아간 뒤에도 탭을 메모리에 유지할 수 있다.
    // 하지만 비활성 컨텍스트에서는 preview viewport / skeletal skinning / gizmo debug tick을
    // 돌리면 보이지 않는 Asset Editor 때문에 프레임이 크게 떨어진다.
    if (IsAssetEditorContextActive())
    {
        AssetEditorManager.Tick(DeltaTime);
    }

    LevelEditorWindow.Update();

    // 입력 캡처 여부는 보이는 모든 패널이 아니라 현재 활성 에디터 컨텍스트가 결정한다.
    // 이렇게 하면 Asset Editor 패널이 Level Viewport 입력을 가로채지 않고,
    // 커서가 Asset Preview Viewport 위에 있을 때는 해당 뷰포트가 ImGui 캡처를 적절히 해제할 수 있다.
    LevelEditorWindow.UpdateInputState(IsMouseOverActiveViewport(), IsAssetEditorCapturingInput(), IsScoreSavePopupOpen());

    if (IsLevelEditorContextActive())
    {
        if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
        {
            for (FEditorViewportClient *VC : ActiveLayout->GetAllViewportClients())
            {
                if (VC)
                {
                    VC->Tick(DeltaTime);
                }
            }
        }
    }

    WorldTick(DeltaTime);
    Render(DeltaTime);

    if (!IsPIEPossessedMode())
    {
        GetSelectionManager().Tick();
    }
}

bool UEditorEngine::LoadScene(const FString &InSceneReference)
{
    return LevelEditor.GetPIEManager().LoadScene(InSceneReference);
}

FEditorViewportCamera *UEditorEngine::GetCamera() const
{
    if (const FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
    {
        if (FLevelEditorViewportClient *ActiveVC = ActiveLayout->GetActiveViewport())
        {
            return ActiveVC->GetCamera();
        }
    }
    return nullptr;
}

bool UEditorEngine::FocusActorInViewport(AActor *Actor)
{
    if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
    {
        if (FLevelEditorViewportClient *ActiveVC = ActiveLayout->GetActiveViewport())
        {
            return ActiveVC->FocusActor(Actor);
        }
    }
    return false;
}

void UEditorEngine::SetActiveEditorContext(EEditorContextType InContextType)
{
    const EEditorContextType PreviousContextType = ActiveEditorContextType;
    if (PreviousContextType == InContextType)
    {
        // Re-entering the same context can happen when selecting a document tab inside the same main window.
        // Treat it as a full ownership refresh, not a no-op.  This cleans up the opposite editor side even if
        // an earlier activation path accidentally left a viewport alive.
        if (InContextType == EEditorContextType::AssetEditor)
        {
            if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
            {
                DeactivateLevelViewportClients(ActiveLayout->GetLevelViewportClients());
            }
            AssetEditorManager.GetAssetEditorWindow().EnterEditorContext();
        }
        else
        {
            AssetEditorManager.GetAssetEditorWindow().ExitEditorContext();
            AssetEditorManager.ForceDeactivateAllViewportClients();
            if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
            {
                DeactivateAllExceptActiveLevelViewport(ActiveLayout);
            }
            ApplyTransformSettingsToGizmo();
        }
        return;
    }

    if (PreviousContextType == EEditorContextType::AssetEditor)
    {
        AssetEditorManager.GetAssetEditorWindow().ExitEditorContext();
    }
    else
    {
        // Level Editor는 split viewport를 가질 수 있으므로 active viewport 하나만 끊으면 부족하다.
        // Asset Editor 탭으로 넘어갈 때 모든 Level viewport의 live context / gizmo target / input state를 끊는다.
        if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
        {
            DeactivateLevelViewportClients(ActiveLayout->GetLevelViewportClients());
        }
    }

    ActiveEditorContextType = InContextType;

    if (IsAssetEditorContextActive())
    {
        AssetEditorManager.GetAssetEditorWindow().EnterEditorContext();
        return;
    }

    // Asset Editor 탭은 닫지 않는다. state/selection/preview scene/layout은 탭 객체에 유지하고,
    // live input/tick/render target sync만 ExitEditorContext()에서 끊는다.
    AssetEditorManager.ForceDeactivateAllViewportClients();
    RestoreLevelEditorUIAfterAssetEditor();
    if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
    {
        DeactivateAllExceptActiveLevelViewport(ActiveLayout);
    }
    ApplyTransformSettingsToGizmo();
}

FEditorViewportClient *UEditorEngine::GetActiveEditorViewportClient() const
{
    if (IsAssetEditorContextActive())
    {
        return AssetEditorManager.GetActiveViewportClient();
    }

    if (const FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
    {
        return ActiveLayout->GetActiveViewport();
    }
    return nullptr;
}

bool UEditorEngine::IsMouseOverActiveViewport() const
{
    if (IsAssetEditorContextActive())
    {
        if (FEditorViewportClient *ViewportClient = AssetEditorManager.GetActiveViewportClient())
        {
            return ViewportClient->IsHovered();
        }
        return false;
    }

    if (const FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
    {
        return ActiveLayout->IsMouseOverViewport();
    }
    return false;
}

void UEditorEngine::RenderUI(float DeltaTime)
{
    ImGuiSystem.BeginFrame();

    LevelEditorWindow.RenderContent(DeltaTime);
    if (IsAssetEditorContextActive())
    {
        AssetEditorManager.RenderContent(DeltaTime, LevelEditorWindow.GetMainDockspaceId());
    }

    // Level Editor 패널과 Asset/SkeletalMesh Editor 패널을 모두 렌더링한 뒤 한 번만 flush한다.
    // PanelTitleUtils는 이 시점에 dock tab bar의 빈 영역 fill과 선택 탭 accent line을 그린다.
    PanelTitleUtils::FlushPanelDecorations();
    FNotificationToast::Render();

    ImGuiSystem.EndFrame();
    LevelEditorWindow.FlushPendingMenuAction();
}

bool UEditorEngine::CanCloseEditorWithPrompt() const
{
    return LevelEditorWindow.CanCloseEditorWindowWithPrompt();
}

void UEditorEngine::RenderPIEOverlayPopups() { LevelEditor.GetPIEManager().RenderOverlayPopups(); }

void UEditorEngine::OpenScoreSavePopup(int32 InScore) { LevelEditor.GetPIEManager().OpenScoreSavePopup(InScore); }

bool UEditorEngine::ConsumeScoreSavePopupResult(FString &OutNickname) { return LevelEditor.GetPIEManager().ConsumeScoreSavePopupResult(OutNickname); }

void UEditorEngine::OpenMessagePopup(const FString &InMessage) { LevelEditor.GetPIEManager().OpenMessagePopup(InMessage); }

bool UEditorEngine::ConsumeMessagePopupConfirmed() { return LevelEditor.GetPIEManager().ConsumeMessagePopupConfirmed(); }

void UEditorEngine::OpenScoreboardPopup(const FString &InFilePath) { LevelEditor.GetPIEManager().OpenScoreboardPopup(InFilePath); }

void UEditorEngine::OpenTitleOptionsPopup() { LevelEditor.GetPIEManager().OpenTitleOptionsPopup(); }

void UEditorEngine::OpenTitleCreditsPopup() { LevelEditor.GetPIEManager().OpenTitleCreditsPopup(); }

bool UEditorEngine::IsScoreSavePopupOpen() const { return LevelEditor.GetPIEManager().IsScoreSavePopupOpen(); }

void UEditorEngine::ToggleCoordSystem()
{
    if (!IsLevelEditorContextActive())
    {
        return;
    }

    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
    Settings.CoordSystem = (Settings.CoordSystem == EEditorCoordSystem::World) ? EEditorCoordSystem::Local : EEditorCoordSystem::World;
    ApplyTransformSettingsToGizmo();
}

void UEditorEngine::SetEditorGizmoMode(EGizmoMode NewMode)
{
    if (!IsLevelEditorContextActive())
    {
        return;
    }

    if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
    {
        if (FLevelEditorViewportClient *Client = ActiveLayout->GetActiveViewport())
        {
            ApplyLevelTransformSettingsToGizmo(Client->GetGizmoManager(), FLevelEditorSettings::Get(), NewMode);
        }
    }
}

EGizmoMode UEditorEngine::GetEditorGizmoMode() const
{
    if (const FEditorViewportClient *Client = GetActiveEditorViewportClient())
    {
        return Client->GetGizmoManager().GetMode();
    }
    return EGizmoMode::Translate;
}

void UEditorEngine::ApplyTransformSettingsToGizmo()
{
    if (!IsLevelEditorContextActive())
    {
        // Asset Editor 탭은 각 탭/viewport client가 자기 상태를 직접 적용한다.
        // Level Editor 전역 툴 상태를 Asset Editor의 FGizmoManager에 절대 밀어넣지 않는다.
        return;
    }

    if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
    {
        if (FLevelEditorViewportClient *Client = ActiveLayout->GetActiveViewport())
        {
            ApplyLevelTransformSettingsToGizmo(Client->GetGizmoManager(), FLevelEditorSettings::Get(), Client->GetGizmoManager().GetMode());
        }
    }
}

void UEditorEngine::SyncActiveGizmoVisualFromTarget()
{
    if (FEditorViewportClient *Client = GetActiveEditorViewportClient())
    {
        Client->GetGizmoManager().SyncVisualFromTarget();
    }
}

void UEditorEngine::RequestPlaySession(const FRequestPlaySessionParams &InParams)
{
    LevelEditor.GetPIEManager().RequestPlaySession(InParams);
}

void UEditorEngine::CancelRequestPlaySession() { LevelEditor.GetPIEManager().CancelRequestPlaySession(); }

void UEditorEngine::RequestEndPlayMap() { LevelEditor.GetPIEManager().RequestEndPlayMap(); }

bool UEditorEngine::TogglePIEControlMode() { return LevelEditor.GetPIEManager().TogglePIEControlMode(); }

// ─── 기존 메서드 ──────────────────────────────────────────

void UEditorEngine::ResetViewport() { GetViewportLayout().ResetViewport(GetWorld()); }

void UEditorEngine::CloseScene() { LevelEditor.GetSceneManager().CloseScene(); }

void UEditorEngine::NewScene() { LevelEditor.GetSceneManager().NewScene(); }

void UEditorEngine::ClearScene() { LevelEditor.GetSceneManager().ClearScene(); }

void UEditorEngine::BeginTrackedSceneChange()
{
    LevelEditor.GetHistoryManager().BeginTrackedSceneChange();
}

void UEditorEngine::CommitTrackedSceneChange()
{
    if (LevelEditor.GetHistoryManager().CommitTrackedSceneChange())
    {
        LevelEditorWindow.MarkActiveLevelDocumentDirty();
    }
}

void UEditorEngine::CancelTrackedSceneChange()
{
    LevelEditor.GetHistoryManager().CancelTrackedSceneChange();
}

bool UEditorEngine::CanUndoSceneChange() const
{
    return LevelEditor.GetHistoryManager().CanUndoSceneChange();
}

bool UEditorEngine::CanRedoSceneChange() const { return LevelEditor.GetHistoryManager().CanRedoSceneChange(); }

void UEditorEngine::UndoTrackedSceneChange()
{
    if (CanUndoSceneChange())
    {
        LevelEditor.GetHistoryManager().UndoTrackedSceneChange();
        LevelEditorWindow.MarkActiveLevelDocumentDirty();
    }
}

void UEditorEngine::RedoTrackedSceneChange()
{
    if (CanRedoSceneChange())
    {
        LevelEditor.GetHistoryManager().RedoTrackedSceneChange();
        LevelEditorWindow.MarkActiveLevelDocumentDirty();
    }
}

void UEditorEngine::ClearTrackedTransformHistory()
{
    LevelEditor.GetHistoryManager().ClearTrackedTransformHistory();
}

void UEditorEngine::BeginTrackedTransformChange() { LevelEditor.GetHistoryManager().BeginTrackedTransformChange(); }

void UEditorEngine::CommitTrackedTransformChange() { CommitTrackedSceneChange(); }

bool UEditorEngine::CanUndoTransformChange() const { return LevelEditor.GetHistoryManager().CanUndoTransformChange(); }

bool UEditorEngine::CanRedoTransformChange() const { return LevelEditor.GetHistoryManager().CanRedoTransformChange(); }

void UEditorEngine::UndoTrackedTransformChange()
{
    if (CanUndoTransformChange())
    {
        LevelEditor.GetHistoryManager().UndoTrackedTransformChange();
        LevelEditorWindow.MarkActiveLevelDocumentDirty();
    }
}

void UEditorEngine::RedoTrackedTransformChange()
{
    if (CanRedoTransformChange())
    {
        LevelEditor.GetHistoryManager().RedoTrackedTransformChange();
        LevelEditorWindow.MarkActiveLevelDocumentDirty();
    }
}

void UEditorEngine::ApplyTrackedSceneChange(const FTrackedSceneChange &Change, bool bRedo)
{
    LevelEditor.GetHistoryManager().ApplyTrackedSceneChange(Change, bRedo);
}

void UEditorEngine::ApplyTrackedActorDeltas(const FTrackedSceneChange &Change, bool bRedo)
{
    LevelEditor.GetHistoryManager().ApplyTrackedActorDeltas(Change, bRedo);
}

void UEditorEngine::RestoreTrackedActorOrder(const TArray<uint32> &OrderedUUIDs)
{
    LevelEditor.GetHistoryManager().RestoreTrackedActorOrder(OrderedUUIDs);
}

void UEditorEngine::RestoreTrackedFolderOrder(const TArray<FString> &OrderedFolders)
{
    LevelEditor.GetHistoryManager().RestoreTrackedFolderOrder(OrderedFolders);
}

void UEditorEngine::RestoreTrackedSelection(const TArray<uint32> &SelectedUUIDs)
{
    LevelEditor.GetHistoryManager().RestoreTrackedSelection(SelectedUUIDs);
}

void UEditorEngine::InvalidateTrackedSceneSnapshotCache() { LevelEditor.GetHistoryManager().InvalidateTrackedSceneSnapshotCache(); }

FEditorViewportCamera *UEditorEngine::FindSceneViewportCamera() const
{
    const FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout();
    if (!ActiveLayout)
    {
        return nullptr;
    }

    for (FLevelEditorViewportClient *VC : ActiveLayout->GetLevelViewportClients())
    {
        if (!VC)
        {
            continue;
        }

        if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective ||
            VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
        {
            return VC->GetCamera();
        }
    }

    return nullptr;
}

void UEditorEngine::RestoreViewportCamera(const FPerspectiveCameraData &CamData)
{
    if (!CamData.bValid)
    {
        return;
    }

    if (FEditorViewportCamera *Camera = FindSceneViewportCamera())
    {
        Camera->SetWorldLocation(CamData.Location);
        Camera->SetRelativeRotation(CamData.Rotation);
        FMinimalViewInfo CameraState = Camera->GetCameraState();
        CameraState.FOV = CamData.FOV;
        CameraState.NearZ = CamData.NearClip;
        CameraState.FarZ = CamData.FarClip;
        Camera->SetCameraState(CameraState);
    }
}

bool UEditorEngine::SaveSceneAs(const FString &InScenePath)
{
    const bool bSaved = LevelEditor.GetSceneManager().SaveSceneAs(InScenePath);
    if (bSaved)
    {
        LevelEditorWindow.MarkActiveLevelDocumentClean();
    }
    return bSaved;
}

bool UEditorEngine::SaveScene()
{
    const bool bSaved = LevelEditor.GetSceneManager().SaveScene();
    if (bSaved)
    {
        LevelEditorWindow.MarkActiveLevelDocumentClean();
    }
    return bSaved;
}

void UEditorEngine::RequestSaveSceneAsDialog() { LevelEditor.GetSceneManager().RequestSaveSceneAsDialog(); }

bool UEditorEngine::SaveSceneAsWithDialog()
{
    const bool bSaved = LevelEditor.GetSceneManager().SaveSceneAsWithDialog();
    if (bSaved)
    {
        LevelEditorWindow.MarkActiveLevelDocumentClean();
    }
    return bSaved;
}

bool UEditorEngine::LoadSceneFromPath(const FString &InScenePath) { return LevelEditor.GetSceneManager().LoadSceneFromPath(InScenePath); }

bool UEditorEngine::LoadSceneWithDialog() { return LevelEditor.GetSceneManager().LoadSceneWithDialog(); }

bool UEditorEngine::ImportAssetWithDialog() { return AssetImportManager.ImportAssetWithDialog(); }

bool UEditorEngine::ImportAssetFromPath(const FString& SourcePath, FString* OutImportedAssetPath) { return AssetImportManager.ImportAssetFromPath(SourcePath, OutImportedAssetPath); }

bool UEditorEngine::QueueImportAssetFromPath(const FString& SourcePath) { return AssetImportManager.QueueImportAssetFromPath(SourcePath); }

bool UEditorEngine::ImportMaterialWithDialog() { return AssetImportManager.ImportMaterialWithDialog(); }

bool UEditorEngine::ImportTextureWithDialog() { return AssetImportManager.ImportTextureWithDialog(); }
