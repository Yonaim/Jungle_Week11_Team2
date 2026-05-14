#include "PCH/LunaticPCH.h"
#include "Mesh/FbxCommon.h"
#include "Materials/MaterialManager.h"

#include "Core/Log.h"
#include "Engine/Platform/Paths.h"
#include "Engine/Core/SimpleJsonWrapper.h"
#include "Texture/Texture2D.h"

#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cwctype>

#if defined(_WIN64)

namespace
{
	FString FindTexturePathFromProperty(const FString& FbxFilePath, FbxSurfaceMaterial* FbxMaterial, const char* PropertyName)
	{
		if (!FbxMaterial || !PropertyName)
		{
			return "";
		}

		FbxProperty Property = FbxMaterial->FindProperty(PropertyName);
		if (!Property.IsValid())
		{
			return "";
		}

		const int32 TextureCount = Property.GetSrcObjectCount<FbxFileTexture>();
		if (TextureCount <= 0)
		{
			return "";
		}

		return FFbxCommon::ResolveFbxTexturePath(FbxFilePath, Property.GetSrcObject<FbxFileTexture>(0));
	}

	int32 GetPolygonVertexLinearIndex(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex)
	{
		if (!Mesh || PolygonIndex < 0 || CornerIndex < 0)
		{
			return -1;
		}

		int32 LinearIndex = 0;
		for (int32 Index = 0; Index < PolygonIndex; ++Index)
		{
			LinearIndex += Mesh->GetPolygonSize(Index);
		}
		return LinearIndex + CornerIndex;
	}

	int32 ResolveLayerElementIndex(const FbxLayerElementTemplate<FbxVector4>* Element, int32 ElementIndex)
	{
		if (!Element || ElementIndex < 0)
		{
			return -1;
		}

		switch (Element->GetReferenceMode())
		{
		case FbxLayerElement::eDirect:
			return ElementIndex;
		case FbxLayerElement::eIndexToDirect:
			return ElementIndex < Element->GetIndexArray().GetCount() ? Element->GetIndexArray().GetAt(ElementIndex) : -1;
		default:
			return -1;
		}
	}

	bool IsFiniteVector(const FVector& Vector)
	{
		return std::isfinite(Vector.X) && std::isfinite(Vector.Y) && std::isfinite(Vector.Z);
	}

	void AddUniqueCandidatePath(std::vector<std::filesystem::path>& Candidates, const std::filesystem::path& Candidate)
	{
		if (Candidate.empty())
		{
			return;
		}

		const std::filesystem::path Normalized = Candidate.lexically_normal();
		if (std::find(Candidates.begin(), Candidates.end(), Normalized) == Candidates.end())
		{
			Candidates.push_back(Normalized);
		}
	}

	std::wstring ToLowerCopy(std::wstring Text)
	{
		std::transform(Text.begin(), Text.end(), Text.begin(), [](wchar_t Character)
		{
			return static_cast<wchar_t>(std::towlower(Character));
		});
		return Text;
	}

	void AddAncestorTextureRoots(std::vector<std::filesystem::path>& SearchRoots, const std::filesystem::path& StartPath)
	{
		std::filesystem::path Current = StartPath.lexically_normal();
		while (!Current.empty())
		{
			AddUniqueCandidatePath(SearchRoots, Current);
			AddUniqueCandidatePath(SearchRoots, Current / L"textures");
			AddUniqueCandidatePath(SearchRoots, Current / L"Textures");

			const std::filesystem::path Parent = Current.parent_path();
			if (Parent == Current)
			{
				break;
			}
			Current = Parent;
		}
	}

	void AddTrimmedRelativeCandidates(
		std::vector<std::filesystem::path>& Candidates,
		const std::vector<std::filesystem::path>& SearchRoots,
		const std::filesystem::path& RawPath)
	{
		if (RawPath.empty())
		{
			return;
		}

		std::vector<std::filesystem::path> SuffixPaths;
		AddUniqueCandidatePath(SuffixPaths, RawPath);

		std::vector<std::filesystem::path> Components;
		for (const auto& Part : RawPath)
		{
			if (Part.empty() || Part == L".")
			{
				continue;
			}
			SuffixPaths.reserve(SuffixPaths.size() + 1);
			Components.push_back(Part);
		}

		for (size_t StartIndex = 1; StartIndex < Components.size(); ++StartIndex)
		{
			std::filesystem::path Suffix;
			for (size_t Index = StartIndex; Index < Components.size(); ++Index)
			{
				Suffix /= Components[Index];
			}
			AddUniqueCandidatePath(SuffixPaths, Suffix);
		}

		for (const std::filesystem::path& SearchRoot : SearchRoots)
		{
			for (const std::filesystem::path& SuffixPath : SuffixPaths)
			{
				AddUniqueCandidatePath(Candidates, SearchRoot / SuffixPath);
			}
		}
	}

