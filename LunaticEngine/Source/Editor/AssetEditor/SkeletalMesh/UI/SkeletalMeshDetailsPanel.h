#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"
#include "Common/UI/Panels/Panel.h"

class FSkeletalMeshSelectionManager;
class FSkeletalMeshPreviewPoseController;
class USkeletalMesh;
class USkeletalMeshComponent;

// Skeletal Mesh Editor의 선택 Details 패널.
// Asset Details가 애셋 자체 정보를 보여준다면, 이 패널은 현재 선택한 Bone의 정보를 보여준다.
class FSkeletalMeshDetailsPanel
{
  public:
    void Render(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent, FSkeletalMeshPreviewPoseController *PoseController,
                FSkeletalMeshEditorState &State,
                FSkeletalMeshSelectionManager &SelectionManager, const FPanelDesc &PanelDesc);

  private:
    void RenderSearchToolbar();
    void RenderBoneSection(USkeletalMesh *Mesh, FSkeletalMeshSelectionManager &SelectionManager);
    void RenderTransformSection(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent,
                                FSkeletalMeshPreviewPoseController *PoseController,
                                FSkeletalMeshEditorState &State, FSkeletalMeshSelectionManager &SelectionManager);
};
