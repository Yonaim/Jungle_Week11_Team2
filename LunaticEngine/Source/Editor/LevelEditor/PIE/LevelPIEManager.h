#pragma once

#include "Engine/Runtime/GameImGuiOverlay.h"
#include "LevelEditor/PIE/LevelPIETypes.h"
#include <optional>

class UEditorEngine;

/**
 * Level Editor의 Play In Editor 실행 상태를 관리한다.
 *
 * 역할:
 * - 현재 Editor World를 PIE World로 복제
 * - GameViewportClient 연결
 * - PIE possessed/ejected 모드 전환
 * - PIE 종료 시 Editor World / Selection / Camera 상태 복원
 * - PIE 전용 popup overlay 처리
 *
 * 주의:
 * - PIE는 현재 Level Editor가 편집 중인 World를 실행해보는 기능이므로 LevelEditor 소속이다.
 * - 독립 게임 실행기나 Standalone Game launcher가 아니다.
 */
class FLevelPIEManager
{
public:
    void Init(UEditorEngine* InEditorEngine);
    void Shutdown();
    void Tick(float DeltaTime);

    bool LoadScene(const FString& InSceneReference);

    void RenderOverlayPopups();

    void RequestPlaySession(const FRequestPlaySessionParams& InParams);
    void CancelRequestPlaySession();
    bool HasPlaySessionRequest() const;

    void RequestEndPlayMap();
    bool IsPlayingInEditor() const;
    EPIEControlMode GetPIEControlMode() const;
    bool IsPIEPossessedMode() const;
    bool IsPIEEjectedMode() const;
    bool TogglePIEControlMode();
    void StopPlayInEditorImmediate();

    void OpenScoreSavePopup(int32 InScore);
    bool ConsumeScoreSavePopupResult(FString& OutNickname);
    void OpenMessagePopup(const FString& InMessage);
    bool ConsumeMessagePopupConfirmed();
    void OpenScoreboardPopup(const FString& InFilePath);
    void OpenTitleOptionsPopup();
    void OpenTitleCreditsPopup();
    bool IsScoreSavePopupOpen() const;

private:
    void StartQueuedPlaySessionRequest();
    void StartPlayInEditorSession(const FRequestPlaySessionParams& Params);
    void EndPlayMap();
    bool EnterPIEPossessedMode();
    bool EnterPIEEjectedMode();
    void SyncGameViewportPIEControlState(bool bPossessedMode);

private:
    UEditorEngine* EditorEngine = nullptr;
    std::optional<FRequestPlaySessionParams> PlaySessionRequest;
    std::optional<FPlayInEditorSessionInfo> PlayInEditorSessionInfo;
    bool bRequestEndPlayMapQueued = false;
    EPIEControlMode PIEControlMode = EPIEControlMode::Possessed;
    FGameImGuiOverlay PIEOverlay;
};