	std::filesystem::path FindTextureByFileName(
		const std::vector<std::filesystem::path>& SearchRoots,
		const std::wstring& FileName)
	{
		for (const std::filesystem::path& SearchRoot : SearchRoots)
		{
			std::error_code ErrorCode;
			if (!std::filesystem::exists(SearchRoot, ErrorCode))
			{
				continue;
			}

			for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(SearchRoot, std::filesystem::directory_options::skip_permission_denied, ErrorCode))
			{
				if (ErrorCode)
				{
					break;
				}
				if (Entry.is_regular_file() && ToLowerCopy(Entry.path().filename().wstring()) == ToLowerCopy(FileName))
				{
					return Entry.path().lexically_normal();
				}
			}
		}

		return {};
	}

	std::filesystem::path FindTextureByStemHeuristic(
		const std::vector<std::filesystem::path>& SearchRoots,
		const std::filesystem::path& RawPath)
	{
		const std::wstring TargetStem = ToLowerCopy(RawPath.stem().wstring());
		const std::wstring TargetExtension = ToLowerCopy(RawPath.extension().wstring());
		if (TargetStem.empty())
		{
			return {};
		}

		std::filesystem::path BestMatch;
		int32 BestScore = 0;

		for (const std::filesystem::path& SearchRoot : SearchRoots)
		{
			std::error_code ErrorCode;
			if (!std::filesystem::exists(SearchRoot, ErrorCode))
			{
				continue;
			}

			for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(SearchRoot, std::filesystem::directory_options::skip_permission_denied, ErrorCode))
			{
				if (ErrorCode)
				{
					break;
				}
				if (!Entry.is_regular_file())
				{
					continue;
				}

				const std::wstring CandidateStem = ToLowerCopy(Entry.path().stem().wstring());
				const std::wstring CandidateExtension = ToLowerCopy(Entry.path().extension().wstring());
				if (!TargetExtension.empty() && CandidateExtension != TargetExtension)
				{
					continue;
				}

				int32 Score = 0;
				if (CandidateStem == TargetStem)
				{
					Score = 4;
				}
				else if (CandidateStem.find(TargetStem) != std::wstring::npos || TargetStem.find(CandidateStem) != std::wstring::npos)
				{
					Score = 3;
				}
				else
				{
					continue;
				}

				if (Score > BestScore)
				{
					BestScore = Score;
					BestMatch = Entry.path().lexically_normal();
				}
			}
		}

		return BestMatch;
	}

	bool IsRelativeSubpath(const std::filesystem::path& RelativePath)
	{
		return !RelativePath.empty()
			&& RelativePath.native().find(L"..") != 0
			&& !RelativePath.is_absolute();
	}

	std::filesystem::path MakeImportedMaterialDirectory(const FString& SourceFilePath)
	{
		const std::filesystem::path ContentRoot = std::filesystem::path(FPaths::ContentDir()).lexically_normal();
		const std::filesystem::path SourceRoot = std::filesystem::path(FPaths::EngineSourceDir()).lexically_normal();
		const std::filesystem::path SourcePath = std::filesystem::path(FPaths::ToWide(SourceFilePath)).lexically_normal();
		const std::filesystem::path RelativeToSource = SourcePath.lexically_relative(SourceRoot);

		std::filesystem::path MaterialDirectory = ContentRoot / L"Materials";
		if (IsRelativeSubpath(RelativeToSource))
		{
			MaterialDirectory /= RelativeToSource.parent_path();
			MaterialDirectory /= RelativeToSource.stem();
		}

		return MaterialDirectory.lexically_normal();
	}
}

size_t FFbxVertexKeyHash::operator()(const FFbxVertexKey& Key) const noexcept
{
	size_t Seed = std::hash<int32>{}(Key.ControlPointIndex);
	Seed ^= std::hash<int32>{}(Key.PolygonIndex) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
	Seed ^= std::hash<int32>{}(Key.CornerIndex) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
	Seed ^= std::hash<int32>{}(Key.MaterialIndex) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
	return Seed;
}

