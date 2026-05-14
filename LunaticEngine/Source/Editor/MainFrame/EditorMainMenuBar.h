#pragma once

class FWindowsWindow;
class UEditorEngine;
class IEditorMenuProvider;
struct ImFont;

/**
 * Editor MainFrame 메뉴바 렌더링에 필요한 컨텍스트.
 *
 * MainFrame 전용 메뉴바가 Window 제어 버튼, 공통 오버레이 토글,
 * 메뉴 제공자 정보를 한 번에 받을 수 있도록 묶어둔다.
 */
struct FEditorMainMenuBarContext
{
    FWindowsWindow *Window = nullptr;
    UEditorEngine *EditorEngine = nullptr;
    IEditorMenuProvider *MenuProvider = nullptr;
    ImFont *TitleBarFont = nullptr;
    ImFont *WindowControlIconFont = nullptr;
    bool *bShowProjectSettings = nullptr;
    bool *bShowShortcutOverlay = nullptr;
    bool *bShowCreditsOverlay = nullptr;
};

/**
 * Editor MainFrame 상단 메뉴바를 렌더링한다.
 *
 * ImGui 생명주기는 소유하지 않으며, 이미 열린 ImGui frame 안에서
 * 메뉴바 UI만 그리는 사용자 클래스다.
 */
class FEditorMainMenuBar
{
  public:
    void Render(const FEditorMainMenuBarContext &Context);
};
