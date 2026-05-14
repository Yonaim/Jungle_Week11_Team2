#include "PCH/LunaticPCH.h"
#include "LevelEditor/UI/Toolbar/LevelPlayToolbar.h"

#include "Common/UI/Style/AccentColor.h"
#include "Common/UI/Toolbar/EditorPlayToolbar.h"
#include "EditorEngine.h"
#include "LevelEditor/PIE/LevelPIETypes.h"
#include "ImGui/imgui.h"
#include "Resource/ResourceManager.h"
#include "WICTextureLoader.h"


#include <d3d11.h>

void FLevelPlayToolbar::Init(UEditorEngine *InEditor, ID3D11Device *InDevice)
{
    Editor = InEditor;
    if (!InDevice)
        return;

    const FString PlayIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Play"));
    const FString PauseIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Pause"));
    const FString StopIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Stop"));
    const FString UndoIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Undo"));
    const FString RedoIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Redo"));

    DirectX::CreateWICTextureFromFile(InDevice, FPaths::ToWide(PlayIconPath).c_str(), nullptr, &PlayIcon);

    DirectX::CreateWICTextureFromFile(InDevice, FPaths::ToWide(PauseIconPath).c_str(), nullptr, &PauseIcon);

    DirectX::CreateWICTextureFromFile(InDevice, FPaths::ToWide(StopIconPath).c_str(), nullptr, &StopIcon);

    DirectX::CreateWICTextureFromFile(InDevice, FPaths::ToWide(UndoIconPath).c_str(), nullptr, &UndoIcon);

    DirectX::CreateWICTextureFromFile(InDevice, FPaths::ToWide(RedoIconPath).c_str(), nullptr, &RedoIcon);
}

void FLevelPlayToolbar::Release()
{
    if (PlayIcon)
    {
        PlayIcon->Release();
        PlayIcon = nullptr;
    }
    if (PauseIcon)
    {
        PauseIcon->Release();
        PauseIcon = nullptr;
    }
    if (StopIcon)
    {
        StopIcon->Release();
        StopIcon = nullptr;
    }
    if (UndoIcon)
    {
        UndoIcon->Release();
        UndoIcon = nullptr;
    }
    if (RedoIcon)
    {
        RedoIcon->Release();
        RedoIcon = nullptr;
    }
    Editor = nullptr;
}

void FLevelPlayToolbar::Render(float Width)
{
    if (!Editor)
        return;

    const bool bPlaying = Editor->IsPlayingInEditor();
    const bool bPaused = Editor->IsGamePaused();
    const bool bDisableHistory = bPlaying;
    const ImVec4 PlayTint = bPlaying ? ImVec4(1.0f, 1.0f, 1.0f, 0.7f) : ImVec4(0.30f, 0.90f, 0.35f, 1.0f);
    const ImVec4 PauseTint = (bPlaying && bPaused) ? UIAccentColor::Value : ImVec4(1.0f, 1.0f, 1.0f, bPlaying ? 1.0f : 0.7f);
    const ImVec4 StopTint = bPlaying ? ImVec4(0.95f, 0.28f, 0.25f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 0.7f);

    std::vector<FEditorPlayToolbarButtonDesc> PlayButtons = {
        {"##PIE_Play", PlayIcon, "Play", bPlaying, PlayTint, "Play", [this]() {
             FRequestPlaySessionParams Params;
             Editor->RequestPlaySession(Params);
         }},
        {"##PIE_Pause", PauseIcon, "Pause", !bPlaying, PauseTint, bPaused ? "Resume" : "Pause", [this]() {
             Editor->SetGamePaused(!Editor->IsGamePaused());
         }},
        {"##PIE_Stop", StopIcon, "Stop", !bPlaying, StopTint, "Stop", [this]() {
             Editor->RequestEndPlayMap();
             Editor->SetGamePaused(false);
         }},
    };

    std::vector<FEditorPlayToolbarButtonDesc> HistoryButtons = {
        {"##SceneUndo", UndoIcon, "Undo", bDisableHistory || !Editor->CanUndoTransformChange(),
         ImVec4(1.0f, 1.0f, 1.0f, bDisableHistory ? 0.35f : 0.9f), "Undo", [this]() { Editor->UndoTrackedTransformChange(); }},
        {"##SceneRedo", RedoIcon, "Redo", bDisableHistory || !Editor->CanRedoTransformChange(),
         ImVec4(1.0f, 1.0f, 1.0f, bDisableHistory ? 0.35f : 0.9f), "Redo", [this]() { Editor->RedoTrackedTransformChange(); }},
    };

    FEditorPlayToolbar::Render("##PlayToolbar", Width, PlayButtons, HistoryButtons, ToolbarHeight, IconSize, ButtonSpacing);
}