bool FFbxCommon::ParseFbx(const FString& FbxFilePath, FFbxInfo& OutContext)
{
	OutContext.Manager = FbxManager::Create();
	if (!OutContext.Manager)
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to create FBX SDK manager.");
		return false;
	}

	FbxIOSettings* IOSettings = FbxIOSettings::Create(OutContext.Manager, IOSROOT);
	OutContext.Manager->SetIOSettings(IOSettings);

	FbxImporter* Importer = FbxImporter::Create(OutContext.Manager, "");
	if (!Importer->Initialize(FbxFilePath.c_str(), -1, OutContext.Manager->GetIOSettings()))
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to initialize FBX importer: %s", FbxFilePath.c_str());
		Importer->Destroy();
		Destroy(OutContext);
		return false;
	}

	OutContext.Scene = FbxScene::Create(OutContext.Manager, "ImportedFbxScene");
	if (!Importer->Import(OutContext.Scene))
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to import FBX scene: %s", FbxFilePath.c_str());
		Importer->Destroy();
		Destroy(OutContext);
		return false;
	}

	Importer->Destroy();

	const FbxAxisSystem TargetAxisSystem = FbxAxisSystem::MayaZUp;
	const FbxAxisSystem SourceAxisSystem = OutContext.Scene->GetGlobalSettings().GetAxisSystem();
	if (SourceAxisSystem != TargetAxisSystem)
	{
		TargetAxisSystem.ConvertScene(OutContext.Scene);
	}

	FbxGeometryConverter GeometryConverter(OutContext.Manager);
	GeometryConverter.Triangulate(OutContext.Scene, true);
	return true;
}

void FFbxCommon::Destroy(FFbxInfo& Context)
{
	if (Context.Manager)
	{
		Context.Manager->Destroy();
	}

	Context = FFbxInfo();
}

int32 FFbxCommon::GetPolygonMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex)
{
	FbxLayerElementMaterial* MaterialElement = Mesh->GetElementMaterial();
	if (!MaterialElement)
	{
		return 0;
	}

	if (MaterialElement->GetMappingMode() == FbxLayerElement::eAllSame)
	{
		return MaterialElement->GetIndexArray().GetCount() > 0 ? MaterialElement->GetIndexArray().GetAt(0) : 0;
	}

	if (MaterialElement->GetMappingMode() == FbxLayerElement::eByPolygon)
	{
		return MaterialElement->GetIndexArray().GetCount() > PolygonIndex ? MaterialElement->GetIndexArray().GetAt(PolygonIndex) : 0;
	}

	return 0;
}

int32 FFbxCommon::FindOrAddMaterial(const FString& FbxFilePath, FbxNode* Node, int32 LocalMaterialIndex, FFbxInfo& Context)
{
	FbxSurfaceMaterial* SourceMaterial = Node ? Node->GetMaterial(LocalMaterialIndex) : nullptr;
	if (!SourceMaterial)
	{
		if (Context.Materials.empty())
		{
			FFbxMaterialInfo DefaultMaterial;
			Context.Materials.push_back(DefaultMaterial);
		}
		return 0;
	}

	auto It = Context.MaterialMap.find(SourceMaterial);
	if (It != Context.MaterialMap.end())
	{
		return It->second;
	}

	FFbxMaterialInfo MaterialInfo;
	MaterialInfo.SourceMaterial = SourceMaterial;
	MaterialInfo.MaterialSlotName = SanitizeName(SourceMaterial->GetName());
	MaterialInfo.DiffuseTexturePath = FindDiffuseTexturePath(FbxFilePath, SourceMaterial);
	MaterialInfo.NormalTexturePath = FindNormalTexturePath(FbxFilePath, SourceMaterial);
	MaterialInfo.DiffuseColor = GetDiffuseColor(SourceMaterial);

	const int32 MaterialIndex = static_cast<int32>(Context.Materials.size());
	Context.Materials.push_back(MaterialInfo);
	Context.MaterialMap.emplace(SourceMaterial, MaterialIndex);
	return MaterialIndex;
}

