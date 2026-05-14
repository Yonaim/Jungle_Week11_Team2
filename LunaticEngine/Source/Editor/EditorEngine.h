#pragma once

#include "AssetEditor/AssetEditorManager.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Platform/Paths.h"
#include "Engine/Serialization/SceneSaveManager.h"

#include "AssetTools/AssetImportManager.h"
#include "Common/UI/ImGui/EditorImGuiSystem.h"
#include "LevelEditor/LevelEditor.h"
#include "LevelEditor/PIE/LevelPIETypes.h"
#include "LevelEditor/Settings/LevelEditorSettings.h"
#include "LevelEditor/Window/LevelEditorWindow.h"
#include "Common/Viewport/EditorViewportCamera.h"
#include "Component/Gizmo/GizmoTypes.h"

#if STATS
#include "LevelEditor/Render/EditorRenderPipeline.h"
#endif

#include <filesystem>

class UGizmoVisualComponent;
class FLevelEditorViewportClient;
class FEditorViewportClient;
class FOverlayStatSystem;
class AActor;
class UGameViewportClient;
struct FPerspectiveCameraData;

enum class EEditorContextType
{
    LevelEditor,
    AssetEditor
};

class UEditorEngine : public UEngine
{
  public:
    DECLARE_CLASS(UEditorEngine, UEngine)

    UEditorEngine() = default;
    ~UEditorEngine() override = default;

    // 생명주기 오버라이드
    void Init(FWindowsWindow *InWindow) override;
    void Shutdown() override;
    void Tick(float DeltaTime) override;
    void OnWindowResized(uint32 Width, uint32 Height) override;
    bool LoadScene(const FString &InSceneReference) override;
    void OpenScoreSavePopup(int32 InScore) override;
    bool ConsumeScoreSavePopupResult(FString &OutNickname) override;
    void OpenMessagePopup(const FString &InMessage) override;
    bool ConsumeMessagePopupConfirmed() override;
    void OpenScoreboardPopup(const FString &InFilePath) override;
    void OpenTitleOptionsPopup() override;
    void OpenTitleCreditsPopup() override;
    bool IsScoreSavePopupOpen() const override;

    // 에디터 전용 API
    FEditorViewportCamera *GetCamera() const;
    bool              FocusActorInViewport(AActor *Actor);

    void SetActiveEditorContext(EEditorContextType InContextType);
    EEditorContextType GetActiveEditorContext() const { return ActiveEditorContextType; }
    bool IsLevelEditorContextActive() const { return ActiveEditorContextType == EEditorContextType::LevelEditor; }
    bool IsAssetEditorContextActive() const { return ActiveEditorContextType == EEditorContextType::AssetEditor; }
    FEditorViewportClient *GetActiveEditorViewportClient() const;
    bool IsMouseOverActiveViewport() const;

