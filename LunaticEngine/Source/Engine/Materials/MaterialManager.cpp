#include "PCH/LunaticPCH.h"
#include "MaterialManager.h"
#include <filesystem>
#include <fstream>
#include "Core/Log.h"
#include "Materials/Material.h"
#include "Platform/Paths.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Resource/Buffer.h"
#include "Texture/Texture2D.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Object/ObjectFactory.h"
#include "Render/Pipeline/Renderer.h"

#include <algorithm>
#include <vector>

namespace
{
	std::filesystem::path ResolveMaterialAssetDiskPath(const FString& MaterialPath)
	{
		const std::filesystem::path Resolved(FPaths::ResolvePathToDisk(MaterialPath));
		if (!Resolved.empty())
		{
			return Resolved.lexically_normal();
		}

		return std::filesystem::path(FPaths::ToWide(FPaths::NormalizePath(MaterialPath))).lexically_normal();
	}
}

void FMaterialManager::ScanMaterialAssets()
{
	AvailableMaterialFiles.clear();

	const std::filesystem::path MaterialRoots[] = {
		std::filesystem::path(FPaths::ContentDir()) / L"Materials",
		std::filesystem::path(FPaths::EngineContentDir()) / L"Materials"
	};
	std::vector<std::filesystem::path> UniqueRoots;

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const std::filesystem::path& MaterialRoot : MaterialRoots)
	{
		const std::filesystem::path NormalizedRoot = MaterialRoot.lexically_normal();
		if (std::find(UniqueRoots.begin(), UniqueRoots.end(), NormalizedRoot) != UniqueRoots.end())
		{
			continue;
		}
		UniqueRoots.push_back(NormalizedRoot);

		std::error_code ErrorCode;
		if (!std::filesystem::exists(NormalizedRoot, ErrorCode) || !std::filesystem::is_directory(NormalizedRoot, ErrorCode))
		{
			continue;
		}

		std::filesystem::recursive_directory_iterator It(
			NormalizedRoot,
			std::filesystem::directory_options::skip_permission_denied,
			ErrorCode);
		const std::filesystem::recursive_directory_iterator End;
		for (; It != End; It.increment(ErrorCode))
		{
			if (ErrorCode)
			{
				ErrorCode.clear();
				continue;
			}

			const std::filesystem::directory_entry& Entry = *It;
			if (!Entry.is_regular_file(ErrorCode))
			{
				ErrorCode.clear();
				continue;
			}

			const std::filesystem::path& Path = Entry.path();

			if (Path.extension() != L".uasset") continue;

			FAssetFileHeader Header;
			if (!FAssetFileSerializer::ReadAssetHeader(Path, Header)) continue;
			if (Header.ClassId != EAssetClassId::Material) continue;
			if (Path.stem() == L"None") continue; // Fallback 머티리얼은 목록에서 제외

			FMaterialAssetListItem Item;
			Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
			Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
			AvailableMaterialFiles.push_back(std::move(Item));
		}
	}

	std::sort(
		AvailableMaterialFiles.begin(),
		AvailableMaterialFiles.end(),
		[](const FMaterialAssetListItem& A, const FMaterialAssetListItem& B)
		{
			return A.DisplayName < B.DisplayName;
		});
}

UTexture2D* FMaterialManager::GetMaterialPreviewTexture(UMaterial* Material) const
{
	if (!Material)
	{
		return nullptr;
	}

	UTexture2D* PreviewTexture = nullptr;
	static const char* PreferredSlots[] = {
		"DiffuseTexture",
		"EmissiveTexture",
		"Custom0Texture",
		"Custom1Texture",
		"NormalTexture"
	};

	for (const char* SlotName : PreferredSlots)
	{
		if (Material->GetTextureParameter(SlotName, PreviewTexture) && PreviewTexture)
		{
			return PreviewTexture;
		}
	}

	TMap<FString, UTexture2D*>* Textures = Material->GetTexture();
	if (!Textures)
	{
		return nullptr;
	}

	for (const auto& Pair : *Textures)
	{
		if (Pair.second)
		{
			return Pair.second;
		}
	}

	return nullptr;
}

