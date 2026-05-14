#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"

class USkeletalMesh;
class FRenderer;

/**
 * SkeletalMesh Editor의 상단 Toolbar 패널.
 *
 * 역할:
 * - Reference Pose / Skinned Pose 표시 모드 전환
 * - Bone 표시 여부 토글
 * - Pose Edit Mode placeholder 제공
 *
 * 실제 Pose 편집 기능은 김형도 담당 영역이므로 여기서는 상태 토글까지만 둔다.
 */
class FSkeletalMeshEditorToolbar
{
  public:
    void RenderViewportToolbar(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, FRenderer *Renderer);
};
