#include "PCH/LunaticPCH.h"
#include "Common/Viewport/EditorViewportClient.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Viewport/Viewport.h"
#include "EditorEngine.h"

#include "ImGui/imgui.h"

#include <algorithm>

namespace
{
bool TryConvertMouseToViewportPixel(const ImVec2 &MousePos, const FRect &ViewportScreenRect, const FViewport *Viewport,
                                    float FallbackWidth, float FallbackHeight, float &OutViewportX,
                                    float &OutViewportY)
{
    if (ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
    {
        return false;
    }

    const float LocalX = MousePos.x - ViewportScreenRect.X;
    const float LocalY = MousePos.y - ViewportScreenRect.Y;

    // Drag 중 뷰포트 밖으로 나가도 같은 screen-rect -> render-target 변환식을 유지한다.
    // 여기서 false를 반환하고 raw local 좌표로 fallback하면 viewport rect와 RT 크기가 다를 때 delta가 튄다.
    const float TargetWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : FallbackWidth;
    const float TargetHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : FallbackHeight;
    if (TargetWidth <= 0.0f || TargetHeight <= 0.0f)
    {
        return false;
    }

    const float ScaleX = TargetWidth / ViewportScreenRect.Width;
    const float ScaleY = TargetHeight / ViewportScreenRect.Height;
    OutViewportX = LocalX * ScaleX;
    OutViewportY = LocalY * ScaleY;
    return true;
}
} // namespace

void FEditorViewportClient::Init(FWindowsWindow *InWindow)
{
    Window = InWindow;
    CameraController.SetCamera(&ViewCamera);
}

void FEditorViewportClient::Shutdown()
{
    DeactivateEditorContext();
    Viewport = nullptr;
    LayoutWindow = nullptr;
    Window = nullptr;
}

void FEditorViewportClient::Tick(float DeltaTime)
{
    (void)DeltaTime;
}


bool FEditorViewportClient::IsLiveContextOwner() const
{
    const UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
    if (!EditorEngine)
    {
        return false;
    }

    return EditorEngine->GetActiveEditorViewportClient() == this;
}

bool FEditorViewportClient::CanProcessLiveContextWork() const
{
    return bEditorContextActive && IsLiveContextOwner();
}

void FEditorViewportClient::ActivateEditorContext()
{
    ++EditorContextEpoch;
    bEditorContextActive = true;
    SetActive(true);
    // Reactivation starts with a clean input/drag cache.  The derived viewport will
    // rebuild a target on its next live tick after it has confirmed ownership.
    GizmoManager.ResetVisualInteractionState();
}

void FEditorViewportClient::DeactivateEditorContext()
{
    // A tab/context switch is only a live-session detach. It must not write to the
    // target transform. CancelDrag() restores DragStartWorldMatrix, which is correct
    // for an explicit user cancel but dangerous for hidden-tab deactivation because
    // stale drag data can snap actors/components back to origin.
    GizmoManager.SetInteractionPolicy(EGizmoInteractionPolicy::VisualOnly);
    GizmoManager.AbortLiveInteractionWithoutApplying();

    bEditorContextActive = false;
    SetHovered(false);
    SetActive(false);
    ++EditorContextEpoch;
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
    bool bSizeChanged = false;

    if (InWidth > 0.0f && WindowWidth != InWidth)
    {
        WindowWidth = InWidth;
        bSizeChanged = true;
    }
    if (InHeight > 0.0f && WindowHeight != InHeight)
    {
        WindowHeight = InHeight;
        bSizeChanged = true;
    }

    // 에디터 뷰포트 카메라는 더 이상 UCameraComponent가 아니므로 리사이즈 알림이
    // 컴포넌트/World 경로로 전달되지 않는다. 여기서 공유 뷰포트 카메라의 종횡비를
    // 실제 ImGui 뷰포트 사각형과 동기화한다.
    //
    // 이 처리가 없으면 Asset Preview 뷰포트는 grid/gizmo 같은 헬퍼 씬 데이터를 현재
    // 렌더 타깃 크기에 맞춰 그리지만, 메시 카메라는 이전 종횡비를 유지해서
    // 씬 범위와 메시 범위가 서로 다르게 보일 수 있다.
    if (bSizeChanged && WindowWidth > 0.0f && WindowHeight > 0.0f)
    {
        ViewCamera.OnResize(static_cast<int32>(WindowWidth), static_cast<int32>(WindowHeight));
    }
}


void FEditorViewportClient::SetViewportScreenRect(const FRect &InRect)
{
    ViewportScreenRect = InRect;
    SetViewportSize(InRect.Width, InRect.Height);

    if (!Viewport)
    {
        return;
    }

    const uint32 NewWidth = static_cast<uint32>(InRect.Width > 0.0f ? InRect.Width : 0.0f);
    const uint32 NewHeight = static_cast<uint32>(InRect.Height > 0.0f ? InRect.Height : 0.0f);
    if (NewWidth > 0 && NewHeight > 0 && (NewWidth != Viewport->GetWidth() || NewHeight != Viewport->GetHeight()))
    {
        Viewport->RequestResize(NewWidth, NewHeight);
    }
}

void FEditorViewportClient::UpdateLayoutRect()
{
    if (!LayoutWindow)
    {
        return;
    }

    const FRect &R = LayoutWindow->GetRect();
    ViewportScreenRect = R;
    SetViewportSize(R.Width, R.Height);

    if (!Viewport)
    {
        return;
    }

    const uint32 SlotW = static_cast<uint32>(R.Width);
    const uint32 SlotH = static_cast<uint32>(R.Height);
    if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight()))
    {
        Viewport->RequestResize(SlotW, SlotH);
    }
}

void FEditorViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
    if (!Viewport || !Viewport->GetSRV())
    {
        return;
    }

    const FRect &R = ViewportScreenRect;
    if (R.Width <= 0 || R.Height <= 0)
    {
        return;
    }

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const ImVec2 Min(R.X, R.Y);
    const ImVec2 Max(R.X + R.Width, R.Y + R.Height);
    DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);

    if (!bIsActiveViewport)
    {
        return;
    }

    constexpr float ActiveBorderThickness = 4.0f;
    const float BorderInset = ActiveBorderThickness * 0.5f;
    if (R.Width > ActiveBorderThickness && R.Height > ActiveBorderThickness)
    {
        DrawList->AddRect(ImVec2(Min.x + BorderInset, Min.y + BorderInset),
                          ImVec2(Max.x - BorderInset, Max.y - BorderInset), IM_COL32(255, 165, 0, 220), 0.0f, 0,
                          ActiveBorderThickness);
    }

}

void FEditorViewportClient::RenderViewportTooltipBar() const
{
    const char *TooltipBarText = GetViewportTooltipBarText();
    if (!TooltipBarText || TooltipBarText[0] == '\0')
    {
        return;
    }

    const FRect &R = ViewportScreenRect;
    if (R.Width <= 0.0f || R.Height <= 0.0f)
    {
        return;
    }

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const ImVec2 TextSize = ImGui::CalcTextSize(TooltipBarText);
    const ImVec2 Padding(12.0f, 7.0f);
    const float  Margin = 12.0f;
    const float  BarHeight = TextSize.y + Padding.y * 2.0f;
    const float  MaxBarWidth = (std::max)(120.0f, R.Width - Margin * 2.0f);
    const float  DesiredBarWidth = TextSize.x + Padding.x * 2.0f;
    const float  BarWidth = (std::min)(MaxBarWidth, DesiredBarWidth);

    const ImVec2 BarMin(R.X + Margin, R.Y + R.Height - Margin - BarHeight);
    const ImVec2 BarMax(BarMin.x + BarWidth, BarMin.y + BarHeight);
    const ImVec2 TextMin(BarMin.x + Padding.x, BarMin.y + Padding.y);

    DrawList->AddRectFilled(BarMin, BarMax, IM_COL32(20, 22, 26, 210), 8.0f);
    DrawList->AddRect(BarMin, BarMax, IM_COL32(70, 74, 82, 220), 8.0f);
    DrawList->PushClipRect(BarMin, BarMax, true);
    DrawList->AddText(TextMin, IM_COL32(190, 196, 205, 255), TooltipBarText);
    DrawList->PopClipRect();
}

bool FEditorViewportClient::GetCursorViewportPosition(uint32 &OutX, uint32 &OutY) const
{
    const ImVec2 MousePos = ImGui::GetIO().MousePos;
    if (!CanProcessLiveContextWork())
    {
        return false;
    }

    if (!bIsHovered && !bIsActive)
    {
        return false;
    }

    float ViewportX = 0.0f;
    float ViewportY = 0.0f;
    if (!TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, ViewportX,
                                        ViewportY))
    {
        return false;
    }

    const float TargetWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
    const float TargetHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
    if (ViewportX < 0.0f || ViewportY < 0.0f || ViewportX >= TargetWidth || ViewportY >= TargetHeight)
    {
        return false;
    }

    OutX = static_cast<uint32>(ViewportX);
    OutY = static_cast<uint32>(ViewportY);
    return true;
}
