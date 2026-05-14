#pragma once

#include "LevelEditor/History/LevelEditorHistoryManager.h"
#include "LevelEditor/Selection/SelectionManager.h"
#include "LevelEditor/PIE/LevelPIEManager.h"
#include "LevelEditor/Scene/LevelSceneManager.h"
#include "LevelEditor/Subsystem/OverlayStatSystem.h"
#include "LevelEditor/Viewport/LevelViewportLayout.h"

class UEditorEngine;
class FWindowsWindow;
class FRenderer;
class FEditorViewportClient;
class FLevelEditorViewportClient;

/**
 * Level Editor의 로직/상태를 묶는 루트 객체.
 *
 * 이 클래스는 Window UI 자체를 그리지 않는다.
 * UI 렌더링은 FLevelEditorWindow가 담당하고,
 * 여기서는 Level 편집에 필요한 시스템들을 소유한다.
 *
 * 소유 시스템:
 * - SceneManager: Level 생성/저장/로드/삭제
 * - PIEManager: Play In Editor 시작/종료/오버레이
 * - HistoryManager: Scene/Transform Undo/Redo
 * - SelectionManager: Actor/Component 선택 상태 관리
 * - ViewportLayout: Level Viewport 생성/배치
 * - OverlayStatSystem: Viewport 통계 표시용 데이터
 */
class FLevelEditor
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FWindowsWindow *InWindow, FRenderer &InRenderer);
    void Shutdown();
    void Tick(float DeltaTime);

    FSelectionManager &GetSelectionManager() { return SelectionManager; }
    const FSelectionManager &GetSelectionManager() const { return SelectionManager; }

    FLevelViewportLayout &GetViewportLayout() { return ViewportLayout; }
    const FLevelViewportLayout &GetViewportLayout() const { return ViewportLayout; }

    FOverlayStatSystem &GetOverlayStatSystem() { return OverlayStatSystem; }
    const FOverlayStatSystem &GetOverlayStatSystem() const { return OverlayStatSystem; }

    FLevelPIEManager& GetPIEManager() { return PIEManager; }
    const FLevelPIEManager& GetPIEManager() const { return PIEManager; }

    FLevelEditorHistoryManager& GetHistoryManager() { return HistoryManager; }
    const FLevelEditorHistoryManager& GetHistoryManager() const { return HistoryManager; }

    FLevelSceneManager& GetSceneManager() { return SceneManager; }
    const FLevelSceneManager& GetSceneManager() const { return SceneManager; }

  private:
    UEditorEngine       *EditorEngine = nullptr;
    FSelectionManager    SelectionManager;
    FLevelViewportLayout ViewportLayout;
    FOverlayStatSystem   OverlayStatSystem;
    FLevelPIEManager     PIEManager;
    FLevelEditorHistoryManager HistoryManager;
    FLevelSceneManager   SceneManager;
};