UTexture2D* FMaterialManager::GetMaterialPreviewTexture(const FString& MaterialPath)
{
	return GetMaterialPreviewTexture(GetOrCreateMaterial(MaterialPath));
}

UMaterial* FMaterialManager::GetOrCreateMaterial(const FString& MaterialPath)
{
	FString GenericPath = FPaths::NormalizePath(MaterialPath);
	if (MaterialPath == "None" || MaterialPath.empty())
	{
		GenericPath = "None";
	}
	std::filesystem::path Path(FPaths::ToWide(GenericPath));

	auto It = MaterialCache.find(GenericPath);
	if (It != MaterialCache.end())
	{
		return It->second;
	}

	if (Path.extension() == L".uasset")
	{
		const std::filesystem::path DiskPath = ResolveMaterialAssetDiskPath(GenericPath);
		FString Error;
		UObject* LoadedObject = FAssetFileSerializer::LoadObjectFromAssetFile(DiskPath, &Error);
		UMaterial* LoadedMaterial = Cast<UMaterial>(LoadedObject);
		if (LoadedMaterial)
		{
			LoadedMaterial->SetAssetPathFileName(GenericPath);
			LoadedMaterial->RebuildCachedSRVs();
			MaterialCache.emplace(GenericPath, LoadedMaterial);
			return LoadedMaterial;
		}

		if (LoadedObject)
		{
			UObjectManager::Get().DestroyObject(LoadedObject);
		}

		UE_LOG_CATEGORY(Material, Warning,
			"Material asset load failed. path=%s resolved=%s error=%s",
			GenericPath.c_str(),
			FPaths::ToUtf8(DiskPath.generic_wstring()).c_str(),
			Error.c_str());
	}

	// .uasset이 없거나 None인 경우 기본 머티리얼 생성.
	UMaterial* DefaultMaterial = UObjectManager::Get().CreateObject<UMaterial>();
	FMaterialTemplate* Template = GetOrCreateTemplate(DefaultShaderPath);
	if (!Template)
	{
		UE_LOG_CATEGORY(Material, Error,
			"Failed to create fallback material because default shader template could not be loaded: %s",
			DefaultShaderPath.c_str());
		UObjectManager::Get().DestroyObject(DefaultMaterial);
		return nullptr;
	}
	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> Buffers = CreateConstantBuffers(Template);
	DefaultMaterial->Create(GenericPath, Template, ERenderPass::Opaque, EBlendState::Opaque,
		EDepthStencilState::Default, ERasterizerState::SolidBackCull, std::move(Buffers), DefaultShaderPath);
	DefaultMaterial->SetVector4Parameter("SectionColor", FVector4(1.0f, 0.0f, 1.0f, 1.0f));
	MaterialCache.emplace(GenericPath, DefaultMaterial);
	return DefaultMaterial;
}

