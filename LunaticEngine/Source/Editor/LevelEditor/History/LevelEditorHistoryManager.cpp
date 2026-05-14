#include "PCH/LunaticPCH.h"
#include "LevelEditor/History/LevelEditorHistoryManager.h"

#include "Component/CameraComponent.h"
#include "Component/GizmoVisualComponent.h"
#include "EditorEngine.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Level.h"
#include "GameFramework/World.h"
#include "LevelEditor/History/SceneHistoryBuilder.h"
#include "LevelEditor/Selection/SelectionManager.h"
#include "Object/Object.h"
#include <set>

void FLevelEditorHistoryManager::Init(UEditorEngine* InEditorEngine)
{
    EditorEngine = InEditorEngine;
}

void FLevelEditorHistoryManager::Shutdown()
{
    ClearTrackedTransformHistory();
    EditorEngine = nullptr;
}

void FLevelEditorHistoryManager::BeginTrackedSceneChange()
{
    if (!EditorEngine || bTrackingSceneChange || EditorEngine->IsPlayingInEditor())
    {
        return;
    }

    FTrackedSceneSnapshot Snapshot;
    if (CachedTrackedSceneSnapshot.has_value())
    {
        Snapshot = *CachedTrackedSceneSnapshot;
    }
    else
    {
        Snapshot = FSceneHistoryBuilder::CaptureSnapshot(*EditorEngine);
    }

    PendingTrackedSceneBefore = Snapshot;
    bTrackingSceneChange = true;
}

bool FLevelEditorHistoryManager::CommitTrackedSceneChange()
{
    if (!EditorEngine || !bTrackingSceneChange || !PendingTrackedSceneBefore.has_value())
    {
        return false;
    }

    const FTrackedSceneSnapshot Before = *PendingTrackedSceneBefore;
    const FTrackedSceneSnapshot After = FSceneHistoryBuilder::CaptureSnapshot(*EditorEngine);

    PendingTrackedSceneBefore.reset();
    bTrackingSceneChange = false;
    CachedTrackedSceneSnapshot = After;

    if (!FSceneHistoryBuilder::HasMeaningfulDelta(Before, After))
    {
        return false;
    }

    const FTrackedSceneChange Change = FSceneHistoryBuilder::BuildChange(Before, After);

    if (SceneHistoryCursor + 1 < static_cast<int32>(SceneHistory.size()))
    {
        SceneHistory.erase(SceneHistory.begin() + (SceneHistoryCursor + 1), SceneHistory.end());
    }

    SceneHistory.push_back(Change);
    if (SceneHistory.size() > 10)
    {
        SceneHistory.erase(SceneHistory.begin());
    }
    SceneHistoryCursor = static_cast<int32>(SceneHistory.size()) - 1;
    return true;
}

void FLevelEditorHistoryManager::CancelTrackedSceneChange()
{
    PendingTrackedSceneBefore.reset();
    bTrackingSceneChange = false;
}

bool FLevelEditorHistoryManager::CanUndoSceneChange() const
{
    return SceneHistoryCursor >= 0 && SceneHistoryCursor < static_cast<int32>(SceneHistory.size());
}

bool FLevelEditorHistoryManager::CanRedoSceneChange() const
{
    return SceneHistoryCursor + 1 < static_cast<int32>(SceneHistory.size());
}

void FLevelEditorHistoryManager::UndoTrackedSceneChange()
{
    if (!CanUndoSceneChange())
    {
        return;
    }

    const FTrackedSceneChange& Change = SceneHistory[SceneHistoryCursor];
    ApplyTrackedSceneChange(Change, false);
    --SceneHistoryCursor;
}

void FLevelEditorHistoryManager::RedoTrackedSceneChange()
{
    if (!CanRedoSceneChange())
    {
        return;
    }

    const int32 RedoIndex = SceneHistoryCursor + 1;
    const FTrackedSceneChange& Change = SceneHistory[RedoIndex];
    ApplyTrackedSceneChange(Change, true);
    SceneHistoryCursor = RedoIndex;
}

void FLevelEditorHistoryManager::ClearTrackedTransformHistory()
{
    SceneHistory.clear();
    SceneHistoryCursor = -1;
    PendingTrackedSceneBefore.reset();
    CachedTrackedSceneSnapshot.reset();
    bTrackingSceneChange = false;
}

void FLevelEditorHistoryManager::BeginTrackedTransformChange()
{
    BeginTrackedSceneChange();
}

bool FLevelEditorHistoryManager::CommitTrackedTransformChange()
{
    return CommitTrackedSceneChange();
}

bool FLevelEditorHistoryManager::CanUndoTransformChange() const
{
    return CanUndoSceneChange();
}

bool FLevelEditorHistoryManager::CanRedoTransformChange() const
{
    return CanRedoSceneChange();
}

void FLevelEditorHistoryManager::UndoTrackedTransformChange()
{
    UndoTrackedSceneChange();
}

void FLevelEditorHistoryManager::RedoTrackedTransformChange()
{
    RedoTrackedSceneChange();
}