FString FFbxCommon::ConvertMaterialInfoToMaterialAsset(const FString& FbxFilePath, const FFbxMaterialInfo& MaterialInfo)
{
	if (!MaterialInfo.SourceMaterial)
	{
		return "None";
	}

	const FString SlotAssetName = SanitizeName(MaterialInfo.MaterialSlotName.empty() ? FString("None") : MaterialInfo.MaterialSlotName);

	std::filesystem::path MaterialDirectory = MakeImportedMaterialDirectory(FbxFilePath);
	std::filesystem::path MaterialAssetPathW = MaterialDirectory / (L"M_" + FPaths::ToWide(SlotAssetName) + L".uasset");
	const FString MaterialAssetPath = MakeProjectRelativePath(MaterialAssetPathW);

	std::filesystem::create_directories(MaterialDirectory);

	json::JSON JsonData;
	JsonData["PathFileName"] = MaterialAssetPath;
	JsonData["Origin"] = "FbxImport";
	JsonData["SourceFilePath"] = MakeProjectRelativePath(std::filesystem::path(FPaths::ToWide(FbxFilePath)));
	JsonData["ShaderPath"] = "Shaders/Geometry/UberLit.hlsl";
	JsonData["RenderPass"] = "Opaque";

	const FString DiffuseTextureAssetPath = MaterialInfo.DiffuseTexturePath.empty()
		? FString()
		: UTexture2D::ImportTextureAsset(MaterialInfo.DiffuseTexturePath);
	const FString NormalTextureAssetPath = MaterialInfo.NormalTexturePath.empty()
		? FString()
		: UTexture2D::ImportTextureAsset(MaterialInfo.NormalTexturePath);

	if (!DiffuseTextureAssetPath.empty())
	{
		JsonData["Textures"]["DiffuseTexture"] = DiffuseTextureAssetPath;
		JsonData["Parameters"]["SectionColor"][0] = 1.0f;
		JsonData["Parameters"]["SectionColor"][1] = 1.0f;
		JsonData["Parameters"]["SectionColor"][2] = 1.0f;
		JsonData["Parameters"]["SectionColor"][3] = 1.0f;
	}
	else
	{
		JsonData["Parameters"]["SectionColor"][0] = MaterialInfo.DiffuseColor.X;
		JsonData["Parameters"]["SectionColor"][1] = MaterialInfo.DiffuseColor.Y;
		JsonData["Parameters"]["SectionColor"][2] = MaterialInfo.DiffuseColor.Z;
		JsonData["Parameters"]["SectionColor"][3] = 1.0f;
	}

	JsonData["Parameters"]["HasNormalMap"] = NormalTextureAssetPath.empty() ? 0.0f : 1.0f;
	if (!NormalTextureAssetPath.empty())
	{
		JsonData["Textures"]["NormalTexture"] = NormalTextureAssetPath;
	}

	FMaterialManager::Get().CreateMaterialAssetFromJson(MaterialAssetPath, JsonData);
	return MaterialAssetPath;
}

FString FFbxCommon::SanitizeName(FString Name)
{
	if (Name.empty())
	{
		return "None";
	}

	for (char& Character : Name)
	{
		const bool bAlphaNum = (Character >= 'a' && Character <= 'z')
			|| (Character >= 'A' && Character <= 'Z')
			|| (Character >= '0' && Character <= '9');
		if (!bAlphaNum && Character != '_' && Character != '-')
		{
			Character = '_';
		}
	}

	return Name;
}

FVector FFbxCommon::GetDiffuseColor(FbxSurfaceMaterial* FbxMaterial)
{
	if (FbxSurfaceLambert* Lambert = FbxCast<FbxSurfaceLambert>(FbxMaterial))
	{
		const FbxDouble3 Diffuse = Lambert->Diffuse.Get();
		const double DiffuseFactor = Lambert->DiffuseFactor.Get();
		return FVector(
			static_cast<float>(Diffuse[0] * DiffuseFactor),
			static_cast<float>(Diffuse[1] * DiffuseFactor),
			static_cast<float>(Diffuse[2] * DiffuseFactor));
	}

	return FVector(1.0f, 1.0f, 1.0f);
}

FString FFbxCommon::FindDiffuseTexturePath(const FString& FbxFilePath, FbxSurfaceMaterial* FbxMaterial)
{
	return FindTexturePathFromProperty(FbxFilePath, FbxMaterial, FbxSurfaceMaterial::sDiffuse);
}

