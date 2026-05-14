#pragma once

#include "Core/CoreTypes.h"

class UEditorEngine;

/**
 * Level Editor가 편집하는 Scene/Level 파일을 관리한다.
 *
 * 역할:
 * - New / Clear / Close Scene
 * - Level 파일 로드/저장
 * - 시작 Level 자동 로드
 * - 현재 Level 파일 경로 보관
 *
 * 주의:
 * - 이 클래스는 Level Editor 전용 Scene 관리 객체다.
 * - 게임 런타임의 Level Loading 시스템이나 Asset Editor Preview Scene과는 다르다.
 */
class FLevelSceneManager
{
public:
    void Init(UEditorEngine* InEditorEngine);
    void Shutdown();

    void Tick(float DeltaTime);

    void CloseScene();
    void NewScene();
    void ClearScene();

    void LoadStartLevel();

    bool LoadSceneFromPath(const FString& InScenePath);
    bool LoadSceneWithDialog();

    bool SaveScene();
    bool SaveSceneAs(const FString& InScenePath);
    bool SaveSceneAsWithDialog();
    void RequestSaveSceneAsDialog();

    bool HasCurrentLevelFilePath() const
    {
        return !CurrentLevelFilePath.empty();
    }

    const FString& GetCurrentLevelFilePath() const
    {
        return CurrentLevelFilePath;
    }

private:
    void ProcessDeferredActions();
    void DestroyCurrentSceneWorlds(bool bClearHistory, bool bResetLevelPath);

private:
    UEditorEngine* EditorEngine = nullptr;
    bool bRequestSaveSceneAsDialogQueued = false;
    FString CurrentLevelFilePath;
};
