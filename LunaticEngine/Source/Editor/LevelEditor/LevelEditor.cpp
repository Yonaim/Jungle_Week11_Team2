#include "PCH/LunaticPCH.h"
#include "LevelEditor/LevelEditor.h"
#include "EditorEngine.h"


void FLevelEditor::Initialize(UEditorEngine *InEditorEngine, FWindowsWindow *InWindow, FRenderer &InRenderer)
{
    EditorEngine = InEditorEngine;

    SelectionManager.Init();
    SelectionManager.SetWorld(EditorEngine->GetWorld());

    ViewportLayout.Init(EditorEngine, InWindow, InRenderer, &SelectionManager);

    ViewportLayout.LoadFromSettings();
    PIEManager.Init(EditorEngine);
    HistoryManager.Init(EditorEngine);
    SceneManager.Init(EditorEngine);
}

void FLevelEditor::Tick(float DeltaTime)
{
    PIEManager.Tick(DeltaTime);
    SceneManager.Tick(DeltaTime);
}

void FLevelEditor::Shutdown()
{
    SceneManager.Shutdown();
    HistoryManager.Shutdown();
    PIEManager.Shutdown();
    SelectionManager.Shutdown();
    ViewportLayout.Release();
    EditorEngine = nullptr;
}