void FLevelEditorHistoryManager::ApplyTrackedSceneChange(const FTrackedSceneChange& Change, bool bRedo)
{
    if (!EditorEngine)
    {
        return;
    }

    EditorEngine->GetSelectionManager().ClearSelection();
    ApplyTrackedActorDeltas(Change, bRedo);
    RestoreTrackedActorOrder(bRedo ? Change.AfterActorOrderUUIDs : Change.BeforeActorOrderUUIDs);
    RestoreTrackedFolderOrder(bRedo ? Change.AfterOutlinerFolders : Change.BeforeOutlinerFolders);
    if (UWorld* World = EditorEngine->GetWorld())
    {
        World->WarmupPickingData();
    }
    EditorEngine->RestoreViewportCamera(bRedo ? Change.AfterCameraData : Change.BeforeCameraData);

    TArray<uint32> PreferredSelection = FSceneHistoryBuilder::GetChangedActorUUIDs(Change, bRedo);
    if (PreferredSelection.empty())
    {
        PreferredSelection = bRedo ? Change.AfterSelectedActorUUIDs : Change.BeforeSelectedActorUUIDs;
    }
    RestoreTrackedSelection(PreferredSelection);
    CachedTrackedSceneSnapshot = FSceneHistoryBuilder::CaptureSnapshot(*EditorEngine);
}

void FLevelEditorHistoryManager::ApplyTrackedActorDeltas(const FTrackedSceneChange& Change, bool bRedo)
{
    if (!EditorEngine)
    {
        return;
    }

    UWorld* World = EditorEngine->GetWorld();
    if (!World)
    {
        return;
    }

    if (bRedo)
    {
        for (const FDeletedActorDelta& Delta : Change.DeletedActors)
        {
            AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
            if (ExistingActor)
            {
                World->DestroyActor(ExistingActor);
            }
        }

        for (const FCreatedActorDelta& Delta : Change.CreatedActors)
        {
            AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
            if (!ExistingActor)
            {
                FSceneSaveManager::LoadActorFromJSONString(Delta.SerializedActor, World);
            }
        }

        for (const FModifiedActorDelta& Delta : Change.ModifiedActors)
        {
            AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
            if (ExistingActor)
            {
                if (!FSceneSaveManager::ApplyActorFromJSONString(ExistingActor, Delta.AfterSerializedActor))
                {
                    World->DestroyActor(ExistingActor);
                    FSceneSaveManager::LoadActorFromJSONString(Delta.AfterSerializedActor, World);
                }
            }
            else
            {
                FSceneSaveManager::LoadActorFromJSONString(Delta.AfterSerializedActor, World);
            }
        }
        return;
    }

    for (const FCreatedActorDelta& Delta : Change.CreatedActors)
    {
        AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
        if (ExistingActor)
        {
            World->DestroyActor(ExistingActor);
        }
    }

    for (const FDeletedActorDelta& Delta : Change.DeletedActors)
    {
        AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
        if (!ExistingActor)
        {
            FSceneSaveManager::LoadActorFromJSONString(Delta.SerializedActor, World);
        }
    }

    for (const FModifiedActorDelta& Delta : Change.ModifiedActors)
    {
        AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(Delta.ActorUUID));
        if (ExistingActor)
        {
            if (!FSceneSaveManager::ApplyActorFromJSONString(ExistingActor, Delta.BeforeSerializedActor))
            {
                World->DestroyActor(ExistingActor);
                FSceneSaveManager::LoadActorFromJSONString(Delta.BeforeSerializedActor, World);
            }
        }
        else
        {
            FSceneSaveManager::LoadActorFromJSONString(Delta.BeforeSerializedActor, World);
        }
    }
}

void FLevelEditorHistoryManager::RestoreTrackedActorOrder(const TArray<uint32>& OrderedUUIDs)
{
    if (!EditorEngine)
    {
        return;
    }

    UWorld* World = EditorEngine->GetWorld();
    ULevel* PersistentLevel = World ? World->GetPersistentLevel() : nullptr;
    if (!World || !PersistentLevel || OrderedUUIDs.empty())
    {
        return;
    }

    std::set<uint32> OrderedUUIDSet(OrderedUUIDs.begin(), OrderedUUIDs.end());
    size_t PrefixCount = 0;
    for (AActor* Actor : PersistentLevel->GetActors())
    {
        if (!Actor || OrderedUUIDSet.find(Actor->GetUUID()) != OrderedUUIDSet.end())
        {
            break;
        }
        ++PrefixCount;
    }

    for (size_t Index = 0; Index < OrderedUUIDs.size(); ++Index)
    {
        AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(OrderedUUIDs[Index]));
        if (!Actor)
        {
            continue;
        }

        World->MoveActorToIndex(Actor, PrefixCount + Index);
    }
}

void FLevelEditorHistoryManager::RestoreTrackedFolderOrder(const TArray<FString>& OrderedFolders)
{
    if (!EditorEngine)
    {
        return;
    }

    UWorld* World = EditorEngine->GetWorld();
    ULevel* PersistentLevel = World ? World->GetPersistentLevel() : nullptr;
    if (!PersistentLevel)
    {
        return;
    }

    PersistentLevel->SetOutlinerFolders(OrderedFolders);
}

void FLevelEditorHistoryManager::RestoreTrackedSelection(const TArray<uint32>& SelectedUUIDs)
{
    if (!EditorEngine)
    {
        return;
    }

    TArray<AActor*> RestoredSelection;
    for (uint32 SelectedUUID : SelectedUUIDs)
    {
        if (AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(SelectedUUID)))
        {
            RestoredSelection.push_back(Actor);
        }
    }

    if (!RestoredSelection.empty())
    {
        EditorEngine->GetSelectionManager().SelectActors(RestoredSelection);
    }
    else
    {
        EditorEngine->GetSelectionManager().ClearSelection();
    }

    EditorEngine->SyncActiveGizmoVisualFromTarget();
}

void FLevelEditorHistoryManager::InvalidateTrackedSceneSnapshotCache()
{
    CachedTrackedSceneSnapshot.reset();
}