    void           ClearScene();
    void           ResetViewport();
    void           CloseScene();
    void           NewScene();
    bool           LoadSceneWithDialog();
    bool           LoadSceneFromPath(const FString &InScenePath);
    bool           ImportAssetWithDialog();
    bool           ImportAssetFromPath(const FString& SourcePath, FString* OutImportedAssetPath = nullptr);
    bool           QueueImportAssetFromPath(const FString& SourcePath);
    bool           ImportMaterialWithDialog();
    bool           ImportTextureWithDialog();
    bool           SaveScene();
    void           RequestSaveSceneAsDialog();
    bool           SaveSceneAsWithDialog();
    bool           SaveSceneAs(const FString &InScenePath);
    bool           HasCurrentLevelFilePath() const { return LevelEditor.GetSceneManager().HasCurrentLevelFilePath(); }
    const FString &GetCurrentLevelFilePath() const { return LevelEditor.GetSceneManager().GetCurrentLevelFilePath(); }
    void           RefreshContentBrowser() { LevelEditorWindow.RefreshContentBrowser(); }
    void           SelectContentBrowserPath(const std::filesystem::path& Path) { LevelEditorWindow.SelectContentBrowserPath(Path); }
    void           SelectContentBrowserPath(const FString& Path) { LevelEditorWindow.SelectContentBrowserPath(std::filesystem::path(FPaths::ToWide(Path))); }
    void           SetContentBrowserIconSize(float Size) { LevelEditorWindow.SetContentBrowserIconSize(Size); }
    float          GetContentBrowserIconSize() const { return LevelEditorWindow.GetContentBrowserIconSize(); }
    void           HideEditorWindows() { LevelEditorWindow.HideEditorWindows(); }
    void           ShowEditorWindows() { LevelEditorWindow.ShowEditorWindows(); }
    void           HideLevelEditorUIForAssetEditor() { LevelEditorWindow.HideLevelEditorUIForAssetEditor(); }
    void           RestoreLevelEditorUIAfterAssetEditor() { LevelEditorWindow.RestoreLevelEditorUIAfterAssetEditor(); }
    void           RequestDefaultDockLayout() { LevelEditorWindow.RequestDefaultDockLayout(); }
    void           SetShowEditorOnlyComponents(bool bEnable) { LevelEditorWindow.SetShowEditorOnlyComponents(bEnable); }
    bool           IsShowingEditorOnlyComponents() const { return LevelEditorWindow.IsShowingEditorOnlyComponents(); }
    bool           IsWorldCoordSystem() const { return FLevelEditorSettings::Get().CoordSystem == EEditorCoordSystem::World; }
    void           ToggleCoordSystem();
    void           SetEditorGizmoMode(EGizmoMode NewMode);
    EGizmoMode     GetEditorGizmoMode() const;
    void           ApplyTransformSettingsToGizmo();
    void           SyncActiveGizmoVisualFromTarget();
    void           BeginTrackedSceneChange();
    void           CommitTrackedSceneChange();
    void           CancelTrackedSceneChange();
    bool           CanUndoSceneChange() const;
    bool           CanRedoSceneChange() const;
    void           UndoTrackedSceneChange();
    void           RedoTrackedSceneChange();
    void           BeginTrackedTransformChange();
    void           CommitTrackedTransformChange();
    bool           CanUndoTransformChange() const;
    bool           CanRedoTransformChange() const;
    void           UndoTrackedTransformChange();
    void           RedoTrackedTransformChange();

    // GPU Occlusion readback 스테이징 데이터 무효화 — 액터 삭제 시 dangling proxy 방지
    void InvalidateOcclusionResults()
    {
        if (auto *P = GetRenderPipeline())
            P->OnSceneCleared();
    }

    FLevelEditorSettings       &GetSettings() { return FLevelEditorSettings::Get(); }
    const FLevelEditorSettings &GetSettings() const { return FLevelEditorSettings::Get(); }

    FSelectionManager       &GetSelectionManager() { return LevelEditor.GetSelectionManager(); }
    const FSelectionManager &GetSelectionManager() const { return LevelEditor.GetSelectionManager(); }

    FLevelViewportLayout       &GetViewportLayout() { return LevelEditor.GetViewportLayout(); }
    const FLevelViewportLayout &GetViewportLayout() const { return LevelEditor.GetViewportLayout(); }
    // 레이아웃에 위임
    // 다중 document tab 구조에서는 LevelEditor 내부 legacy ViewportLayout이 아니라
    // 현재 활성 Level document tab이 소유한 ViewportLayout을 사용해야 한다.
    const TArray<FEditorViewportClient *> &GetAllViewportClients() const
    {
        static const TArray<FEditorViewportClient *> EmptyClients;
        const FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout();
        return ActiveLayout ? ActiveLayout->GetAllViewportClients() : EmptyClients;
    }
    const TArray<FLevelEditorViewportClient *> &GetLevelViewportClients() const
    {
        static const TArray<FLevelEditorViewportClient *> EmptyClients;
        const FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout();
        return ActiveLayout ? ActiveLayout->GetLevelViewportClients() : EmptyClients;
    }
    bool ShouldRenderViewportClient(const FLevelEditorViewportClient *ViewportClient) const
    {
        const FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout();
        return IsLevelEditorContextActive() && ActiveLayout && ActiveLayout->ShouldRenderViewportClient(ViewportClient);
    }

    void SetActiveViewport(FLevelEditorViewportClient *InClient)
    {
        if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
        {
            ActiveLayout->SetActiveViewport(InClient);
        }
    }
    FLevelEditorViewportClient *GetActiveViewport() const
    {
        const FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout();
        return ActiveLayout ? ActiveLayout->GetActiveViewport() : nullptr;
    }

    void ToggleViewportSplit()
    {
        if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
        {
            ActiveLayout->ToggleViewportSplit();
        }
    }
    bool IsSplitViewport() const
    {
        const FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout();
        return ActiveLayout && ActiveLayout->IsSplitViewport();
    }