UMaterial* FMaterialManager::CreateMaterialAssetFromJson(const FString& AssetPath, json::JSON& JsonData)
{
	const FString GenericPath = FPaths::NormalizePath(AssetPath);
	std::filesystem::path Path(FPaths::ToWide(GenericPath));

	FString PathFileName = JsonData[MatKeys::PathFileName].ToString().c_str();
	FString ShaderPath = JsonData[MatKeys::ShaderPath].ToString().c_str();
	if (ShaderPath.empty())
	{
		ShaderPath = DefaultShaderPath;
	}

	FString RenderPassStr = JsonData[MatKeys::RenderPass].ToString().c_str();
	ERenderPass RenderPass = StringToRenderPass(RenderPassStr);

	FString BlendStr = JsonData.hasKey(MatKeys::BlendState) ? JsonData[MatKeys::BlendState].ToString().c_str() : "";
	FString DepthStr = JsonData.hasKey(MatKeys::DepthStencilState) ? JsonData[MatKeys::DepthStencilState].ToString().c_str() : "";
	FString RasterStr = JsonData.hasKey(MatKeys::RasterizerState) ? JsonData[MatKeys::RasterizerState].ToString().c_str() : "";

	EBlendState BlendState = StringToBlendState(BlendStr, RenderPass);
	EDepthStencilState DepthState = StringToDepthStencilState(DepthStr, RenderPass);
	ERasterizerState RasterState = StringToRasterizerState(RasterStr, RenderPass);

	FMaterialTemplate* Template = GetOrCreateTemplate(ShaderPath);
	if (!Template) return nullptr;

	auto InjectedBuffers = CreateConstantBuffers(Template);

	UMaterial* Material = nullptr;
	auto It = MaterialCache.find(GenericPath);
	if (It != MaterialCache.end())
	{
		Material = It->second;
	}
	else
	{
		Material = UObjectManager::Get().CreateObject<UMaterial>();
		Material->SetFName(FName(FPaths::ToUtf8(Path.stem().wstring())));
		MaterialCache.emplace(GenericPath, Material);
	}

	Material->Create(PathFileName.empty() ? GenericPath : FPaths::NormalizePath(PathFileName), Template, RenderPass, BlendState, DepthState, RasterState, std::move(InjectedBuffers), ShaderPath);

	InjectDefaultParameters(JsonData, Template, Material);
	PurgeStaleParameters(JsonData, Template);
	ApplyParameters(Material, JsonData);
	ApplyTextures(Material, JsonData);
	Material->RebuildCachedSRVs();

	std::filesystem::create_directories(Path.parent_path());
	FString Error;
	FAssetFileSerializer::SaveObjectToAssetFile(Path, Material, &Error);
	return Material;
}

json::JSON FMaterialManager::ReadJsonFile(const FString& FilePath) const
{
	std::ifstream File(FPaths::ToWide(FilePath).c_str());
	if (!File.is_open()) return json::JSON(); // Null JSON 반환

	std::stringstream Buffer;
	Buffer << File.rdbuf();
	return json::JSON::Load(Buffer.str());
}

TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> FMaterialManager::CreateConstantBuffers(FMaterialTemplate* Template)
{

	TMap<FString, std::unique_ptr<FMaterialConstantBuffer>> InjectedBuffers;
	if (!Template)
	{
		return InjectedBuffers;
	}

	const auto& RequiredBuffers = Template->GetParameterInfo();
	std::vector<FString> CreatedBuffers;

	for (const auto& BufferInfo : RequiredBuffers)
	{
		const FMaterialParameterInfo* ParamInfo = BufferInfo.second;

		if (std::find(CreatedBuffers.begin(), CreatedBuffers.end(), ParamInfo->BufferName) != CreatedBuffers.end())
			continue;

		auto MatCB = std::make_unique<FMaterialConstantBuffer>();
		MatCB->Init(Device, ParamInfo->BufferSize, ParamInfo->SlotIndex);

		InjectedBuffers.emplace(ParamInfo->BufferName, std::move(MatCB));
		CreatedBuffers.push_back(ParamInfo->BufferName);
	}

	return InjectedBuffers;
}

void FMaterialManager::ApplyParameters(UMaterial* Material, json::JSON& JsonData)
{
	if (!JsonData.hasKey(MatKeys::Parameters)) return;

	for (auto& Pair : JsonData[MatKeys::Parameters].ObjectRange())
	{
		FString ParamName = Pair.first.c_str();
		json::JSON& Value = Pair.second;

		if (Value.JSONType() == json::JSON::Class::Array)
		{
			if (Value.length() == 3)
			{
				Material->SetVector3Parameter(ParamName, FVector((float)Value[0].ToFloat(), (float)Value[1].ToFloat(), (float)Value[2].ToFloat()));
			}
			else if (Value.length() == 4)
			{
				Material->SetVector4Parameter(ParamName, FVector4((float)Value[0].ToFloat(), (float)Value[1].ToFloat(), (float)Value[2].ToFloat(), (float)Value[3].ToFloat()));
			}
		}
		else if (Value.JSONType() == json::JSON::Class::Floating || Value.JSONType() == json::JSON::Class::Integral)
		{
			Material->SetScalarParameter(ParamName, (float)Value.ToFloat());
		}
	}
}

