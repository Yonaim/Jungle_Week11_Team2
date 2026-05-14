#pragma once

#include "Core/CoreTypes.h"

// Skeletal Mesh Editor 전용 선택 관리자.
// Skeleton Tree / Asset Details / Preview Viewport가 같은 Bone 선택 상태를 공유하도록 관리한다.
// 단일 선택, Ctrl 다중 선택, Shift 범위 선택을 지원하며 마지막 선택 Bone을 Range Anchor로 사용한다.
class FSkeletalMeshSelectionManager
{
public:
    void Reset();
    void ClearSelection();

    void SelectBone(int32 BoneIndex);
    void SelectBones(const TArray<int32>& BoneIndices);
    void AddBone(int32 BoneIndex);
    void ToggleBone(int32 BoneIndex);
    void DeselectBone(int32 BoneIndex);
    void SelectRange(int32 ClickedBoneIndex, const TArray<int32>& VisibleBoneOrder);
    void SelectAll(const TArray<int32>& BoneIndices);

    bool IsSelected(int32 BoneIndex) const;
    bool IsEmpty() const { return SelectedBoneIndices.empty(); }
    int32 GetPrimaryBoneIndex() const;
    int32 GetLastSelectedBoneIndex() const { return LastSelectedBoneIndex; }
    int32 GetSelectedCount() const { return static_cast<int32>(SelectedBoneIndices.size()); }
    const TArray<int32>& GetSelectedBoneIndices() const { return SelectedBoneIndices; }

private:
    bool IsValidBoneIndex(int32 BoneIndex) const { return BoneIndex >= 0; }
    void RemoveInvalidDuplicates();

private:
    TArray<int32> SelectedBoneIndices;
    int32 LastSelectedBoneIndex = -1;
};
