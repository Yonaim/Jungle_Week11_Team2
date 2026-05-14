#pragma once

#include <filesystem>

#include "Core/CoreTypes.h"

#include "ImGui/imgui.h"

class UObject;
class UEditorEngine;
class FRenderer;
class FEditorViewportClient;

/**
 * 모든 에셋 에디터의 공통 인터페이스.
 *
 * 이 인터페이스는 Window나 Tab 자체가 아니라, "탭/패널 내부에 그려질 편집기 내용"을 의미한다.
 * 예:
 * - FCameraModifierStackEditor
 * - FSkeletalMeshEditor
 *
 * 기본 에디터는 하나의 도킹 가능한 패널 안에 RenderContent()를 그린다.
 * SkeletalMeshEditor처럼 Toolbar / Preview / Skeleton Tree / Details를 각각 별도 패널로 나누고 싶으면
 * UsesExternalPanels()에서 true를 반환하고 RenderPanels()를 직접 구현한다.
 */
class IAssetEditor
{
  public:
    virtual ~IAssetEditor() = default;

    virtual void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer) = 0;

    virtual bool OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath) = 0;
    virtual void Close() = 0;
    virtual bool Save() = 0;

    virtual void Tick(float DeltaTime) {}

    /** 단일 패널형 에디터의 본문 렌더링. */
    virtual void RenderContent(float DeltaTime) = 0;

    virtual bool UsesExternalPanels() const { return false; }

    // Asset Editor가 Level Editor DockSpace 안에 임시로 붙는 구조에서는,
    // Level Editor로 돌아갔다가 다시 Asset Editor를 보여줄 때 기존 dock node가 재사용되어
    // 패널 배치가 깨질 수 있다. 다시 활성화될 때 각 에디터가 자기 layout을 재빌드하도록 한다.
    virtual void InvalidateDockLayout() {}

    // Document tab/context activation hooks.
    // Use these to detach transient viewport/gizmo targets when an editor tab is hidden,
    // so a stale target from an inactive tab cannot receive later input/render sync.
    virtual void OnActivated() {}
    virtual void OnDeactivated() {}

    virtual void RenderPanels(float DeltaTime, ImGuiID DockspaceId)
    {
        (void)DockspaceId;
        RenderContent(DeltaTime);
    }

    // 공통 메뉴바에서 활성 에디터가 자기 전용 메뉴를 추가할 수 있도록 둔 hook.
    virtual void BuildFileMenu() {}
    virtual void BuildEditMenu() {}
    virtual void BuildWindowMenu() {}
    virtual void BuildCustomMenus() {}

    // Asset Editor 공통 history hook.
    // 개별 에셋 에디터가 실제 편집 가능한 상태만 undo/redo snapshot으로 관리한다.
    virtual bool CanUndo() const { return false; }
    virtual bool CanRedo() const { return false; }
    virtual void Undo() {}
    virtual void Redo() {}

    virtual bool IsDirty() const = 0;
    virtual bool IsCapturingInput() const = 0;

    /** Asset Editor 내부에 있는 렌더 가능한 Preview Viewport들을 수집한다. */
    virtual void CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) { (void)OutClients; }
    virtual FEditorViewportClient *GetActiveViewportClient() { return nullptr; }

    virtual const char *GetEditorName() const = 0;
    virtual const std::filesystem::path &GetAssetPath() const = 0;

    /** Document tab strip에 표시할 에셋 타입 아이콘. */
    virtual const char *GetDocumentTabIconKey() const { return "Editor.Icon.Panel.Asset"; }
    virtual ImVec4 GetDocumentTabIconTint() const { return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); }
};
