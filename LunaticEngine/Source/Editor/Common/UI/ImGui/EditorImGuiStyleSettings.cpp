#include "PCH/LunaticPCH.h"
#include "Common/UI/ImGui/EditorImGuiStyleSettings.h"

#include "Common/UI/Style/AccentColor.h"
#include "Common/UI/Style/EditorUIStyle.h"
#include "Core/CoreTypes.h"
#include "Engine/Core/SimpleJsonWrapper.h"
#include "Math/Vector.h"
#include "Platform/Paths.h"

#include "ImGui/imgui.h"

#include <filesystem>
#include <fstream>

namespace
{
    constexpr ImVec4 UnrealPanelSurface = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
    constexpr ImVec4 UnrealPanelSurfaceHover = ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f);
    constexpr ImVec4 UnrealBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);

    FString GetImGuiStyleSettingsPath()
    {
        return FPaths::ToUtf8(FPaths::SettingsDir() + L"ImGuiStyle.ini");
    }

    void SetNextSettingsWindowPosition(ImGuiCond Condition = ImGuiCond_Appearing)
    {
        if (const ImGuiViewport *MainViewport = ImGui::GetMainViewport())
        {
            const ImVec2 Anchor(MainViewport->Pos.x + MainViewport->Size.x * 0.5f,
                                MainViewport->Pos.y + MainViewport->Size.y * 0.42f);
            ImGui::SetNextWindowPos(Anchor, Condition, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowViewport(MainViewport->ID);
        }
    }

    bool BeginSettingsWindow(const char *Title, bool *bOpen, const ImVec2 &InitialSize, ImGuiCond SizeCondition,
                             ImGuiWindowFlags Flags = 0)
    {
        SetNextSettingsWindowPosition(ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(InitialSize, SizeCondition);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_Border, UnrealBorder);
        const bool bVisible = ImGui::Begin(Title, bOpen, Flags);
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
        return bVisible;
    }

    void DrawSettingsSectionHeader(const char *Label)
    {
        FEditorUIStyle::DrawPopupSectionHeader(Label);
    }
}

void FEditorImGuiStyleSettings::ShowPanel(bool *bOpen)
{
    if (bOpen && !*bOpen)
    {
        return;
    }

    if (!BeginSettingsWindow("ImGui Style Settings", bOpen, ImVec2(560.0f, 520.0f), ImGuiCond_Appearing))
    {
        ImGui::End();
        return;
    }

    ImGuiStyle &Style = ImGui::GetStyle();
    ImVec4 *Colors = Style.Colors;

    DrawSettingsSectionHeader("GENERAL");
    ImGui::DragFloat("Window Rounding", &Style.WindowRounding, 1.0f, 0.0f);
    ImGui::DragFloat("Frame Rounding", &Style.FrameRounding, 1.0f, 0.0f);
    ImGui::DragFloat("Grab Rounding", &Style.GrabRounding, 1.0f, 0.0f);
    ImGui::DragFloat("Scrollbar Rounding", &Style.ScrollbarRounding, 1.0f, 0.0f);
    ImGui::DragFloat("Window Border Size", &Style.WindowBorderSize, 1.0f, 0.0f);
    ImGui::DragFloat("Frame Border Size", &Style.FrameBorderSize, 1.0f, 0.0f);

    DrawSettingsSectionHeader("COLORS");
    const float FooterHeight = ImGui::GetFrameHeightWithSpacing() + 12.0f;
    if (ImGui::BeginChild("##ImGuiStyleScrollRegion", ImVec2(0.0f, -FooterHeight), true))
    {
        for (int i = 0; i < ImGuiCol_COUNT; i++)
        {
            ImGui::ColorEdit4(ImGui::GetStyleColorName(i), reinterpret_cast<float *>(&Colors[i]), ImGuiColorEditFlags_AlphaPreviewHalf);
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, UIAccentColor::WithAlpha(0.92f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UIAccentColor::Value);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIAccentColor::Value);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.97f, 0.98f, 1.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 8.0f));
    if (ImGui::Button("Save"))
    {
        Save();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::SameLine();
    if (ImGui::Button("Close"))
    {
        if (bOpen)
        {
            *bOpen = false;
        }
    }

    ImGui::End();
}

void FEditorImGuiStyleSettings::Save()
{
    using namespace json;

    ImGuiStyle& Style = ImGui::GetStyle();

    JSON Root = Object();
    JSON Colors = Array();

    for (int i = 0; i < ImGuiCol_COUNT; ++i)
    {
        const ImVec4& Color = Style.Colors[i];
        Colors.append(Array(Color.x, Color.y, Color.z, Color.w));
    }

    Root["Colors"] = Colors;
    Root["Alpha"] = Style.Alpha;
    Root["WindowRounding"] = Style.WindowRounding;
    Root["FrameRounding"] = Style.FrameRounding;
    Root["GrabRounding"] = Style.GrabRounding;
    Root["ScrollbarRounding"] = Style.ScrollbarRounding;
    Root["WindowBorderSize"] = Style.WindowBorderSize;
    Root["FrameBorderSize"] = Style.FrameBorderSize;

    const FString Path = GetImGuiStyleSettingsPath();
    std::filesystem::path FilePath(FPaths::ToWide(Path));
    if (FilePath.has_parent_path())
    {
        std::filesystem::create_directories(FilePath.parent_path());
    }

    std::ofstream File(FilePath);
    if (File.is_open())
    {
        File << Root;
    }
}

void FEditorImGuiStyleSettings::Load()
{
    using namespace json;

    ImGui::StyleColorsDark();

    const FString Path = GetImGuiStyleSettingsPath();
    std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
    if (!File.is_open())
    {
        return;
    }

    FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    JSON Root = JSON::Load(Content);
    if (Root.IsNull())
    {
        return;
    }

    ImGuiStyle& Style = ImGui::GetStyle();

    if (Root.hasKey("Colors"))
    {
        JSON Colors = Root["Colors"];
        int32 Count = static_cast<int32>(Colors.length());
        if (Count > ImGuiCol_COUNT)
        {
            Count = ImGuiCol_COUNT;
        }

        for (int32 i = 0; i < Count; ++i)
        {
            JSON Color = Colors[i];
            Style.Colors[i] = ImVec4(
                static_cast<float>(Color[0].ToFloat()),
                static_cast<float>(Color[1].ToFloat()),
                static_cast<float>(Color[2].ToFloat()),
                static_cast<float>(Color[3].ToFloat()));
        }
    }

    if (Root.hasKey("Alpha")) Style.Alpha = static_cast<float>(Root["Alpha"].ToFloat());
    if (Root.hasKey("WindowRounding")) Style.WindowRounding = static_cast<float>(Root["WindowRounding"].ToFloat());
    if (Root.hasKey("FrameRounding")) Style.FrameRounding = static_cast<float>(Root["FrameRounding"].ToFloat());
    if (Root.hasKey("GrabRounding")) Style.GrabRounding = static_cast<float>(Root["GrabRounding"].ToFloat());
    if (Root.hasKey("ScrollbarRounding")) Style.ScrollbarRounding = static_cast<float>(Root["ScrollbarRounding"].ToFloat());
    if (Root.hasKey("WindowBorderSize")) Style.WindowBorderSize = static_cast<float>(Root["WindowBorderSize"].ToFloat());
    if (Root.hasKey("FrameBorderSize")) Style.FrameBorderSize = static_cast<float>(Root["FrameBorderSize"].ToFloat());
}
