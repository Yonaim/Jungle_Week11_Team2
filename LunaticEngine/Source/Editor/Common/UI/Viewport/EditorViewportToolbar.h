#pragma once

#include "Common/UI/Style/AccentColor.h"
#include "Common/UI/Style/EditorUIStyle.h"
#include "Core/CoreTypes.h"
#include "Platform/Paths.h"
#include "Render/Pipeline/Renderer.h"
#include "Resource/ResourceManager.h"
#include "WICTextureLoader.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <functional>

/**
 * Level Editor / Asset Editor viewport 상단 toolbar의 실제 버튼 구성을 공유하는 renderer.
 *
 * FViewportToolbar가 toolbar window/child의 배치만 담당한다면,
 * FEditorViewportToolbar는 Translate/Rotate/Scale, Coord Space, Snap, Camera,
 * Viewport Type, View Mode, Show, Layout 버튼의 공통 UI 구성을 담당한다.
 */
class FEditorViewportToolbar
{
  public:
    enum class EIcon : int32
    {
        Menu = 0,
        Setting,
        AddActor,
        Translate,
        Rotate,
        Scale,
        WorldSpace,
        LocalSpace,
        TranslateSnap,
        RotateSnap,
        ScaleSnap,
        SnapSettings,
        CameraSettings,
        ShowFlag,
        ViewModeLit,
        ViewModeUnlit,
        ViewModeWireframe,
        ViewModeSceneDepth,
        ViewModeWorldNormal,
        ViewModeLightCulling,
        ViewportPerspective,
        ViewportTop,
        ViewportBottom,
        ViewportLeft,
        ViewportRight,
        ViewportFront,
        ViewportBack,
        ViewportFreeOrtho,
        Count
    };

    enum class EToolMode : int32
    {
        Translate = 0,
        Rotate,
        Scale
    };

    enum class ECoordSpace : int32
    {
        World = 0,
        Local
    };

    struct FDesc
    {
        const char *IdPrefix = "ViewportToolbar";
        float ToolbarWidth = 0.0f;
        FRenderer *Renderer = nullptr;
        bool bEnabled = true;

        EToolMode ToolMode = EToolMode::Translate;
        ECoordSpace CoordSpace = ECoordSpace::World;
        bool bSnapEnabled = false;

        EIcon ViewportTypeIcon = EIcon::ViewportPerspective;
        const char *ViewportTypeLabel = "Perspective";
        EIcon ViewModeIcon = EIcon::ViewModeLit;
        const char *ViewModeLabel = "Lit";

        bool bShowCameraButton = true;
        bool bShowViewportTypeButton = true;
        bool bShowViewModeButton = true;
        bool bShowShowButton = true;
        bool bShowLayoutButton = true;

        std::function<void(EToolMode)> OnToolModeChanged;
        std::function<void()> OnCoordSpaceToggled;
        std::function<void()> DrawSnapPopup;
        std::function<void()> DrawCameraPopup;
        std::function<void()> DrawViewportTypePopup;
        std::function<void()> DrawViewModePopup;
        std::function<void()> DrawShowPopup;
        std::function<void()> DrawLayoutPopup;
    };