FString FFbxCommon::FindNormalTexturePath(const FString& FbxFilePath, FbxSurfaceMaterial* FbxMaterial)
{
	FString TexturePath = FindTexturePathFromProperty(FbxFilePath, FbxMaterial, FbxSurfaceMaterial::sNormalMap);
	if (!TexturePath.empty())
	{
		return TexturePath;
	}

	return FindTexturePathFromProperty(FbxFilePath, FbxMaterial, FbxSurfaceMaterial::sBump);
}

FString FFbxCommon::ResolveFbxTexturePath(const FString& FbxFilePath, FbxFileTexture* Texture)
{
	if (!Texture)
	{
		return "";
	}

	TArray<FString> RawPaths;
	if (Texture->GetRelativeFileName() && Texture->GetRelativeFileName()[0] != '\0')
	{
		RawPaths.push_back(Texture->GetRelativeFileName());
	}
	if (Texture->GetFileName() && Texture->GetFileName()[0] != '\0')
	{
		RawPaths.push_back(Texture->GetFileName());
	}
	if (RawPaths.empty())
	{
		return "";
	}

	std::vector<std::filesystem::path> Candidates;

	const std::filesystem::path Root(FPaths::RootDir());
	const std::filesystem::path FbxDir = std::filesystem::path(FPaths::ToWide(FbxFilePath)).parent_path();
	std::vector<std::filesystem::path> SearchRoots;
	AddAncestorTextureRoots(SearchRoots, FbxDir);
	AddUniqueCandidatePath(SearchRoots, std::filesystem::path(FPaths::AssetDir()));
	AddUniqueCandidatePath(SearchRoots, std::filesystem::path(FPaths::ContentDir()));
	AddUniqueCandidatePath(SearchRoots, std::filesystem::path(FPaths::EngineSourceDir()));

	for (const FString& RawPath : RawPaths)
	{
		std::filesystem::path Raw(FPaths::ToWide(RawPath));
		AddUniqueCandidatePath(Candidates, Raw.is_absolute() ? Raw : FbxDir / Raw);
		AddUniqueCandidatePath(Candidates, Raw.is_absolute() ? Raw : Root / Raw);
		AddTrimmedRelativeCandidates(Candidates, SearchRoots, Raw);

		std::wstring SlashPath = FPaths::ToWide(RawPath);
		std::replace(SlashPath.begin(), SlashPath.end(), L'\\', L'/');
		auto AddProjectSegmentCandidate = [&](const std::wstring& Marker)
		{
			const size_t Pos = SlashPath.find(Marker);
			if (Pos != std::wstring::npos)
			{
				AddUniqueCandidatePath(Candidates, Root / std::filesystem::path(SlashPath.substr(Pos)));
			}
		};
		AddProjectSegmentCandidate(L"Asset/Content/");
		AddProjectSegmentCandidate(L"Asset/");
	}

	for (const std::filesystem::path& Candidate : Candidates)
	{
		std::error_code ErrorCode;
		if (std::filesystem::exists(Candidate, ErrorCode) && std::filesystem::is_regular_file(Candidate, ErrorCode))
		{
			return FPaths::NormalizePath(FPaths::ToUtf8(Candidate.generic_wstring()));
		}
	}

	const std::wstring FileName = std::filesystem::path(FPaths::ToWide(RawPaths.front())).filename().wstring();
	if (!FileName.empty())
	{
		if (std::filesystem::path ExactFileMatch = FindTextureByFileName(SearchRoots, FileName); !ExactFileMatch.empty())
		{
			return FPaths::NormalizePath(FPaths::ToUtf8(ExactFileMatch.generic_wstring()));
		}

		if (std::filesystem::path HeuristicMatch = FindTextureByStemHeuristic(SearchRoots, std::filesystem::path(FPaths::ToWide(RawPaths.front()))); !HeuristicMatch.empty())
		{
			return FPaths::NormalizePath(FPaths::ToUtf8(HeuristicMatch.generic_wstring()));
		}
	}

	UE_LOG_CATEGORY(ObjImporter, Warning,
		"Failed to resolve FBX texture path: fbx=%s raw=%s",
		FbxFilePath.c_str(),
		RawPaths.front().c_str());
	return "";
}

FString FFbxCommon::MakeProjectRelativePath(const std::filesystem::path& Path)
{
	const std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir()).lexically_normal();
	const std::filesystem::path NormalizedPath = Path.lexically_normal();
	std::error_code ErrorCode;
	const std::filesystem::path RelativePath = std::filesystem::relative(NormalizedPath, RootPath, ErrorCode);
	if (!ErrorCode && !RelativePath.empty())
	{
		return FPaths::NormalizePath(FPaths::ToUtf8(RelativePath.generic_wstring()));
	}
	return FPaths::NormalizePath(FPaths::ToUtf8(NormalizedPath.generic_wstring()));
}

