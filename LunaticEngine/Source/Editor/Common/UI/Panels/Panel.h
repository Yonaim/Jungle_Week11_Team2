#pragma once

#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Core/CoreTypes.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <string>
#include <unordered_map>
#include <vector>

/**
 * Panel 공통 descriptor.
 *
 * 역할:
 * - Level Editor 패널과 Asset Editor 패널이 같은 title/icon/inset 스타일을 쓰도록 한다.
 * - DisplayName은 사용자가 보는 이름이고, StableId는 ImGui 내부 ID 충돌 방지용 이름이다.
 * - 같은 이름의 Details / Viewport 패널이 여러 에디터에 생길 수 있으므로 StableId는 반드시 고유해야 한다.
 */
struct FPanelDesc
{
    const char *DisplayName = "";
    const char *StableId = "";
    const char *IconKey = nullptr;

    ImGuiID DockspaceId = 0;
    ImGuiCond DockCond = ImGuiCond_FirstUseEver;
    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse;

    bool bClosable = false;
    bool *bOpen = nullptr;

    bool bDrawTitleIcon = true;
    bool bApplyContentTopInset = true;
    bool bApplySideInset = true;
    bool bApplyBottomInset = true;
};

struct FDockPanelLayoutState
{
    // Per-document-tab layout state. This is intentionally in-memory only.
    // It stores the current dock relationship that ImGui assigned to each panel,
    // so tab switching restores the user's current layout instead of rebuilding
    // the default layout every time.
    bool bDefaultLayoutBuilt = false;
    bool bRequestDefaultLayout = true;
    bool bRestoreCapturedLayoutNextFrame = false;
    bool bApplyingRestore = false;
    int32 RestoreCapturedLayoutFramesRemaining = 0;
    std::unordered_map<std::string, ImGuiID> PanelDockIds;
};

/**
 * Panel 공통 wrapper.
 *
 * 사용 이유:
 * - 기존 Level Editor 패널은 PanelTitleUtils 기반의 탭 스타일을 사용한다.
 * - SkeletalMeshEditor 같은 Asset Editor 패널도 이 wrapper를 통하면 같은 스타일을 유지할 수 있다.
 * - 패널마다 ImGui::Begin / DrawPanelTitleIcon / ApplyPanelContentTopInset 코드를 반복하지 않게 한다.
 */
class FPanel
{
  private:
    struct FBeginState
    {
        bool bDidBeginWindow = false;
        bool bPushedStyle = false;
    };

    static std::vector<FBeginState> &GetBeginStateStack()
    {
        static std::vector<FBeginState> Stack;
        return Stack;
    }

    static std::string &GetStableIdPrefixStorage()
    {
        static std::string Prefix;
        return Prefix;
    }

    static FDockPanelLayoutState *&GetCurrentLayoutStateStorage()
    {
        static FDockPanelLayoutState *State = nullptr;
        return State;
    }

    static ImGuiID &GetCurrentDockspaceIdStorage()
    {
        static ImGuiID DockspaceId = 0;
        return DockspaceId;
    }

    static std::string MakeEffectiveStableId(const char *StableId)
    {
        const char *RawStableId = (StableId && StableId[0] != '\0') ? StableId : "Panel";
        const std::string &Prefix = GetStableIdPrefixStorage();
        if (Prefix.empty())
        {
            return RawStableId;
        }
        return Prefix + "_" + RawStableId;
    }

  public:
    static void SetCurrentStableIdPrefix(const std::string &Prefix)
    {
        GetStableIdPrefixStorage() = Prefix;
    }

    static void ClearCurrentStableIdPrefix()
    {
        GetStableIdPrefixStorage().clear();
    }

    static void SetCurrentDockspaceId(ImGuiID DockspaceId)
    {
        GetCurrentDockspaceIdStorage() = DockspaceId;
    }

    static void ClearCurrentDockspaceId()
    {
        GetCurrentDockspaceIdStorage() = 0;
    }

