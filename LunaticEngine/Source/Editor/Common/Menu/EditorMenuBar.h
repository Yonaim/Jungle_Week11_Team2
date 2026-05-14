#pragma once

#include <functional>

class FWindowsWindow;
class UEditorEngine;
class IEditorMenuProvider;
struct ImFont;

/**
 * 공통 Editor 메뉴바 렌더링에 필요한 컨텍스트.
 *
 * 같은 메뉴바 UI를 LevelEditorWindow와 AssetEditor 패널 양쪽에서 재사용하기 위해
 * 메뉴 항목은 IEditorMenuProvider로 위임하고, 창 제어 버튼은 callback으로 분리한다.
 */
struct FEditorMenuBarContext
{
    const char *Id = "##EditorMenuBar";

    FWindowsWindow      *Window = nullptr;
    UEditorEngine       *EditorEngine = nullptr;
    IEditorMenuProvider *MenuProvider = nullptr;

    ImFont *TitleBarFont = nullptr;
    ImFont *WindowControlIconFont = nullptr;

    bool bShowProjectSettingsMenu = false;

    // Window마다 닫기/최대화/최소화의 의미가 다를 수 있으므로 직접 Window 함수를 호출하지 않고 callback으로 처리한다.
    std::function<void()> OnMinimizeWindow;
    std::function<void()> OnToggleMaximizeWindow;
    std::function<void()> OnCloseWindow;
    std::function<void()> OnOpenProjectSettings;
    std::function<void()> OnToggleShortcutOverlay;
    std::function<void()> OnOpenCredits;
};

/**
 * Editor 공통 메뉴바 UI.
 *
 * 주의:
 * - 이 클래스는 LevelEditor나 AssetEditor의 구체 기능을 알지 않는다.
 * - 메뉴 내용은 IEditorMenuProvider가 제공한다.
 * - 그래서 LevelEditorWindow와 AssetEditor 패널이 같은 메뉴바 스타일을 공유할 수 있다.
 */
class FEditorMenuBar
{
  public:
    void Render(const FEditorMenuBarContext &Context);
};
