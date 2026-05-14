#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshPreviewViewport.h"

#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"
#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"
#include "Common/UI/Panels/Panel.h"
#include "Common/UI/Viewport/ViewportToolbar.h"
#include "Common/Viewport/EditorViewportPanel.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "Render/Pipeline/Renderer.h"
#include "UI/SWindow.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

FSkeletalMeshPreviewViewport::~FSkeletalMeshPreviewViewport()
{
    Shutdown();
}

void FSkeletalMeshPreviewViewport::Initialize(FWindowsWindow *InWindow, FRenderer *InRenderer)
{
    Window = InWindow;
    Renderer = InRenderer;
    EnsureViewportResources();
}

void FSkeletalMeshPreviewViewport::Shutdown()
{
    if (PreviewViewportClient)
    {
        PreviewViewportClient->Shutdown();
    }

    PreviewViewport.reset();
    PreviewViewportClient.reset();
    PreviewLayoutWindow.reset();

    Renderer = nullptr;
    Window = nullptr;
}

void FSkeletalMeshPreviewViewport::BindEditorContext(FSkeletalMeshEditorState& State,
                                                      FSkeletalMeshSelectionManager* SelectionManager)
{
    EnsureViewportResources();
    if (PreviewViewportClient)
    {
        PreviewViewportClient->BindEditorContext(&State, SelectionManager);
    }
}

void FSkeletalMeshPreviewViewport::ActivateEditorContext()
{
    EnsureViewportResources();
    if (PreviewViewportClient)
    {
        PreviewViewportClient->ActivateEditorContext();
    }
}

void FSkeletalMeshPreviewViewport::DeactivateEditorContext()
{
    if (PreviewViewportClient)
    {
        PreviewViewportClient->DeactivateEditorContext();
    }
}

void FSkeletalMeshPreviewViewport::Tick(float DeltaTime)
{
    if (PreviewViewportClient)
    {
        PreviewViewportClient->Tick(DeltaTime);
    }
}

void FSkeletalMeshPreviewViewport::EnsureViewportResources()
{
    if (!Renderer || PreviewViewportClient)
    {
        return;
    }

    PreviewViewportClient = std::make_unique<FSkeletalMeshPreviewViewportClient>();
    PreviewViewportClient->Init(Window);

    PreviewViewport = std::make_unique<FViewport>();
    PreviewViewport->Initialize(Renderer->GetFD3DDevice().GetDevice(), 512, 512);
    PreviewViewport->SetClient(PreviewViewportClient.get());
    PreviewViewportClient->SetViewport(PreviewViewport.get());

    PreviewLayoutWindow = std::make_unique<SWindow>();
    PreviewViewportClient->SetLayoutWindow(PreviewLayoutWindow.get());
}

void FSkeletalMeshPreviewViewport::Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State,
                                          FSkeletalMeshSelectionManager *SelectionManager,
                                          FSkeletalMeshEditorToolbar *Toolbar, float DeltaTime, const FPanelDesc &PanelDesc)
{
    EnsureViewportResources();

    if (FPanel::Begin(PanelDesc))
    {
        RenderViewportPanel(Mesh, State, SelectionManager, Toolbar, DeltaTime);
    }
    FPanel::End();
}

void FSkeletalMeshPreviewViewport::RenderViewportPanel(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State,
                                                       FSkeletalMeshSelectionManager *SelectionManager,
                                                       FSkeletalMeshEditorToolbar *Toolbar, float DeltaTime)
{
    (void)DeltaTime;

    if (!PreviewViewportClient)
    {
        ImGui::TextDisabled("Preview viewport client is not initialized.");
        return;
    }

    PreviewViewportClient->SetPreviewMesh(Mesh);
    PreviewViewportClient->SetEditorState(&State);
    PreviewViewportClient->SetSelectionManager(SelectionManager);

    if (Toolbar)
    {
        if (FViewportToolbar::Begin("##SkeletalMeshViewportToolbar", FViewportToolbar::GetHeight(ImGui::GetContentRegionAvail().x)))
        {
            Toolbar->RenderViewportToolbar(Mesh, State, Renderer);
        }
        FViewportToolbar::End();
    }

    const bool bActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    FEditorViewportPanel::RenderViewportClient(*PreviewViewportClient, bActive);
}