    static void RenderViewportToolbar(const FDesc &Desc)
    {
        EnsureIconsLoaded(Desc.Renderer);
        PushToolbarButtonStyle();
        ImGui::BeginDisabled(!Desc.bEnabled);

        constexpr float FallbackIconSize = 14.0f;
        constexpr float MaxIconSize = 16.0f;
        constexpr float ButtonHeight = 26.0f;
        constexpr float ButtonSpacing = 6.0f;
        const float ToolbarWidth = Desc.ToolbarWidth > 0.0f ? Desc.ToolbarWidth : ImGui::GetWindowWidth();
        const bool bCompact = ToolbarWidth < 520.0f;
        const float EffectiveSpacing = bCompact ? 3.0f : ButtonSpacing;
        const float PopupButtonWidth = ToolbarWidth >= 700.0f ? 46.0f : (bCompact ? 30.0f : 36.0f);
        const float RightButtonSpacing = ToolbarWidth >= 700.0f ? ButtonSpacing : (bCompact ? 0.0f : 2.0f);

        bool bHasLeftItem = false;
        auto BeginLeftItem = [&](float Spacing)
        {
            if (bHasLeftItem)
            {
                ImGui::SameLine(0.0f, Spacing);
            }
            bHasLeftItem = true;
        };

        auto DrawToolButton = [&](const char *Id, EIcon Icon, EToolMode Mode, const char *Tooltip)
        {
            BeginLeftItem(EffectiveSpacing);
            const bool bSelected = Desc.ToolMode == Mode;
            if (bSelected)
            {
                PushSelectedButtonStyle();
            }
            if (DrawIconButton(Id, Icon, Tooltip, FallbackIconSize, MaxIconSize, IM_COL32_WHITE))
            {
                if (Desc.OnToolModeChanged)
                {
                    Desc.OnToolModeChanged(Mode);
                }
            }
            if (bSelected)
            {
                PopSelectedButtonStyle();
            }
            ShowTooltip(Tooltip);
        };

        if (ToolbarWidth >= 150.0f)
        {
            DrawToolButton(MakeId(Desc.IdPrefix, "TranslateTool"), EIcon::Translate, EToolMode::Translate, "Translate");
        }
        if (ToolbarWidth >= 185.0f)
        {
            DrawToolButton(MakeId(Desc.IdPrefix, "RotateTool"), EIcon::Rotate, EToolMode::Rotate, "Rotate");
        }
        if (ToolbarWidth >= 220.0f)
        {
            DrawToolButton(MakeId(Desc.IdPrefix, "ScaleTool"), EIcon::Scale, EToolMode::Scale, "Scale");
        }

        if (ToolbarWidth >= 255.0f)
        {
            BeginLeftItem(bCompact ? 5.0f : 10.0f);
            const bool bWorld = Desc.CoordSpace == ECoordSpace::World;
            if (DrawIconButton(MakeId(Desc.IdPrefix, "CoordSpace"), bWorld ? EIcon::WorldSpace : EIcon::LocalSpace,
                               bWorld ? "World Space" : "Local Space", FallbackIconSize, MaxIconSize, IM_COL32_WHITE))
            {
                if (Desc.OnCoordSpaceToggled)
                {
                    Desc.OnCoordSpaceToggled();
                }
            }
            ShowTooltip(bWorld ? "World Space" : "Local Space");
        }

        if (ToolbarWidth >= 292.0f)
        {
            BeginLeftItem(EffectiveSpacing);
            if (Desc.bSnapEnabled)
            {
                PushSelectedButtonStyle();
            }
            if (DrawIconDropdownButton(MakeId(Desc.IdPrefix, "SnapSettings"), EIcon::SnapSettings, PopupButtonWidth, ButtonHeight,
                                       FallbackIconSize, MaxIconSize, IM_COL32_WHITE))
            {
                ImGui::OpenPopup(MakeId(Desc.IdPrefix, "SnapPopup"));
            }
            if (Desc.bSnapEnabled)
            {
                PopSelectedButtonStyle();
            }
            DrawDropdownArrowForLastItem();
            ShowTooltip("Snap Settings");
            PushCommonPopupStyle();
            if (ImGui::BeginPopup(MakeId(Desc.IdPrefix, "SnapPopup")))
            {
                if (Desc.DrawSnapPopup)
                {
                    Desc.DrawSnapPopup();
                }
                ImGui::EndPopup();
            }
            PopCommonPopupStyle();
        }

        const bool bShowViewportLabel = ToolbarWidth >= 560.0f;
        const bool bShowViewModeLabel = ToolbarWidth >= 680.0f;
        const float ViewportButtonWidth = bShowViewportLabel ? GetIconLabelButtonWidth(Desc.ViewportTypeIcon, Desc.ViewportTypeLabel,
                                                                                       FallbackIconSize, MaxIconSize)
                                                        : PopupButtonWidth;
        const float ViewModeButtonWidth = bShowViewModeLabel ? GetIconLabelButtonWidth(Desc.ViewModeIcon, Desc.ViewModeLabel,
                                                                                       FallbackIconSize, MaxIconSize)
                                                          : PopupButtonWidth;

        float RightGroupWidth = 0.0f;
        bool bHasRightItem = false;
        auto AccumulateRightWidth = [&](bool bVisible, float Width)
        {
            if (!bVisible)
            {
                return;
            }
            if (bHasRightItem)
            {
                RightGroupWidth += RightButtonSpacing;
            }
            RightGroupWidth += Width;
            bHasRightItem = true;
        };
        AccumulateRightWidth(Desc.bShowCameraButton && ToolbarWidth >= 385.0f, PopupButtonWidth);
        AccumulateRightWidth(Desc.bShowViewportTypeButton, ViewportButtonWidth);
        AccumulateRightWidth(Desc.bShowViewModeButton && ToolbarWidth >= 340.0f, ViewModeButtonWidth);
        AccumulateRightWidth(Desc.bShowShowButton && ToolbarWidth >= 430.0f, PopupButtonWidth);
        AccumulateRightWidth(Desc.bShowLayoutButton && ToolbarWidth >= 475.0f, PopupButtonWidth);

        const float RightStartX = ImGui::GetWindowWidth() - RightGroupWidth - 8.0f;
        if (RightGroupWidth > 0.0f && RightStartX > ImGui::GetCursorPosX() + 12.0f)
        {
            ImGui::SameLine(RightStartX);
        }
        else if (bHasLeftItem)
        {
            ImGui::SameLine(0.0f, ButtonSpacing);
        }

        bool bPrintedRightItem = false;
        auto BeginRightItem = [&]()
        {
            if (bPrintedRightItem)
            {
                ImGui::SameLine(0.0f, RightButtonSpacing);
            }
            bPrintedRightItem = true;
        };

        auto DrawPopupButton = [&](bool bVisible, const char *ButtonId, const char *PopupId, EIcon Icon, const char *Label,
                                   float Width, bool bShowLabel, const char *Tooltip, const std::function<void()> &DrawPopup)
        {
            if (!bVisible)
            {
                return;
            }
            BeginRightItem();
            const bool bClicked = bShowLabel ? DrawIconLabelButton(MakeId(Desc.IdPrefix, ButtonId), Icon, Label, Width, ButtonHeight,
                                                                   FallbackIconSize, MaxIconSize)
                                           : DrawIconDropdownButton(MakeId(Desc.IdPrefix, ButtonId), Icon, Width, ButtonHeight,
                                                                    FallbackIconSize, MaxIconSize);
            DrawDropdownArrowForLastItem();
            ShowTooltip(Tooltip);
            if (bClicked)
            {
                ImGui::OpenPopup(MakeId(Desc.IdPrefix, PopupId));
            }
            PushCommonPopupStyle();
            if (ImGui::BeginPopup(MakeId(Desc.IdPrefix, PopupId)))
            {
                if (DrawPopup)
                {
                    DrawPopup();
                }
                ImGui::EndPopup();
            }
            PopCommonPopupStyle();
        };

        DrawPopupButton(Desc.bShowCameraButton && ToolbarWidth >= 385.0f, "Camera", "CameraPopup", EIcon::CameraSettings,
                        "", PopupButtonWidth, false, "Camera Settings", Desc.DrawCameraPopup);
        DrawPopupButton(Desc.bShowViewportTypeButton, "ViewportType", "ViewportTypePopup", Desc.ViewportTypeIcon,
                        Desc.ViewportTypeLabel, ViewportButtonWidth, bShowViewportLabel, "Viewport Type", Desc.DrawViewportTypePopup);
        DrawPopupButton(Desc.bShowViewModeButton && ToolbarWidth >= 340.0f, "ViewMode", "ViewModePopup", Desc.ViewModeIcon,
                        Desc.ViewModeLabel, ViewModeButtonWidth, bShowViewModeLabel, "View Mode", Desc.DrawViewModePopup);
        DrawPopupButton(Desc.bShowShowButton && ToolbarWidth >= 430.0f, "Show", "ShowPopup", EIcon::ShowFlag, "",
                        PopupButtonWidth, false, "Show", Desc.DrawShowPopup);
        DrawPopupButton(Desc.bShowLayoutButton && ToolbarWidth >= 475.0f, "Layout", "LayoutPopup", EIcon::Menu, "",
                        PopupButtonWidth, false, "Viewport Layout", Desc.DrawLayoutPopup);

        ImGui::EndDisabled();
        PopToolbarButtonStyle();
    }