void FMaterialManager::ApplyTextures(UMaterial* Material, json::JSON& JsonData)
{
	if (!JsonData.hasKey(MatKeys::Textures)) return;

	for (auto& Pair : JsonData[MatKeys::Textures].ObjectRange())
	{
		FString SlotName = Pair.first.c_str();
		FString TexturePath = FPaths::NormalizePath(Pair.second.ToString().c_str());

		UTexture2D* Texture = UTexture2D::LoadFromFile(TexturePath, Device);
		if (Texture)
		{
			Material->SetTextureParameter(SlotName, Texture);
		}
	}
}


ERenderPass FMaterialManager::StringToRenderPass(const FString& Str) const
{
	using namespace RenderStateStrings;
	return FromString(RenderPassMap, Str, ERenderPass::Opaque);
}

EBlendState FMaterialManager::StringToBlendState(const FString& Str, ERenderPass Pass) const
{
	using namespace RenderStateStrings;
	if (!Str.empty())
		return FromString(BlendStateMap, Str, EBlendState::Opaque);

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::AlphaBlend:
	case ERenderPass::WorldText:
	case ERenderPass::Decal:
	case ERenderPass::EditorLines:
	case ERenderPass::EditorGrid:
	case ERenderPass::PostProcess:
	case ERenderPass::GizmoInner:
	case ERenderPass::ScreenText:
		return EBlendState::AlphaBlend;
	case ERenderPass::AdditiveDecal:
		return EBlendState::Additive;
	case ERenderPass::SelectionMask:
		return EBlendState::NoColor;
	default:
		return EBlendState::Opaque;
	}
}

EDepthStencilState FMaterialManager::StringToDepthStencilState(const FString& Str, ERenderPass Pass) const
{
	using namespace RenderStateStrings;
	if (!Str.empty())
		return FromString(DepthStencilStateMap, Str, EDepthStencilState::Default);

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::WorldText:
		return EDepthStencilState::Default;
	case ERenderPass::Decal:
	case ERenderPass::AdditiveDecal:
		return EDepthStencilState::DepthReadOnly;
	case ERenderPass::SelectionMask:
		return EDepthStencilState::StencilWrite;
	case ERenderPass::PostProcess:
	case ERenderPass::EditorGrid:
	case ERenderPass::ScreenText:
		return EDepthStencilState::NoDepth;
	case ERenderPass::GizmoOuter:
		return EDepthStencilState::GizmoOutside;
	case ERenderPass::GizmoInner:
		return EDepthStencilState::GizmoInside;
	default:
		return EDepthStencilState::Default;
	}
}

ERasterizerState FMaterialManager::StringToRasterizerState(const FString& Str, ERenderPass Pass) const
{
	using namespace RenderStateStrings;
	if (!Str.empty())
		return FromString(RasterizerStateMap, Str, ERasterizerState::SolidBackCull);

	// 문자열이 비어있으면 Pass 기반 기본값
	switch (Pass)
	{
	case ERenderPass::Decal:
	case ERenderPass::AdditiveDecal:
	case ERenderPass::SelectionMask:
	case ERenderPass::PostProcess:
	case ERenderPass::EditorGrid:
		return ERasterizerState::SolidNoCull;
	default:
		return ERasterizerState::SolidBackCull;
	}
}

void FMaterialManager::SaveToJSON(json::JSON& JsonData, const FString& MatFilePath)
{
	std::ofstream File(FPaths::ToWide(MatFilePath));
	File << JsonData.dump();
}

