#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"

#include <algorithm>
#include <climits>
#include <cstdlib>

void FSkeletalMeshSelectionManager::Reset()
{
    SelectedBoneIndices.clear();
    LastSelectedBoneIndex = -1;
}

void FSkeletalMeshSelectionManager::ClearSelection()
{
    Reset();
}

void FSkeletalMeshSelectionManager::SelectBone(int32 BoneIndex)
{
    SelectedBoneIndices.clear();

    if (IsValidBoneIndex(BoneIndex))
    {
        SelectedBoneIndices.push_back(BoneIndex);
        LastSelectedBoneIndex = BoneIndex;
    }
    else
    {
        LastSelectedBoneIndex = -1;
    }
}

void FSkeletalMeshSelectionManager::SelectBones(const TArray<int32>& BoneIndices)
{
    SelectedBoneIndices.clear();

    for (int32 BoneIndex : BoneIndices)
    {
        if (!IsValidBoneIndex(BoneIndex))
        {
            continue;
        }

        if (std::find(SelectedBoneIndices.begin(), SelectedBoneIndices.end(), BoneIndex) != SelectedBoneIndices.end())
        {
            continue;
        }

        SelectedBoneIndices.push_back(BoneIndex);
    }

    LastSelectedBoneIndex = SelectedBoneIndices.empty() ? -1 : SelectedBoneIndices.back();
}

void FSkeletalMeshSelectionManager::AddBone(int32 BoneIndex)
{
    if (!IsValidBoneIndex(BoneIndex))
    {
        return;
    }

    if (std::find(SelectedBoneIndices.begin(), SelectedBoneIndices.end(), BoneIndex) != SelectedBoneIndices.end())
    {
        LastSelectedBoneIndex = BoneIndex;
        return;
    }

    SelectedBoneIndices.push_back(BoneIndex);
    LastSelectedBoneIndex = BoneIndex;
}

void FSkeletalMeshSelectionManager::ToggleBone(int32 BoneIndex)
{
    if (!IsValidBoneIndex(BoneIndex))
    {
        return;
    }

    auto It = std::find(SelectedBoneIndices.begin(), SelectedBoneIndices.end(), BoneIndex);
    if (It != SelectedBoneIndices.end())
    {
        SelectedBoneIndices.erase(It);
        LastSelectedBoneIndex = SelectedBoneIndices.empty() ? -1 : SelectedBoneIndices.back();
        return;
    }

    SelectedBoneIndices.push_back(BoneIndex);
    LastSelectedBoneIndex = BoneIndex;
}

void FSkeletalMeshSelectionManager::DeselectBone(int32 BoneIndex)
{
    auto It = std::find(SelectedBoneIndices.begin(), SelectedBoneIndices.end(), BoneIndex);
    if (It == SelectedBoneIndices.end())
    {
        return;
    }

    SelectedBoneIndices.erase(It);
    LastSelectedBoneIndex = SelectedBoneIndices.empty() ? -1 : SelectedBoneIndices.back();
}

void FSkeletalMeshSelectionManager::SelectRange(int32 ClickedBoneIndex, const TArray<int32>& VisibleBoneOrder)
{
    if (!IsValidBoneIndex(ClickedBoneIndex))
    {
        return;
    }

    if (VisibleBoneOrder.empty())
    {
        SelectBone(ClickedBoneIndex);
        return;
    }

    int32 ClickedIndexInOrder = -1;
    int32 AnchorIndexInOrder = -1;

    for (int32 Index = 0; Index < static_cast<int32>(VisibleBoneOrder.size()); ++Index)
    {
        if (VisibleBoneOrder[Index] == ClickedBoneIndex)
        {
            ClickedIndexInOrder = Index;
        }

        if (VisibleBoneOrder[Index] == LastSelectedBoneIndex)
        {
            AnchorIndexInOrder = Index;
        }
    }

    if (ClickedIndexInOrder < 0)
    {
        SelectBone(ClickedBoneIndex);
        return;
    }

    if (AnchorIndexInOrder < 0)
    {
        AnchorIndexInOrder = ClickedIndexInOrder;
    }

    const int32 BeginIndex = (std::min)(AnchorIndexInOrder, ClickedIndexInOrder);
    const int32 EndIndex = (std::max)(AnchorIndexInOrder, ClickedIndexInOrder);

    SelectedBoneIndices.clear();
    for (int32 Index = BeginIndex; Index <= EndIndex; ++Index)
    {
        const int32 BoneIndex = VisibleBoneOrder[Index];
        if (IsValidBoneIndex(BoneIndex))
        {
            SelectedBoneIndices.push_back(BoneIndex);
        }
    }

    LastSelectedBoneIndex = ClickedBoneIndex;
}

void FSkeletalMeshSelectionManager::SelectAll(const TArray<int32>& BoneIndices)
{
    SelectBones(BoneIndices);
}

bool FSkeletalMeshSelectionManager::IsSelected(int32 BoneIndex) const
{
    return std::find(SelectedBoneIndices.begin(), SelectedBoneIndices.end(), BoneIndex) != SelectedBoneIndices.end();
}

int32 FSkeletalMeshSelectionManager::GetPrimaryBoneIndex() const
{
    if (SelectedBoneIndices.empty())
    {
        return -1;
    }

    // The primary gizmo target must be the bone the user most recently selected,
    // not the first item in the selection array. SelectAll/range selection often
    // puts root bone 0 at the front, and driving root makes the whole preview mesh
    // look like it is being transformed.
    if (LastSelectedBoneIndex >= 0 &&
        std::find(SelectedBoneIndices.begin(), SelectedBoneIndices.end(), LastSelectedBoneIndex) != SelectedBoneIndices.end())
    {
        return LastSelectedBoneIndex;
    }

    return SelectedBoneIndices.back();
}

void FSkeletalMeshSelectionManager::RemoveInvalidDuplicates()
{
    TArray<int32> UniqueBoneIndices;
    for (int32 BoneIndex : SelectedBoneIndices)
    {
        if (!IsValidBoneIndex(BoneIndex))
        {
            continue;
        }

        if (std::find(UniqueBoneIndices.begin(), UniqueBoneIndices.end(), BoneIndex) != UniqueBoneIndices.end())
        {
            continue;
        }

        UniqueBoneIndices.push_back(BoneIndex);
    }

    SelectedBoneIndices = UniqueBoneIndices;
    LastSelectedBoneIndex = SelectedBoneIndices.empty() ? -1 : SelectedBoneIndices.back();
}
