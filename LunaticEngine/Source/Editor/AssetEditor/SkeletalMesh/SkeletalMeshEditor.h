#pragma once

#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"
#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"
#include "AssetEditor/Common/UI/AssetDetailsPanel.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshDetailsPanel.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshPreviewViewport.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletonTreePanel.h"
#include "Common/UI/Panels/Panel.h"

#include <filesystem>
#include <memory>
#include <string>

#include "ImGui/imgui.h"

class UObject;
class UEditorEngine;
class FRenderer;
class USkeletalMesh;
class USkeletalMeshComponent;
class FSkeletalMeshPreviewPoseController;

/**
 * SkeletalMesh Viewer / Asset Editor.
 *
 * 패널 구성:
 * - Toolbar Panel
 * - Preview Viewport Panel
 * - Skeleton Tree Panel
 * - Details Panel
 *
 * 구현 규칙:
 * - 각 영역은 실제 독립 패널로 렌더링한다.
 * - Level Editor 패널과 같은 FPanel wrapper를 사용해 title/icon/inset 스타일을 통일한다.
 * - Preview Viewport는 FEditorViewportClient 베이스를 재사용한다.
 */
class FSkeletalMeshEditor final : public IAssetEditor
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer) override;
    bool OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath) override;
    void Close() override;
    bool Save() override;

    void Tick(float DeltaTime) override;
    void RenderContent(float DeltaTime) override;

    // SkeletalMeshEditor는 단일 탭 내부 content가 아니라 여러 도킹 패널을 직접 렌더링한다.
    bool UsesExternalPanels() const override { return true; }
    void InvalidateDockLayout() override;
    void OnActivated() override;
    void OnDeactivated() override;
    void RenderPanels(float DeltaTime, ImGuiID DockspaceId) override;
    void BuildWindowMenu() override;
    void BuildCustomMenus() override;

    bool CanUndo() const override;
    bool CanRedo() const override;
    void Undo() override;
    void Redo() override;

    bool IsDirty() const override { return bDirty; }
    bool IsCapturingInput() const override { return bCapturingInput; }
    FEditorViewportClient *GetActiveViewportClient() override;
    void CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) override;
    const char *GetEditorName() const override { return "Skeletal Mesh"; }
    const std::filesystem::path &GetAssetPath() const override { return EditingAssetPath; }
    const char *GetDocumentTabIconKey() const override { return "Editor.Icon.SkeletalMeshActor"; }
    ImVec4 GetDocumentTabIconTint() const override { return ImVec4(1.0f, 0.42f, 0.78f, 1.0f); }

  private:
    struct FHistoryState
    {
        TArray<FString> MaterialPaths;
    };

    void RenderPanelsInternal(float DeltaTime, ImGuiID DockspaceId);
    void BuildDefaultDockLayout(ImGuiID DockspaceId);
    void RenderPreviewerSettingsPanel(const FPanelDesc& Desc);
    bool ShouldDisablePreviewForAsset(const std::filesystem::path& AssetPath) const;
    bool CanUsePreviewViewport() const;
    void RenderPreviewDisabledPanel(const FPanelDesc& Desc) const;

    FHistoryState CaptureHistoryState() const;
    void ApplyHistoryState(const FHistoryState &HistoryState);
    bool AreHistoryStatesEqual(const FHistoryState &A, const FHistoryState &B) const;
    void PushUndoStateIfChanged(const FHistoryState &BeforeState, const FHistoryState &AfterState);
    USkeletalMeshComponent *GetPreviewComponent() const;

    std::string MakePanelStableId(const char *PanelName) const;
    FPanelDesc MakePanelDesc(const char *DisplayName, const char *StableName, const char *IconKey,
                                   ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse) const;

  private:
    UEditorEngine        *EditorEngine = nullptr;
    FRenderer            *Renderer = nullptr;
    USkeletalMesh        *EditingAsset = nullptr;
    std::filesystem::path EditingAssetPath;

    // 에디터 전체 공유 상태. 패널들이 이 상태를 같이 보고 갱신한다.
    FSkeletalMeshEditorState State;

    FSkeletalMeshEditorToolbar Toolbar;
    FSkeletalMeshPreviewViewport PreviewViewport;
    FSkeletonTreePanel SkeletonTreePanel;
    FAssetDetailsPanel AssetDetailsPanel;
    FSkeletalMeshDetailsPanel DetailsPanel;
    std::shared_ptr<FSkeletalMeshPreviewPoseController> PoseController;

    // Skeleton Tree / Asset Details / Preview Viewport가 공유하는 Bone 선택 관리자.
    FSkeletalMeshSelectionManager SelectionManager;

    TArray<FHistoryState> UndoStack;
    TArray<FHistoryState> RedoStack;

    bool bOpen = false;
    bool bDirty = false;

    // 패널별 close button 상태. FBX를 다시 열면 기본 레이아웃과 함께 모두 열린 상태로 복구한다.
    bool bPreviewPanelOpen = true;
    bool bSkeletonTreePanelOpen = true;
    bool bDetailsPanelOpen = true;
    bool bBoneDetailsPanelOpen = true;
    bool bPreviewerSettingsPanelOpen = true;

    // 동일 타입 에디터를 여러 개 열었을 때 ImGui ID 충돌을 피하기 위한 인스턴스 ID.
    uint32 EditorInstanceId = 0;
    ImGuiID BuiltDockspaceId = 0;

    bool bCapturingInput = false;
    bool bIsActiveTab = false;
    bool bDisablePreviewForCurrentAsset = false;
};