    static void SetCurrentLayoutState(FDockPanelLayoutState *LayoutState)
    {
        GetCurrentLayoutStateStorage() = LayoutState;
    }

    static void ClearCurrentLayoutState()
    {
        GetCurrentLayoutStateStorage() = nullptr;
    }

    static bool HasCapturedDockLayout(const FDockPanelLayoutState *LayoutState)
    {
        return LayoutState && !LayoutState->PanelDockIds.empty();
    }

    static void RequestCapturedLayoutRestore(FDockPanelLayoutState *LayoutState, int32 FrameCount = 2)
    {
        if (!LayoutState || LayoutState->PanelDockIds.empty())
        {
            return;
        }

        LayoutState->bRestoreCapturedLayoutNextFrame = true;
        LayoutState->RestoreCapturedLayoutFramesRemaining =
            (std::max)(LayoutState->RestoreCapturedLayoutFramesRemaining, FrameCount);
    }

    static void ClearCapturedLayoutRestore(FDockPanelLayoutState *LayoutState)
    {
        if (!LayoutState)
        {
            return;
        }

        LayoutState->bRestoreCapturedLayoutNextFrame = false;
        LayoutState->RestoreCapturedLayoutFramesRemaining = 0;
    }

    static void ConsumeCapturedLayoutRestoreFrame(FDockPanelLayoutState *LayoutState)
    {
        if (!LayoutState || !LayoutState->bRestoreCapturedLayoutNextFrame)
        {
            return;
        }

        if (LayoutState->RestoreCapturedLayoutFramesRemaining > 0)
        {
            --LayoutState->RestoreCapturedLayoutFramesRemaining;
        }

        if (LayoutState->RestoreCapturedLayoutFramesRemaining <= 0)
        {
            ClearCapturedLayoutRestore(LayoutState);
        }
    }

    static std::string MakeTitle(const FPanelDesc &Desc)
    {
        const char *StableId = (Desc.StableId && Desc.StableId[0] != '\0') ? Desc.StableId : Desc.DisplayName;
        const bool bHasIcon = Desc.IconKey && Desc.IconKey[0] != '\0';
        const char *Prefix = bHasIcon ? "      " : "";
        return std::string(Prefix) + (Desc.DisplayName ? Desc.DisplayName : "") + "###" + MakeEffectiveStableId(StableId);
    }