    static void PushCommonPopupStyle()
    {
        FEditorUIStyle::PushPopupWindowStyle();
    }

    static void PopCommonPopupStyle()
    {
        FEditorUIStyle::PopPopupWindowStyle();
    }

    static void DrawPopupSectionHeader(const char *Label)
    {
        FEditorUIStyle::DrawPopupSectionHeader(Label);
    }

    static void DrawPopupSeparator(float SpacingBefore = 4.0f, float SpacingAfter = 6.0f)
    {
        FEditorUIStyle::DrawPopupSeparator(SpacingBefore, SpacingAfter);
    }

    static bool DrawIconSelectable(const char *Id, EIcon Icon, const char *Label, bool bSelected, float Width = 240.0f)
    {
        ImGui::PushID(Id);
        const float RowWidth = Width <= 0.0f ? ImGui::GetContentRegionAvail().x : Width;
        const bool bClicked = ImGui::Selectable("##Option", bSelected, ImGuiSelectableFlags_None, ImVec2(RowWidth, 24.0f));
        const bool bHovered = ImGui::IsItemHovered();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        ImDrawList *DrawList = ImGui::GetWindowDrawList();
        if (bHovered || bSelected)
        {
            const ImRect RowRect = FEditorUIStyle::GetFullPopupRowRect(Min, Max);
            DrawList->AddRectFilled(RowRect.Min, RowRect.Max, ImGui::GetColorU32(UIAccentColor::Value));
        }
        const ImU32 RowTextColor = ImGui::GetColorU32((bHovered || bSelected) ? ImVec4(0.02f, 0.02f, 0.025f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text));
        if (ID3D11ShaderResourceView *IconSRV = GetIconTable()[static_cast<int32>(Icon)])
        {
            DrawList->AddImage(reinterpret_cast<ImTextureID>(IconSRV), ImVec2(Min.x + 6.0f, Min.y + 4.0f),
                               ImVec2(Min.x + 20.0f, Min.y + 18.0f), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), RowTextColor);
        }
        DrawList->AddText(ImVec2(Min.x + 28.0f, Min.y + 4.0f), RowTextColor, Label ? Label : "");
        ImGui::PopID();
        return bClicked;
    }



    static const char *GetViewportTypeLabelByIndex(int32 TypeIndex)
    {
        static const char *ViewportTypeNames[] = {"Perspective", "Top", "Bottom", "Left", "Right", "Front", "Back", "Free Ortho"};
        return (TypeIndex >= 0 && TypeIndex < 8) ? ViewportTypeNames[TypeIndex] : "Perspective";
    }

    static EIcon GetViewportTypeIconByIndex(int32 TypeIndex)
    {
        switch (TypeIndex)
        {
        case 1: return EIcon::ViewportTop;
        case 2: return EIcon::ViewportBottom;
        case 3: return EIcon::ViewportLeft;
        case 4: return EIcon::ViewportRight;
        case 5: return EIcon::ViewportFront;
        case 6: return EIcon::ViewportBack;
        case 7: return EIcon::ViewportFreeOrtho;
        case 0:
        default: return EIcon::ViewportPerspective;
        }
    }

    static const char *GetViewModeLabelByIndex(int32 ModeIndex)
    {
        // Lit_Phong / Lit_Gouraud / Lit_Lambert 계열은 toolbar label을 모두 Lit으로 맞춘다.
        static const char *ViewModeNames[] = {"Lit", "Unlit", "Lit", "Lit", "Wireframe", "Scene Depth", "World Normal", "Light Culling"};
        return (ModeIndex >= 0 && ModeIndex < 8) ? ViewModeNames[ModeIndex] : "Lit";
    }

    static EIcon GetViewModeIconByIndex(int32 ModeIndex)
    {
        switch (ModeIndex)
        {
        case 1: return EIcon::ViewModeUnlit;
        case 4: return EIcon::ViewModeWireframe;
        case 5: return EIcon::ViewModeSceneDepth;
        case 6: return EIcon::ViewModeWorldNormal;
        case 7: return EIcon::ViewModeLightCulling;
        case 0:
        case 2:
        case 3:
        default: return EIcon::ViewModeLit;
        }
    }

    static void DrawSnapValueRow(const char *Label, bool &bEnabled, float &Value, const float *Values, int32 Count, const char *Format)
    {
        ImGui::Checkbox(Label, &bEnabled);
        ImGui::BeginDisabled(!bEnabled);
        ImGui::Indent(12.0f);
        for (int32 i = 0; i < Count; ++i)
        {
            ImGui::PushID(i);
            char ButtonLabel[32];
            snprintf(ButtonLabel, sizeof(ButtonLabel), Format, Values[i]);
            const bool bSelected = Value == Values[i];
            if (ImGui::Selectable(ButtonLabel, bSelected, 0, ImVec2(88.0f, 22.0f)))
            {
                Value = Values[i];
            }
            if ((i + 1) % 3 != 0 && i + 1 < Count)
            {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
        ImGui::Unindent(12.0f);
        ImGui::EndDisabled();
    }

    static void DrawCommonSnapPopup(bool &bEnableTranslationSnap, float &TranslationSnapSize,
                                    bool &bEnableRotationSnap, float &RotationSnapSize,
                                    bool &bEnableScaleSnap, float &ScaleSnapSize)
    {
        static const float TranslationSnapSizes[] = {1.0f, 5.0f, 10.0f, 50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f};
        static const float RotationSnapSizes[] = {1.0f, 5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 60.0f, 90.0f};
        static const float ScaleSnapSizes[] = {0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f};

        DrawPopupSectionHeader("SNAP SETTINGS");
        DrawSnapValueRow("Location", bEnableTranslationSnap, TranslationSnapSize, TranslationSnapSizes, 9, "%.0f");
        ImGui::Spacing();
        DrawSnapValueRow("Rotation", bEnableRotationSnap, RotationSnapSize, RotationSnapSizes, 8, "%.0f deg");
        ImGui::Spacing();
        DrawSnapValueRow("Scale", bEnableScaleSnap, ScaleSnapSize, ScaleSnapSizes, 8, "%.5g");
    }

    static void DrawScalarFloat(const char *Label, float &Value, float Speed, float Min, float Max, const char *Format)
    {
        ImGui::SetNextItemWidth(150.0f);
        ImGui::DragFloat(Label, &Value, Speed, Min, Max, Format);
    }

    static void DrawSliderFloat(const char *Label, float &Value, float Min, float Max, const char *Format = "%.2f")
    {
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SliderFloat(Label, &Value, Min, Max, Format);
    }

    static void DrawSliderInt(const char *Label, int32 &Value, int32 Min, int32 Max)
    {
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SliderInt(Label, &Value, Min, Max);
    }

  private:
    static const char *GetIconResourceKey(EIcon Icon)
    {
        switch (Icon)
        {
        case EIcon::Menu: return "Editor.ToolIcon.Menu";
        case EIcon::Setting: return "Editor.ToolIcon.Setting";
        case EIcon::AddActor: return "Editor.ToolIcon.AddActor";
        case EIcon::Translate: return "Editor.ToolIcon.Translate";
        case EIcon::Rotate: return "Editor.ToolIcon.Rotate";
        case EIcon::Scale: return "Editor.ToolIcon.Scale";
        case EIcon::WorldSpace: return "Editor.ToolIcon.WorldSpace";
        case EIcon::LocalSpace: return "Editor.ToolIcon.LocalSpace";
        case EIcon::TranslateSnap: return "Editor.ToolIcon.TranslateSnap";
        case EIcon::RotateSnap: return "Editor.ToolIcon.RotateSnap";
        case EIcon::ScaleSnap: return "Editor.ToolIcon.ScaleSnap";
        case EIcon::SnapSettings: return "Editor.ToolIcon.SnapSettings";
        case EIcon::CameraSettings: return "Editor.ToolIcon.Camera";
        case EIcon::ShowFlag: return "Editor.ToolIcon.ShowFlag";
        case EIcon::ViewModeLit: return "Editor.ToolIcon.ViewMode.Lit";
        case EIcon::ViewModeUnlit: return "Editor.ToolIcon.ViewMode.Unlit";
        case EIcon::ViewModeWireframe: return "Editor.ToolIcon.ViewMode.Wireframe";
        case EIcon::ViewModeSceneDepth: return "Editor.ToolIcon.ViewMode.SceneDepth";
        case EIcon::ViewModeWorldNormal: return "Editor.ToolIcon.ViewMode.WorldNormal";
        case EIcon::ViewModeLightCulling: return "Editor.ToolIcon.ViewMode.LightCulling";
        case EIcon::ViewportPerspective: return "Editor.ToolIcon.Viewport.Perspective";
        case EIcon::ViewportTop: return "Editor.ToolIcon.Viewport.Top";
        case EIcon::ViewportBottom: return "Editor.ToolIcon.Viewport.Bottom";
        case EIcon::ViewportLeft: return "Editor.ToolIcon.Viewport.Left";
        case EIcon::ViewportRight: return "Editor.ToolIcon.Viewport.Right";
        case EIcon::ViewportFront: return "Editor.ToolIcon.Viewport.Front";
        case EIcon::ViewportBack: return "Editor.ToolIcon.Viewport.Back";
        case EIcon::ViewportFreeOrtho: return "Editor.ToolIcon.Viewport.FreeOrtho";
        default: return "";
        }
    }

    static const char *MakeId(const char *Prefix, const char *Suffix)
    {
        static char Buffers[16][128] = {};
        static int32 Index = 0;
        char *Buffer = Buffers[Index++ % 16];
        snprintf(Buffer, 128, "##%s_%s", Prefix ? Prefix : "ViewportToolbar", Suffix ? Suffix : "Item");
        return Buffer;
    }

    static ID3D11ShaderResourceView **GetIconTable()
    {
        static ID3D11ShaderResourceView *Icons[static_cast<int32>(EIcon::Count)] = {};
        return Icons;
    }

    static bool &GetIconsLoadedFlag()
    {
        static bool bLoaded = false;
        return bLoaded;
    }

    static void EnsureIconsLoaded(FRenderer *Renderer)
    {
        bool &bLoaded = GetIconsLoadedFlag();
        if (bLoaded || !Renderer)
        {
            return;
        }
        ID3D11Device *Device = Renderer->GetFD3DDevice().GetDevice();
        if (!Device)
        {
            return;
        }
        ID3D11ShaderResourceView **Icons = GetIconTable();
        for (int32 i = 0; i < static_cast<int32>(EIcon::Count); ++i)
        {
            const FString Path = FResourceManager::Get().ResolvePath(FName(GetIconResourceKey(static_cast<EIcon>(i))));
            DirectX::CreateWICTextureFromFile(Device, FPaths::ToWide(Path).c_str(), nullptr, &Icons[i]);
        }
        bLoaded = true;
    }

    static ImVec2 GetIconRenderSize(EIcon Icon, float FallbackSize, float MaxIconSize)
    {
        ID3D11ShaderResourceView *SRV = GetIconTable()[static_cast<int32>(Icon)];
        if (!SRV)
        {
            return ImVec2(FallbackSize, FallbackSize);
        }
        ID3D11Resource *Resource = nullptr;
        SRV->GetResource(&Resource);
        if (!Resource)
        {
            return ImVec2(FallbackSize, FallbackSize);
        }
        ImVec2 Size(FallbackSize, FallbackSize);
        D3D11_RESOURCE_DIMENSION Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
        Resource->GetType(&Dimension);
        if (Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        {
            ID3D11Texture2D *Texture = static_cast<ID3D11Texture2D *>(Resource);
            D3D11_TEXTURE2D_DESC Desc{};
            Texture->GetDesc(&Desc);
            Size = ImVec2(static_cast<float>(Desc.Width), static_cast<float>(Desc.Height));
        }
        Resource->Release();
        if (Size.x > MaxIconSize || Size.y > MaxIconSize)
        {
            const float Scale = (Size.x > Size.y) ? (MaxIconSize / Size.x) : (MaxIconSize / Size.y);
            Size.x *= Scale;
            Size.y *= Scale;
        }
        return Size;
    }

    static bool DrawIconButton(const char *Id, EIcon Icon, const char *FallbackLabel, float FallbackSize, float MaxIconSize,
                               ImU32 Tint = IM_COL32_WHITE)
    {
        ID3D11ShaderResourceView *SRV = GetIconTable()[static_cast<int32>(Icon)];
        if (SRV)
        {
            return ImGui::ImageButton(Id, reinterpret_cast<ImTextureID>(SRV), GetIconRenderSize(Icon, FallbackSize, MaxIconSize),
                                      ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImVec4(0, 0, 0, 0),
                                      ImGui::ColorConvertU32ToFloat4(Tint));
        }
        return ImGui::Button(FallbackLabel ? FallbackLabel : "?");
    }

    static bool DrawIconLabelButton(const char *Id, EIcon Icon, const char *Label, float Width, float Height,
                                    float FallbackSize, float MaxIconSize, ImU32 Tint = IM_COL32_WHITE)
    {
        const bool bClicked = ImGui::Button(Id, ImVec2(Width, Height));
        ImDrawList *DrawList = ImGui::GetWindowDrawList();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        const ImVec2 IconSize = GetIconRenderSize(Icon, FallbackSize, MaxIconSize);
        const float IconX = Min.x + 7.0f;
        const float IconY = Min.y + ((Max.y - Min.y) - IconSize.y) * 0.5f;
        if (ID3D11ShaderResourceView *SRV = GetIconTable()[static_cast<int32>(Icon)])
        {
            DrawList->AddImage(reinterpret_cast<ImTextureID>(SRV), ImVec2(IconX, IconY), ImVec2(IconX + IconSize.x, IconY + IconSize.y),
                               ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), Tint);
        }
        if (Label && Label[0] != '\0')
        {
            DrawList->AddText(ImVec2(IconX + IconSize.x + 5.0f, Min.y + 5.0f), ImGui::GetColorU32(ImGuiCol_Text), Label);
        }
        return bClicked;
    }

    static bool DrawIconDropdownButton(const char *Id, EIcon Icon, float Width, float Height, float FallbackSize, float MaxIconSize,
                                       ImU32 Tint = IM_COL32_WHITE)
    {
        const bool bClicked = ImGui::Button(Id, ImVec2(Width, Height));
        ImDrawList *DrawList = ImGui::GetWindowDrawList();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        const ImVec2 IconSize = GetIconRenderSize(Icon, FallbackSize, MaxIconSize);
        const float IconX = Min.x + ((Max.x - Min.x) - IconSize.x) * 0.5f;
        const float IconY = Min.y + ((Max.y - Min.y) - IconSize.y) * 0.5f;
        if (ID3D11ShaderResourceView *SRV = GetIconTable()[static_cast<int32>(Icon)])
        {
            DrawList->AddImage(reinterpret_cast<ImTextureID>(SRV), ImVec2(IconX, IconY), ImVec2(IconX + IconSize.x, IconY + IconSize.y),
                               ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), Tint);
        }
        return bClicked;
    }

    static float GetIconLabelButtonWidth(EIcon Icon, const char *Label, float FallbackSize, float MaxIconSize)
    {
        const ImVec2 IconSize = GetIconRenderSize(Icon, FallbackSize, MaxIconSize);
        const float LabelWidth = (Label && Label[0] != '\0') ? ImGui::CalcTextSize(Label).x : 0.0f;
        return (std::max)(36.0f, 7.0f + IconSize.x + (LabelWidth > 0.0f ? 5.0f + LabelWidth : 0.0f) + 20.0f);
    }

    static void DrawDropdownArrowForLastItem()
    {
        ImDrawList *DrawList = ImGui::GetWindowDrawList();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        const float X = Max.x - 12.0f;
        const float Y = Min.y + ((Max.y - Min.y) * 0.5f) - 1.0f;
        const ImU32 Color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        DrawList->AddTriangleFilled(ImVec2(X, Y), ImVec2(X + 7.0f, Y), ImVec2(X + 3.5f, Y + 4.0f), Color);
    }

    static void ShowTooltip(const char *Tooltip)
    {
        if (Tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip("%s", Tooltip);
        }
    }

    static void PushToolbarButtonStyle()
    {
        // LevelViewportLayout.cpp에서 쓰던 pane toolbar 버튼 스타일과 동일하게 유지한다.
        // 공통 toolbar를 쓰는 Asset Editor도 버튼 배경/hover/active 색이 레벨 에디터와 같아야 한다.
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.36f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.35f, 0.39f, 0.9f));
    }

    static void PopToolbarButtonStyle()
    {
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }

    static void PushSelectedButtonStyle()
    {
        ImGui::PushStyleColor(ImGuiCol_Button, UIAccentColor::Value);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UIAccentColor::Value);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIAccentColor::Value);
    }

    static void PopSelectedButtonStyle()
    {
        ImGui::PopStyleColor(3);
    }

};