    void RenderViewportUI(float DeltaTime)
    {
        if (FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout())
        {
            ActiveLayout->RenderViewportUI(DeltaTime);
        }
    }
    AActor *SpawnPlaceActor(FLevelViewportLayout::EViewportPlaceActorType Type, const FVector &Location)
    {
        FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout();
        return ActiveLayout ? ActiveLayout->SpawnPlaceActor(Type, Location) : nullptr;
    }

    bool IsMouseOverViewport() const
    {
        const FLevelViewportLayout *ActiveLayout = LevelEditorWindow.GetActiveLevelViewportLayout();
        return ActiveLayout && ActiveLayout->IsMouseOverViewport();
    }

    void RenderUI(float DeltaTime);
    bool CanCloseEditorWithPrompt() const;
    void RenderPIEOverlayPopups();

    FOverlayStatSystem       &GetOverlayStatSystem() { return LevelEditor.GetOverlayStatSystem(); }
    const FOverlayStatSystem &GetOverlayStatSystem() const { return LevelEditor.GetOverlayStatSystem(); }

    // --- PIE (에디터 내 플레이) ---
    void RequestPlaySession(const FRequestPlaySessionParams &InParams);
    void CancelRequestPlaySession();
    bool HasPlaySessionRequest() const { return LevelEditor.GetPIEManager().HasPlaySessionRequest(); }

    void RequestEndPlayMap();
    bool IsPlayingInEditor() const { return LevelEditor.GetPIEManager().IsPlayingInEditor(); }
    EPIEControlMode GetPIEControlMode() const { return LevelEditor.GetPIEManager().GetPIEControlMode(); }
    bool            IsPIEPossessedMode() const { return LevelEditor.GetPIEManager().IsPIEPossessedMode(); }
    bool            IsPIEEjectedMode() const { return LevelEditor.GetPIEManager().IsPIEEjectedMode(); }
    bool            TogglePIEControlMode();

    // 즉시 동기 종료 — Save / NewScene / Load 등 에디터 월드를 만지는 작업 직전에 호출.
    // PIE 중이 아니면 no-op.
    void StopPlayInEditorImmediate()
    {
        LevelEditor.GetPIEManager().StopPlayInEditorImmediate();
    }

    bool OpenAssetFromPath(const std::filesystem::path &AssetPath)
    {
        return AssetEditorManager.OpenAssetFromPath(AssetPath);
    }


    FEditorImGuiSystem &GetImGuiSystem() { return ImGuiSystem; }
    const FEditorImGuiSystem &GetImGuiSystem() const { return ImGuiSystem; }

    bool IsAssetEditorCapturingInput() const { return AssetEditorManager.IsCapturingInput(); }
    void CollectAssetViewportClients(TArray<FEditorViewportClient *> &OutClients) const
    {
        if (IsAssetEditorContextActive())
        {
            AssetEditorManager.CollectViewportClients(OutClients);
        }
    }
    FAssetEditorManager &GetAssetEditorManager() { return AssetEditorManager; }
    const FAssetEditorManager &GetAssetEditorManager() const { return AssetEditorManager; }

  private:
    friend class FLevelSceneManager;
    friend class FLevelEditorHistoryManager;
    friend class FLevelPIEManager;

    FEditorViewportCamera *FindSceneViewportCamera() const;
    void              RestoreViewportCamera(const FPerspectiveCameraData &CamData);
    void              ClearTrackedTransformHistory();
    void              ApplyTrackedSceneChange(const FTrackedSceneChange &Change, bool bRedo);
    void              ApplyTrackedActorDeltas(const FTrackedSceneChange &Change, bool bRedo);
    void              RestoreTrackedActorOrder(const TArray<uint32> &OrderedUUIDs);
    void              RestoreTrackedFolderOrder(const TArray<FString> &OrderedFolders);
    void              RestoreTrackedSelection(const TArray<uint32> &SelectedUUIDs);
    void              InvalidateTrackedSceneSnapshotCache();

    EEditorContextType ActiveEditorContextType = EEditorContextType::LevelEditor;
    FLevelEditor       LevelEditor;
    FEditorImGuiSystem ImGuiSystem;
    FLevelEditorWindow LevelEditorWindow;
    FAssetEditorManager AssetEditorManager;
    FAssetImportManager AssetImportManager;
};
