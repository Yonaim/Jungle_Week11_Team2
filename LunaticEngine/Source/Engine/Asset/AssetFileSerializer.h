#pragma once

#include "Core/CoreTypes.h"
#include <filesystem>

class UObject;
class UAssetData;

// .uasset 내부 root object type. 확장자는 컨테이너이고, 실제 타입은 이 값으로 구분한다.
enum class EAssetClassId : uint32
{
    Unknown = 0,
    StaticMesh,
    SkeletalMesh,
    Skeleton,
    Material,
    Texture,
    CameraModifierStack,
    AnimationClip,
    PoseAsset,
};

struct FAssetFileHeader
{
    uint32 Magic = 0;
    uint32 FileVersion = 0;
    EAssetClassId ClassId = EAssetClassId::Unknown;
    FString ClassName;
    FString AssetName;
    FString AssetGuid;
    uint32 DependencyCount = 0;
    uint64 BodyOffset = 0;
};

namespace FAssetFileSerializer
{
    static constexpr uint32 AssetMagic = 0x5453414A; // 'JAST' little-endian: Jungle Asset
    static constexpr uint32 AssetVersion = 4;

    EAssetClassId GetAssetClassIdFromClassName(const FString& ClassName);
    FString       GetClassNameFromAssetClassId(EAssetClassId ClassId);

    bool ReadAssetHeader(const std::filesystem::path& FilePath, FAssetFileHeader& OutHeader, FString* OutError = nullptr);

    bool     SaveObjectToAssetFile(const std::filesystem::path& FilePath, UObject* RootObject, FString* OutError = nullptr);
    UObject* LoadObjectFromAssetFile(const std::filesystem::path& FilePath, FString* OutError = nullptr);

    // CameraModifierStack 기존 호출부 호환용 wrapper.
    bool        SaveAssetToFile(const std::filesystem::path& FilePath, UAssetData* Asset, FString* OutError = nullptr);
    UAssetData* LoadAssetFromFile(const std::filesystem::path& FilePath, FString* OutError = nullptr);

    uint32 GetCurrentAssetSerializationVersion();
} // namespace FAssetFileSerializer