bool FFbxCommon::ReadNormal(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonIndex, int32 CornerIndex, FbxVector4& OutNormal)
{
	if (!Mesh)
	{
		return false;
	}

	const FbxGeometryElementNormal* NormalElement = Mesh->GetElementNormal(0);
	if (NormalElement)
	{
		int32 ElementIndex = -1;
		switch (NormalElement->GetMappingMode())
		{
		case FbxLayerElement::eByControlPoint:
			ElementIndex = ControlPointIndex;
			break;
		case FbxLayerElement::eByPolygonVertex:
			ElementIndex = GetPolygonVertexLinearIndex(Mesh, PolygonIndex, CornerIndex);
			break;
		case FbxLayerElement::eByPolygon:
			ElementIndex = PolygonIndex;
			break;
		case FbxLayerElement::eAllSame:
			ElementIndex = 0;
			break;
		default:
			ElementIndex = -1;
			break;
		}

		const int32 DirectIndex = ResolveLayerElementIndex(NormalElement, ElementIndex);
		if (DirectIndex >= 0 && DirectIndex < NormalElement->GetDirectArray().GetCount())
		{
			OutNormal = NormalElement->GetDirectArray().GetAt(DirectIndex);
			return true;
		}
	}

	return Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, OutNormal);
}

FVector FFbxCommon::RemapVector(const FbxVector4& Vector)
{
	return FVector(
		static_cast<float>(-Vector[1]),
		static_cast<float>(-Vector[0]),
		static_cast<float>(Vector[2]));
}

FbxVector4 FFbxCommon::UnmapVector(const FVector& Vector)
{
	return FbxVector4(
		static_cast<double>(-Vector.Y),
		static_cast<double>(-Vector.X),
		static_cast<double>(Vector.Z),
		0.0);
}

FMatrix FFbxCommon::MakeEngineLinearMatrix(const FbxAMatrix& Matrix)
{
	const FVector EngineBasisX = RemapVector(Matrix.MultR(UnmapVector(FVector(1.0f, 0.0f, 0.0f))));
	const FVector EngineBasisY = RemapVector(Matrix.MultR(UnmapVector(FVector(0.0f, 1.0f, 0.0f))));
	const FVector EngineBasisZ = RemapVector(Matrix.MultR(UnmapVector(FVector(0.0f, 0.0f, 1.0f))));

	FMatrix Result = FMatrix::Identity;
	Result.M[0][0] = EngineBasisX.X; Result.M[0][1] = EngineBasisX.Y; Result.M[0][2] = EngineBasisX.Z; Result.M[0][3] = 0.0f;
	Result.M[1][0] = EngineBasisY.X; Result.M[1][1] = EngineBasisY.Y; Result.M[1][2] = EngineBasisY.Z; Result.M[1][3] = 0.0f;
	Result.M[2][0] = EngineBasisZ.X; Result.M[2][1] = EngineBasisZ.Y; Result.M[2][2] = EngineBasisZ.Z; Result.M[2][3] = 0.0f;
	Result.M[3][0] = 0.0f; Result.M[3][1] = 0.0f; Result.M[3][2] = 0.0f; Result.M[3][3] = 1.0f;
	return Result;
}

FVector FFbxCommon::TransformNormalByMatrix(const FbxAMatrix& Matrix, const FbxVector4& Normal)
{
	const FVector EngineNormal = RemapVector(Normal);
	const FMatrix EngineLinear = MakeEngineLinearMatrix(Matrix);

	FVector Result;
	if (std::fabs(EngineLinear.GetBasisDeterminant3x3()) > 1.0e-8f)
	{
		Result = EngineLinear.GetInverse().GetTransposed().TransformVector(EngineNormal);
	}
	else
	{
		Result = EngineLinear.TransformVector(EngineNormal);
	}

	Result = Result.Normalized();
	if (Result.IsNearlyZero() || !IsFiniteVector(Result))
	{
		return FVector(0.0f, 0.0f, 1.0f);
	}
	return Result;
}

FVector2 FFbxCommon::RemapUV(const FbxVector2& UV)
{
	return FVector2(
		static_cast<float>(UV[0]),
		1.0f - static_cast<float>(UV[1]));
}
#endif