    static bool Begin(const FPanelDesc &Desc)
    {
        FBeginState BeginState;

        // Window 메뉴에서 패널을 끈 상태라면 ImGui::Begin 자체를 호출하지 않는다.
        // 닫힌 p_open=false 상태로 Begin/End를 강제로 호출하면 docking 경로에서
        // style stack sanity check가 깨지거나 End mismatch가 발생할 수 있다.
        if (Desc.bClosable && Desc.bOpen && !*Desc.bOpen)
        {
            GetBeginStateStack().push_back(BeginState);
            return false;
        }

        const std::string EffectiveStableId = MakeEffectiveStableId((Desc.StableId && Desc.StableId[0] != '\0') ? Desc.StableId : Desc.DisplayName);
        FDockPanelLayoutState *LayoutState = GetCurrentLayoutStateStorage();
        bool bAppliedCapturedDock = false;
        if (LayoutState && LayoutState->bRestoreCapturedLayoutNextFrame &&
            LayoutState->RestoreCapturedLayoutFramesRemaining > 0 && !LayoutState->bApplyingRestore)
        {
            const auto It = LayoutState->PanelDockIds.find(EffectiveStableId);
            if (It != LayoutState->PanelDockIds.end() && It->second != 0)
            {
                // Do not require DockBuilderGetNode(It->second) here.
                // When switching document tabs, ImGui may recreate the dock node later in
                // the same frame. Falling back to the root DockSpace in that case is what
                // makes panels reappear as undocked/floating windows. Re-apply the last
                // captured DockId for one frame and let ImGui resolve/recreate the relation.
                ImGui::SetNextWindowDockID(It->second, ImGuiCond_Always);
                bAppliedCapturedDock = true;
            }
        }

        if (!bAppliedCapturedDock)
        {
            const ImGuiID TargetDockspaceId = Desc.DockspaceId != 0 ? Desc.DockspaceId : GetCurrentDockspaceIdStorage();
            if (TargetDockspaceId != 0)
            {
                ImGui::SetNextWindowDockID(TargetDockspaceId, Desc.DockCond);
            }
        }

        // DockNode 오른쪽 끝의 전역 X / menu 버튼은 제거한다.
        // 각 패널 탭의 닫기 X는 ImGui::Begin(..., p_open, ...) 경로에 맡긴다.
        ImGuiWindowClass PanelWindowClass{};
        PanelWindowClass.DockNodeFlagsOverrideSet =
            ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;
        ImGui::SetNextWindowClass(&PanelWindowClass);

        bool bTempOpen = true;
        bool *OpenPtr = Desc.bClosable ? (Desc.bOpen ? Desc.bOpen : &bTempOpen) : nullptr;
        const std::string Title = MakeTitle(Desc);

        const bool bVisible = ImGui::Begin(Title.c_str(), OpenPtr, Desc.WindowFlags);
        BeginState.bDidBeginWindow = true;

        if (LayoutState && ImGui::GetCurrentWindowRead())
        {
            const ImGuiID CurrentDockId = ImGui::GetCurrentWindowRead()->DockId;
            if (CurrentDockId != 0)
            {
                LayoutState->PanelDockIds[EffectiveStableId] = CurrentDockId;
            }
        }

        PanelTitleUtils::PushPanelStyle();
        BeginState.bPushedStyle = true;
        GetBeginStateStack().push_back(BeginState);

        const char *DecorationIconKey = (Desc.bDrawTitleIcon && Desc.IconKey) ? Desc.IconKey : nullptr;
        PanelTitleUtils::QueuePanelDecoration(DecorationIconKey, nullptr);

        if (bVisible)
        {
            if (Desc.bApplyContentTopInset)
            {
                PanelTitleUtils::ApplyPanelContentTopInset(Desc.bApplySideInset, Desc.bApplyBottomInset);
            }
        }

        return bVisible;
    }


    static void RenderDockKeepAliveWindows(FDockPanelLayoutState *LayoutState)
    {
        if (!LayoutState || LayoutState->PanelDockIds.empty())
        {
            return;
        }

        ImGuiWindowClass PanelWindowClass{};
        PanelWindowClass.DockNodeFlagsOverrideSet =
            ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;
        ImGui::SetNextWindowClass(&PanelWindowClass);

        const ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar |
                                      ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse |
                                      ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoInputs |
                                      ImGuiWindowFlags_NoNav |
                                      ImGuiWindowFlags_NoFocusOnAppearing |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus |
                                      ImGuiWindowFlags_NoBackground;

        LayoutState->bApplyingRestore = true;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        for (const auto &Pair : LayoutState->PanelDockIds)
        {
            if (Pair.first.empty() || Pair.second == 0)
            {
                continue;
            }

            ImGui::SetNextWindowDockID(Pair.second, ImGuiCond_Always);
            const std::string HiddenTitle = std::string("###") + Pair.first;
            bool bOpen = true;
            if (ImGui::Begin(HiddenTitle.c_str(), &bOpen, Flags))
            {
            }
            ImGui::End();
        }

        ImGui::PopStyleVar(2);
        LayoutState->bApplyingRestore = false;
    }

    static void End()
    {
        std::vector<FBeginState> &Stack = GetBeginStateStack();
        if (Stack.empty())
        {
            return;
        }

        const FBeginState BeginState = Stack.back();
        Stack.pop_back();

        if (!BeginState.bDidBeginWindow)
        {
            return;
        }

        if (BeginState.bPushedStyle)
        {
            PanelTitleUtils::PopPanelStyle();
        }
        ImGui::End();
    }
};
