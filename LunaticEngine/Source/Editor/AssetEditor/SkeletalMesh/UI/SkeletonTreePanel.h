#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"
#include "Common/UI/Panels/Panel.h"

#include "ImGui/imgui.h"

#include <vector>
#include <unordered_set>

class FSkeletalMeshSelectionManager;
class USkeletalMesh;
struct FBoneInfo;

// Skeletal Mesh Editor의 Skeleton Tree 패널.
// Outliner와 동일한 표 스타일을 쓰지 않고, Bone hierarchy가 잘 보이도록
// 단일 Name 컬럼, 들여쓰기, 가지선 중심의 트리 스타일을 사용한다.
// 선택 상태는 FSkeletalMeshSelectionManager가 소유하며, 이 패널은 선택을 변경하는 입력 UI 역할만 맡는다.
class FSkeletonTreePanel
{
public:
    void Render(USkeletalMesh* Mesh, FSkeletalMeshEditorState& State, FSkeletalMeshSelectionManager& SelectionManager,
                const FPanelDesc& PanelDesc);

private:
    void DrawBoneTreeNode(const TArray<FBoneInfo>& Bones, const TArray<TArray<int32>>& BoneChildren, int32 BoneIndex,
                          int32 Depth, bool bIsLastSibling, const std::vector<bool>& AncestorHasNextSibling,
                          FSkeletalMeshEditorState& State, FSkeletalMeshSelectionManager& SelectionManager,
                          const TArray<int32>& VisibleBoneOrder);

    void DrawFilteredBoneList(const TArray<FBoneInfo>& Bones, FSkeletalMeshEditorState& State,
                              FSkeletalMeshSelectionManager& SelectionManager, const TArray<int32>& VisibleBoneOrder);

    bool DrawBoneRow(const FBoneInfo& Bone, int32 BoneIndex, int32 Depth, bool bHasChildren, bool bFilteredList,
                     bool bIsLastSibling, const std::vector<bool>& AncestorHasNextSibling,
                     FSkeletalMeshEditorState& State, FSkeletalMeshSelectionManager& SelectionManager,
                     const TArray<int32>& VisibleBoneOrder);

    void ApplyBoneClickSelection(int32 BoneIndex, FSkeletalMeshEditorState& State,
                                 FSkeletalMeshSelectionManager& SelectionManager,
                                 const TArray<int32>& VisibleBoneOrder);
    void SyncLegacySelectedBoneIndex(FSkeletalMeshEditorState& State, const FSkeletalMeshSelectionManager& SelectionManager);

private:
    bool IsBoneExpanded(int32 BoneIndex) const;
    void ToggleBoneExpanded(int32 BoneIndex);

private:
    char SearchBuffer[128] = "";
    std::unordered_set<int32> CollapsedBoneIndices;
};
