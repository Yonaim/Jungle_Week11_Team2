#pragma once

#include "AssetEditor/IAssetEditor.h"
#include "Common/UI/Panels/Panel.h"
#include "Core/CoreTypes.h"

#include <filesystem>
#include <functional>
#include <string>

class UAssetData;
class UObject;
class UCameraModifierStackAssetData;
class UEditorEngine;
class FRenderer;
struct FAssetBezierCurve;
struct FCameraShakeModifierAssetDesc;

class FCameraModifierStackEditor final : public IAssetEditor
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer) override;
    bool OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath) override;
    void Close() override;
    bool Save() override;

    void RenderContent(float DeltaTime) override;

    bool UsesExternalPanels() const override { return true; }
    void InvalidateDockLayout() override;
    void OnActivated() override;
    void OnDeactivated() override;
    void RenderPanels(float DeltaTime, ImGuiID DockspaceId) override;

    bool IsDirty() const override { return bDirty; }
    bool IsCapturingInput() const override { return bCapturingInput; }
    const char *GetEditorName() const override { return "CameraModifierStackEditor"; }
    const std::filesystem::path &GetAssetPath() const override { return EditingAssetPath; }

    bool CreateCameraShakeAsset();
    bool HasOpenAsset() const { return EditingAsset != nullptr; }

  private:
    void BuildDefaultDockLayout(ImGuiID DockspaceId);
    void RenderPanelsInternal(float DeltaTime, ImGuiID DockspaceId);
    void RenderContentsPanel(const FPanelDesc &Desc);
    void RenderDetailsPanel(const FPanelDesc &Desc);
    std::string MakePanelStableId(const char *PanelName) const;
    FPanelDesc MakePanelDesc(const char *DisplayName, const char *StableName, const char *IconKey,
                             ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse) const;

    void DrawToolbar();
    bool DrawLabeledField(const char *Label, const std::function<bool()> &DrawField);
    bool DrawCurveControlPointRow(const char *Label, float &XValue, float &YValue);
    bool DrawCurveEditor(const char *Label, FAssetBezierCurve &Curve);
    bool PromptForSavePath(void *OwnerWindowHandle = nullptr);
    void DrawCameraModifierStackAssetContents(UCameraModifierStackAssetData &Asset);
    void DrawCameraModifierStackAssetDetails(UCameraModifierStackAssetData &Asset);
    bool DrawCameraShakeDetails(FCameraShakeModifierAssetDesc &Desc);
    void MarkDirty() { bDirty = true; }

    FCameraShakeModifierAssetDesc *FindSelectedCameraShake(UCameraModifierStackAssetData &Asset);

  private:
    UEditorEngine        *EditorEngine = nullptr;
    FRenderer            *Renderer = nullptr;
    UAssetData           *EditingAsset = nullptr;
    std::filesystem::path EditingAssetPath;
    uint64                SelectedEditorId = 0;
    bool                  bOpen = false;
    bool                  bDirty = false;
    bool                  bCapturingInput = false;

    // 동일 타입 에디터를 여러 개 열었을 때 ImGui ID 충돌을 피하기 위한 인스턴스 ID.
    uint32 EditorInstanceId = 0;
    ImGuiID BuiltDockspaceId = 0;

    bool bContentsPanelOpen = true;
    bool bDetailsPanelOpen = true;
    bool bIsActiveTab = false;
};
