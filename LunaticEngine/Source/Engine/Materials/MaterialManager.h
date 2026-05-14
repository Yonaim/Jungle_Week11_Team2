#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"
#include "Engine/Core/SimpleJsonWrapper.h"
#include <memory>

#include "Render/Types/RenderStateTypes.h"

namespace MatKeys
{
	static constexpr const char* PathFileName = "PathFileName";
	static constexpr const char* ShaderPath = "ShaderPath";
	static constexpr const char* RenderPass = "RenderPass";
	static constexpr const char* BlendState = "BlendState";
	static constexpr const char* DepthStencilState = "DepthStencilState";
	static constexpr const char* RasterizerState = "RasterizerState";
	static constexpr const char* Parameters = "Parameters";
	static constexpr const char* Textures = "Textures";
}

class FMaterialTemplate;
class UMaterial;
class UTexture2D;
struct FMaterialConstantBuffer;

struct FMaterialAssetListItem
{
	FString DisplayName;
	FString FullPath;
};

class FMaterialManager : public TSingleton<FMaterialManager>
{
	friend class TSingleton<FMaterialManager>;

    TMap<FString, FMaterialTemplate*> TemplateCache;    // 셰이더 경로 → Template (공유)
	TMap<FString, UMaterial*> MaterialCache;	//MatFilePath
	TArray<FMaterialAssetListItem> AvailableMaterialFiles;

	ID3D11Device* Device = nullptr;

public:
	~FMaterialManager(); // 선언만 남김

	void Initialize(ID3D11Device* InDevice) { Device = InDevice; }

	// 지정된 디렉토리 내의 모든 머티리얼을 미리 로드
	void LoadAllMaterials(ID3D11Device* Device);

    // UMaterial 생성
	UMaterial* GetOrCreateMaterial(const FString& MaterialPath);
	UMaterial* CreateMaterialAssetFromJson(const FString& AssetPath, json::JSON& JsonData);

	void ScanMaterialAssets();
	const TArray<FMaterialAssetListItem>& GetAvailableMaterialFiles() const { return AvailableMaterialFiles; }
	UTexture2D* GetMaterialPreviewTexture(UMaterial* Material) const;
	UTexture2D* GetMaterialPreviewTexture(const FString& MaterialPath);

	void Release();

	// .uasset 로딩 중 UMaterial::Serialize가 Template/CB를 재구성할 때도 사용한다.
	FMaterialTemplate* GetOrCreateTemplate(const FString& ShaderPath);
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> CreateConstantBuffers(FMaterialTemplate* Template);

private:
	json::JSON ReadJsonFile(const FString& FilePath) const;
	void ApplyParameters(UMaterial* Material, json::JSON& JsonData);
	void ApplyTextures(UMaterial* Material, json::JSON& JsonData);

	ERenderPass StringToRenderPass(const FString& Str) const;
	EBlendState StringToBlendState(const FString& Str, ERenderPass Pass) const;
	EDepthStencilState StringToDepthStencilState(const FString& Str, ERenderPass Pass) const;
	ERasterizerState StringToRasterizerState(const FString& Str, ERenderPass Pass) const;
	void SaveToJSON(json::JSON& JsonData, const FString& MatFilePath);

	bool InjectDefaultParameters(json::JSON& JsonData, FMaterialTemplate* Template, UMaterial* Material);
	bool PurgeStaleParameters(json::JSON& JsonData, FMaterialTemplate* Template);
	
	const FString DefaultShaderPath = "Shaders/Geometry/UberLit.hlsl";


};
