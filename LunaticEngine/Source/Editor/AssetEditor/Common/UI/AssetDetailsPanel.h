#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"
#include "Common/UI/Panels/Panel.h"

#include <filesystem>

class FSkeletalMeshSelectionManager;
class USkeletalMesh;
class USkeletalMeshComponent;

// 에셋 에디터 전용 Asset Details 패널.
// SkeletalMesh Editor에서는 애셋 자체 정보와 Material Slots만 표시하고,
// 선택 Bone의 상세 transform은 별도 Details 패널에서 처리한다.
class FAssetDetailsPanel
{
  public:
    bool RenderSkeletalMesh(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent,
                            const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State,
                            FSkeletalMeshSelectionManager &SelectionManager, const FPanelDesc &PanelDesc);

  private:
    void RenderSearchToolbar();
    bool RenderMaterialSlots(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent, FSkeletalMeshEditorState &State);
    void RenderMeshInfo(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State,
                        FSkeletalMeshSelectionManager &SelectionManager);
    void RenderViewerActions(FSkeletalMeshEditorState &State, FSkeletalMeshSelectionManager &SelectionManager);
    void RenderBoneSelectionSummary(FSkeletalMeshEditorState &State, FSkeletalMeshSelectionManager &SelectionManager);
    void RenderPoseAssetTools(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent, const std::filesystem::path &AssetPath);
};