bool FMaterialManager::InjectDefaultParameters(json::JSON& JsonData, FMaterialTemplate* Template, UMaterial* Material)
{
	const auto& Layout = Template->GetParameterInfo();
	bool bInjected = false;

	for (const auto& Pair : Layout)
	{
		const FString& ParamName = Pair.first;
		const FMaterialParameterInfo* Info = Pair.second;

		// 이미 JSON에 있으면 스킵
		if (!JsonData[MatKeys::Parameters][ParamName].IsNull())
			continue;

		bInjected = true;

		switch (Info->Size)
		{
			case sizeof(float) : // 4바이트 - Scalar
			{
				float Value = 0.f;
				Material->GetScalarParameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = Value;
				break;
			}
			case sizeof(float) * 3: // 12바이트 - Vector3
			{
				FVector Value;
				Material->GetVector3Parameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = json::Array(Value.X, Value.Y, Value.Z);
				break;
			}
			case sizeof(float) * 4: // 16바이트 - Vector4
			{
				FVector4 Value;
				Material->GetVector4Parameter(ParamName, Value);
				JsonData[MatKeys::Parameters][ParamName] = json::Array(Value.X, Value.Y, Value.Z, Value.W);
				break;
			}
			case sizeof(float) * 16: // 64바이트 - Matrix
			{
				FMatrix Value;
				Material->GetMatrixParameter(ParamName, Value);
				auto MatArray = json::Array();
				for (int i = 0; i < 16; ++i)
					MatArray.append(Value.Data[i]);
				JsonData[MatKeys::Parameters][ParamName] = MatArray;
				break;
			}
			default:
				break; // uint, bool 등 특수 케이스는 별도 처리 필요
		}
	}

	return bInjected;
}

bool FMaterialManager::PurgeStaleParameters(json::JSON& JsonData, FMaterialTemplate* Template)
{
	if (!JsonData.hasKey(MatKeys::Parameters)) return false;

	const auto& Layout = Template->GetParameterInfo();
	json::JSON CleanParams = json::JSON::Make(json::JSON::Class::Object);
	bool bPurged = false;

	for (auto& Pair : JsonData[MatKeys::Parameters].ObjectRange())
	{
		FString ParamName = Pair.first.c_str();
		if (Layout.find(ParamName) != Layout.end())
		{
			CleanParams[Pair.first] = Pair.second;
		}
		else
		{
			bPurged = true;
		}
	}

	if (bPurged)
	{
		JsonData[MatKeys::Parameters] = std::move(CleanParams);
	}

	return bPurged;
}

FMaterialTemplate* FMaterialManager::GetOrCreateTemplate(const FString& ShaderPath)
{
	// 1. 템플릿이 캐시에 있는지 확인 (셰이더 경로를 키값으로 사용)
	auto It = TemplateCache.find(ShaderPath);
	if (It != TemplateCache.end())
	{
		return It->second;
	}

	// 2. 템플릿이 기존에 없다면 새로 제작
	//    캐시에 있으면 반환, 없으면 컴파일 후 캐싱
	FShader* Shader = FShaderManager::Get().FindOrCreate(ShaderPath);
	if (!Shader)
	{
		return nullptr;
	}

	FMaterialTemplate* NewTemplate = new FMaterialTemplate();
	NewTemplate->Create(Shader, ShaderPath);
	TemplateCache.emplace(ShaderPath, NewTemplate);
	return NewTemplate;
}

FMaterialManager::~FMaterialManager()
{
	if (!Device)
	{
		Release();
	}

}

void FMaterialManager::Release()
{
	// 1. TemplateCache 메모리 해제
	// GetOrCreateTemplate()에서 new FMaterialTemplate()로 직접 할당했으므로 여기서 delete 해줍니다.
	for (auto& Pair : TemplateCache)
	{
		if (Pair.second != nullptr)
		{
			delete Pair.second;
			Pair.second = nullptr;
		}
	}
	TemplateCache.clear();

	// 2. MaterialCache — UMaterial은 UObjectManager가 수명을 관리하므로 캐시 맵만 비움
	MaterialCache.clear();

	// 3. Device 참조 해제
	// 외부에서 주입받은 리소스이므로 포인터만 초기화합니다.
	Device = nullptr;
}
