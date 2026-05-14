#pragma once

#include "MainFrame/EditorMainMenuBar.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;
class IEditorMenuProvider;

// FEditorImGuiSystem = Editor 전체 UI 런타임
// FEditorMainFrame  = 그 UI 런타임 위에서 그려지는 최상위 화면


/**
 * Editor 최상위 UI 프레임을 담당한다.
 *
 * 역할:
 * - 메인 메뉴바, 공통 오버레이, 프로젝트 설정 팝업을 렌더링한다.
 * - Level Editor / Asset Editor가 공유하는 최상위 UI 컨테이너 역할만 수행한다.
 *
 * 주의:
 * - ImGui Context, Win32 backend, DX11 backend는 소유하지 않는다.
 * - ImGui 생명주기는 FEditorImGuiSystem이 단독으로 관리한다.
 */
class FEditorMainFrame
{
  public:
    void Create(FWindowsWindow *InWindow, FRenderer &InRenderer, UEditorEngine *InEditorEngine);
    void Release();

    // ImGui 프레임 생명주기는 FEditorImGuiSystem이 관리한다. 여기서는 UI 루트 상태만 준비한다.
    void BeginFrame();
    void RenderMainMenuBar(IEditorMenuProvider *MenuProvider);
    void RenderCommonOverlays();
    void EndFrame();

    void UpdateInputState(bool bMouseOverViewport, bool bAssetEditorCapturingInput, bool bPIEPopupOpen);

    bool HasBlockingOverlayOpen() const;
    void ShowProjectSettings() { bShowProjectSettings = true; }
    void ToggleShortcutOverlay() { bShowShortcutOverlay = !bShowShortcutOverlay; }
    void ToggleCreditsOverlay() { bShowCreditsOverlay = !bShowCreditsOverlay; }

  private:
    void MakeCurrentContext() const;
    void RenderProjectSettingsWindow();
    void RenderShortcutOverlay();
    void RenderCreditsOverlay();

  private:
    FWindowsWindow    *Window = nullptr;
    UEditorEngine     *EditorEngine = nullptr;
    FEditorMainMenuBar MainMenuBar;
    bool               bShowProjectSettings = false;
    bool               bShowShortcutOverlay = false;
    bool               bShowCreditsOverlay = false;
};
