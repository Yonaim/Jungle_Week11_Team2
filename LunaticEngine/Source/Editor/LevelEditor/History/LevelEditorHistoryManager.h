#pragma once

#include "LevelEditor/History/SceneHistoryTypes.h"
#include <optional>

class UEditorEngine;

/**
 * Level Editor 전용 Undo/Redo 기록 관리자.
 *
 * 현재 구현은 범용 Transaction System이 아니라 Scene Snapshot 기반이다.
 * 따라서 Actor 생성/삭제, Transform 변경, Outliner 순서, Selection 복원 같은
 * Level Editing 작업에 초점을 둔다.
 *
 * 나중에 AssetEditor별 Undo/Redo가 필요해지면 별도의 TransactionManager를 도입하는 것이 좋다.
 */
class FLevelEditorHistoryManager
{
public:
    void Init(UEditorEngine* InEditorEngine);
    void Shutdown();

    void BeginTrackedSceneChange();
    bool CommitTrackedSceneChange();
    void CancelTrackedSceneChange();

    bool CanUndoSceneChange() const;
    bool CanRedoSceneChange() const;
    void UndoTrackedSceneChange();
    void RedoTrackedSceneChange();

    void ClearTrackedTransformHistory();

    void BeginTrackedTransformChange();
    bool CommitTrackedTransformChange();
    bool CanUndoTransformChange() const;
    bool CanRedoTransformChange() const;
    void UndoTrackedTransformChange();
    void RedoTrackedTransformChange();

    void ApplyTrackedSceneChange(const FTrackedSceneChange& Change, bool bRedo);
    void ApplyTrackedActorDeltas(const FTrackedSceneChange& Change, bool bRedo);
    void RestoreTrackedActorOrder(const TArray<uint32>& OrderedUUIDs);
    void RestoreTrackedFolderOrder(const TArray<FString>& OrderedFolders);
    void RestoreTrackedSelection(const TArray<uint32>& SelectedUUIDs);
    void InvalidateTrackedSceneSnapshotCache();

private:
    UEditorEngine* EditorEngine = nullptr;

    TArray<FTrackedSceneChange> SceneHistory;
    int32 SceneHistoryCursor = -1;
    std::optional<FTrackedSceneSnapshot> PendingTrackedSceneBefore;
    std::optional<FTrackedSceneSnapshot> CachedTrackedSceneSnapshot;
    bool bTrackingSceneChange = false;
};
