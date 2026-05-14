#pragma once

#include "Core/CoreTypes.h"
#include "ImGui/imgui.h"

/**
 * 공통 EditorMenuBar에 메뉴 내용을 제공하는 인터페이스.
 *
 * 메뉴바 UI 자체는 Common/Menu/FEditorMenuBar가 담당하고,
 * File/Edit/Window/Custom 메뉴의 실제 항목은 각 Window 또는 Panel Manager가 제공한다.
 *
 * 예:
 * - FLevelEditorWindow: Level 저장/로드, PIE, 패널 표시 메뉴 제공
 * - FAssetEditorWindow: Open Asset, Save Active Tab 메뉴 제공
 */
class IEditorMenuProvider
{
  public:
    virtual ~IEditorMenuProvider() = default;

    virtual void BuildFileMenu() {}
    virtual void BuildEditMenu() {}
    virtual void BuildWindowMenu() {}
    virtual void BuildCustomMenus() {}

    virtual FString GetFrameTitle() const { return "Editor"; }
    virtual FString GetFrameTitleTooltip() const { return GetFrameTitle(); }
};
