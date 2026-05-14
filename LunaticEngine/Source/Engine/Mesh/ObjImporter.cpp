#include "PCH/LunaticPCH.h"
#include "Mesh/ObjImporter.h"
#include "Mesh/StaticMeshCommon.h"
#include "Materials/Material.h"
#include "Core/Log.h"
#include "Engine/Platform/Paths.h"
#include "Mesh/MeshAssetManager.h"
#include "Engine/Core/SimpleJsonWrapper.h"
#include "Materials/MaterialManager.h"
#include "Texture/Texture2D.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <charconv>
#include <chrono>

const FVector FallbackColor3 = FVector(1.0f, 0.0f, 1.0f);
const FVector4 FallbackColor4 = FVector4(1.0f, 0.0f, 1.0f, 1.0f);

namespace
{
	FString SanitizeAssetName(FString Name)
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

	FString MakeProjectRelativePath(const std::filesystem::path& Path)
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

	FString MakeImportedMaterialAssetPath(const FString& SourceFilePath, const FString& MaterialSlotName)
	{
		const FString SourceStem = SanitizeAssetName(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(SourceFilePath)).stem().wstring()));
		FString SlotName = SanitizeAssetName(MaterialSlotName.empty() ? FString("None") : MaterialSlotName);
		const bool bHasMaterialPrefix =
			SlotName.size() > 2 &&
			SlotName[0] == 'M' &&
			SlotName[1] == '_';

		std::filesystem::path MaterialDirectory = std::filesystem::path(FPaths::ContentDir()) / L"Materials";
		const std::wstring FileName = bHasMaterialPrefix
			? FPaths::ToWide(SlotName) + L".uasset"
			: L"M_" + FPaths::ToWide(SlotName) + L".uasset";
		std::filesystem::path MaterialAssetPath = MaterialDirectory / FileName;
		return MakeProjectRelativePath(MaterialAssetPath);
	}

}

struct FVertexKey {
    uint32 p, t, n;
    bool operator==(const FVertexKey& Other) const {
        return p == Other.p && t == Other.t && n == Other.n;
    }
};

namespace std {
template<>
struct hash<FVertexKey>
{
    size_t operator()(const FVertexKey& Key) const noexcept
    {
        return ((size_t)Key.p) ^ (((size_t)Key.t) << 8) ^ (((size_t)Key.n) << 16);
    }
};
}

struct FStringParser
{
	// Delimiter로 구분된 다음 토큰을 추출하고, InOutView에서 해당 토큰과 구분자 제거
	static std::string_view GetNextToken(std::string_view& InOutView, char Delimiter = ' ')
	{
		size_t DelimiterPosition = InOutView.find(Delimiter);
		std::string_view Token = InOutView.substr(0, DelimiterPosition); // [0, DelimiterPosition) 범위의 토큰 추출
		if (DelimiterPosition != std::string_view::npos)
		{
			InOutView.remove_prefix(DelimiterPosition + 1); // 토큰과 구분자 제거
		}
		else
		{
			InOutView = std::string_view();
		}
		return Token;
	}

	// 다수의 공백을 구분자로 사용하여 다음 토큰을 추출하고, InOutView에서 해당 토큰과 앞의 공백 제거
	static std::string_view GetNextWhitespaceToken(std::string_view& InOutView)
	{
		size_t Start = InOutView.find_first_not_of(" \t");
		if (Start == std::string_view::npos)
		{
			InOutView = std::string_view();
			return std::string_view();
		}
		InOutView.remove_prefix(Start); // 유효한 문자 앞의 공백 제거

		size_t End = InOutView.find_first_of(" \t");
		std::string_view Token = InOutView.substr(0, End); // 공백 이전까지의 토큰 추출

		if (End != std::string_view::npos)
		{
			InOutView.remove_prefix(End);
		}
		else
		{
			InOutView = std::string_view();
		}
		return Token;
	}

	// InOutView의 왼쪽 끝에 있는 공백 제거
	static void TrimLeft(std::string_view& InOutView)
	{
		size_t Start = InOutView.find_first_not_of(" \t");
		if (Start != std::string_view::npos)
		{
			InOutView.remove_prefix(Start);  // 유효한 문자 앞의 공백 제거
		}
		else
		{
			InOutView = std::string_view();
		}
	}

	static bool ParseInt(std::string_view Str, int& OutValue)
	{
		if (Str.empty()) return false;
		std::from_chars_result result = std::from_chars(Str.data(), Str.data() + Str.size(), OutValue);
		return result.ec == std::errc();
	}

	static bool ParseFloat(std::string_view Str, float& OutValue)
	{
		if (Str.empty()) return false;
		std::from_chars_result result = std::from_chars(Str.data(), Str.data() + Str.size(), OutValue);
		return result.ec == std::errc();
	}
};

namespace
{
	FString ParseMtlTextureMapPath(std::string_view Line, const FString& MtlFilePath)
	{
		std::string TextureFileName;

		while (!Line.empty())
		{
			std::string_view LineBeforeToken = Line;
			std::string_view Token = FStringParser::GetNextWhitespaceToken(Line);
			if (Token.empty())
			{
				break;
			}

			if (Token[0] == '-')
			{
				int32 ArgsToSkip = 0;
				if (Token == "-s" || Token == "-o" || Token == "-t")
				{
					ArgsToSkip = 3;
				}
				else if (Token == "-mm")
				{
					ArgsToSkip = 2;
				}
				else if (Token == "-bm" || Token == "-boost" || Token == "-texres" ||
						 Token == "-blendu" || Token == "-blendv" || Token == "-clamp" ||
						 Token == "-cc" || Token == "-imfchan" || Token == "-type")
				{
					ArgsToSkip = 1;
				}

				for (int32 i = 0; i < ArgsToSkip; ++i)
				{
					FStringParser::GetNextWhitespaceToken(Line);
				}
			}
			else
			{
				FStringParser::TrimLeft(LineBeforeToken);
				TextureFileName = FString(LineBeforeToken);
				break;
			}
		}

		size_t LastNonSpace = TextureFileName.find_last_not_of(" \t");
		if (LastNonSpace != FString::npos)
		{
			TextureFileName.erase(LastNonSpace + 1);
		}

		return TextureFileName.empty() ? FString() : FPaths::ResolveAssetPath(MtlFilePath, TextureFileName);
	}
}

struct FRawFaceVertex
{
    int32 PosIndex = -1;
    int32 UVIndex = -1;
    int32 NormalIndex = -1;
};

FRawFaceVertex ParseSingleFaceVertex(std::string_view FaceToken)
{
    FRawFaceVertex Result;

    // 첫 번째 토큰: Position
    std::string_view PosStr = FStringParser::GetNextToken(FaceToken, '/');
    FStringParser::ParseInt(PosStr, Result.PosIndex);

    // 두 번째 토큰: UV (있을 수도, 비어있을 수도 있음)
    if (!FaceToken.empty())
    {
        std::string_view UVStr = FStringParser::GetNextToken(FaceToken, '/');
        if (!UVStr.empty())
        {
            FStringParser::ParseInt(UVStr, Result.UVIndex);
        }
    }

    // 세 번째 토큰: Normal
    if (!FaceToken.empty())
    {
        std::string_view NormalStr = FStringParser::GetNextToken(FaceToken, '/');
        FStringParser::ParseInt(NormalStr, Result.NormalIndex);
    }

    return Result;
}

bool FObjImporter::ParseObj(const FString& ObjFilePath, FObjInfo& OutObjInfo)
{
	OutObjInfo = FObjInfo();
	OutObjInfo.SourceFilePath = ObjFilePath;

	std::ifstream File(FPaths::ToWide(ObjFilePath), std::ios::binary | std::ios::ate);
	if (!File.is_open())
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to open OBJ file: %s", ObjFilePath.c_str());
		return false;
	}

	size_t FileSize = static_cast<size_t>(File.tellg());
	File.seekg(0, std::ios::beg);
	TArray<char> Buffer(FileSize);
	if (!File.read(Buffer.data(), FileSize))
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to read OBJ file: %s", ObjFilePath.c_str());
		return false;
	}

	std::string_view FileView(Buffer.data(), Buffer.size());

	// UTF-8 BOM 스킵
	if (FileView.size() >= 3 && FileView[0] == '\xEF' && FileView[1] == '\xBB' && FileView[2] == '\xBF')
	{
		FileView.remove_prefix(3);
	}

	TArray<FRawFaceVertex> FaceVertices;
	FaceVertices.reserve(6); // Heuristic

	while (!FileView.empty())
	{
		std::string_view Line = FStringParser::GetNextToken(FileView, '\n');

		// CRLF 제거
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.remove_suffix(1);
		}

		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		std::string_view Prefix = FStringParser::GetNextToken(Line);

		if (Prefix == "v")
		{
			FVector Position;
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Position.Z);
			OutObjInfo.Positions.emplace_back(Position);
		}
		else if (Prefix == "vt")
		{
			FVector2 UV;
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), UV.U);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), UV.V);
			OutObjInfo.UVs.emplace_back(UV);
		}
		else if (Prefix == "vn")
		{
			FVector Normal;
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), Normal.Z);
			OutObjInfo.Normals.emplace_back(Normal);
		}
		else if (Prefix == "f")
		{
			// default material section 추가 (usemtl이 없이 f가 먼저 나오는 경우)
			if (OutObjInfo.Sections.empty())
			{
				FStaticMeshSection DefaultSection;
				DefaultSection.MaterialSlotName = "None";
				DefaultSection.FirstIndex = 0;
				DefaultSection.NumTriangles = 0;
				OutObjInfo.Sections.emplace_back(DefaultSection);
			}

			while (!Line.empty())
			{
				std::string_view FaceToken = FStringParser::GetNextToken(Line, ' ');
				if (!FaceToken.empty())
				{
					FaceVertices.push_back(ParseSingleFaceVertex(FaceToken));
				}
			}

			if (FaceVertices.size() < 3)
			{
				UE_LOG_CATEGORY(ObjImporter, Warning, "Face with less than 3 vertices");
				continue;
			}

			// Fan triangulation
			for (size_t i = 1; i + 1 < FaceVertices.size(); ++i)
			{
				const std::array<FRawFaceVertex, 3> TriangleVerts = { FaceVertices[0], FaceVertices[i], FaceVertices[i + 1] };
				for (int j = 0; j < 3; ++j)
				{
					constexpr int32 InvalidIndex = -1;
					OutObjInfo.PosIndices.emplace_back(TriangleVerts[j].PosIndex - 1);
					OutObjInfo.UVIndices.emplace_back(TriangleVerts[j].UVIndex > 0 ? TriangleVerts[j].UVIndex - 1 : InvalidIndex);
					OutObjInfo.NormalIndices.emplace_back(TriangleVerts[j].NormalIndex > 0 ? TriangleVerts[j].NormalIndex - 1 : InvalidIndex);
				}
			}
			FaceVertices.clear();
		}
		else
		{
			if (Prefix == "mtllib")
			{
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string_view::npos) { Line = Line.substr(0, CommentPos); }
				FStringParser::TrimLeft(Line);
				OutObjInfo.MaterialLibraryFilePath = FPaths::ResolveAssetPath(ObjFilePath, std::string(Line));
				UE_LOG_CATEGORY(ObjImporter, Info, "Found material library: %s", OutObjInfo.MaterialLibraryFilePath.c_str());
			}
			else if (Prefix == "usemtl")
			{
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string_view::npos) { Line = Line.substr(0, CommentPos); }
				FStringParser::TrimLeft(Line);

				if (!OutObjInfo.Sections.empty())
				{
					OutObjInfo.Sections.back().NumTriangles = (static_cast<uint32>(OutObjInfo.PosIndices.size()) - OutObjInfo.Sections.back().FirstIndex) / 3;
				}
				FStaticMeshSection Section;
				Section.MaterialSlotName = std::string(Line);
				if (Section.MaterialSlotName.empty())
				{
					Section.MaterialSlotName = "None";
				}
				Section.FirstIndex = static_cast<uint32>(OutObjInfo.PosIndices.size());
				OutObjInfo.Sections.emplace_back(Section);
			}
			else if (Prefix == "o")
			{
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string_view::npos) { Line = Line.substr(0, CommentPos); }
				FStringParser::TrimLeft(Line);

				OutObjInfo.ObjectName = std::string(Line);
			}
		}
	}

	if (!OutObjInfo.Sections.empty())
	{
		OutObjInfo.Sections.back().NumTriangles = (static_cast<uint32>(OutObjInfo.PosIndices.size()) - OutObjInfo.Sections.back().FirstIndex) / 3;
	}

	if (OutObjInfo.UVs.empty())
	{
		OutObjInfo.UVs.emplace_back(FVector2{ 0.0f, 0.0f });
	}

	return true;
}

bool FObjImporter::ParseMtl(const FString& MtlFilePath, TArray<FObjMaterialInfo>& OutMtlInfos)
{
	OutMtlInfos.clear();
	std::ifstream File(FPaths::ToWide(MtlFilePath), std::ios::binary | std::ios::ate);

	if (!File.is_open())
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to open MTL file: %s", MtlFilePath.c_str());
		return false;
	}

	size_t FileSize = static_cast<size_t>(File.tellg());
	File.seekg(0, std::ios::beg);
	TArray<char> Buffer(FileSize);
	if (!File.read(Buffer.data(), FileSize))
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to read MTL file: %s", MtlFilePath.c_str());
		return false;
	}

	std::string_view FileView(Buffer.data(), Buffer.size());

	// UTF-8 BOM 스킵
	if (FileView.size() >= 3 && FileView[0] == '\xEF' && FileView[1] == '\xBB' && FileView[2] == '\xBF')
	{
		FileView.remove_prefix(3);
	}

	while (!FileView.empty())
	{
		std::string_view Line = FStringParser::GetNextToken(FileView, '\n');

		// CRLF 제거
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.remove_suffix(1);
		}

		if (Line.empty() || Line[0] == '#')
		{
			continue;
		}

		std::string_view Prefix = FStringParser::GetNextWhitespaceToken(Line);

		if (Prefix == "newmtl")
		{
			FObjMaterialInfo MaterialInfo;
			FStringParser::TrimLeft(Line);
			MaterialInfo.MaterialSlotName = std::string(Line);
			MaterialInfo.Kd = FallbackColor3;
			OutMtlInfos.emplace_back(MaterialInfo);
		}
		else if (Prefix == "Kd")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Kd.Z);
		}
		else if (Prefix == "Ks")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			FObjMaterialInfo& CurrentMaterial = OutMtlInfos.back();
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.X);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.Y);
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), CurrentMaterial.Ks.Z);
		}
		else if (Prefix == "Ns")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}
			FStringParser::ParseFloat(FStringParser::GetNextWhitespaceToken(Line), OutMtlInfos.back().Ns);
		}
		else if (Prefix == "map_Kd")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}

			std::string TextureFileName;

			// 토큰 단위로 옵션들을 건너뜁니다.
			while (!Line.empty())
			{
				// 파일명에 공백이 포함될 수 있으므로, 현재 Line의 상태를 백업해둡니다.
				std::string_view LineBeforeToken = Line;
				std::string_view Token = FStringParser::GetNextWhitespaceToken(Line);

				if (Token.empty()) break;

				// 토큰이 '-'로 시작하면 옵션 플래그인지 확인합니다.
				if (Token[0] == '-')
				{
					int32 ArgsToSkip = 0;

					// 1. 3개의 인자를 받는 옵션 (Vector)
					if (Token == "-s" || Token == "-o" || Token == "-t")
					{
						ArgsToSkip = 3;
					}
					// 2. 2개의 인자를 받는 옵션
					else if (Token == "-mm")
					{
						ArgsToSkip = 2;
					}
					// 3. 1개의 인자를 받는 옵션 (Float, String, Bool)
					else if (Token == "-bm" || Token == "-boost" || Token == "-texres" ||
							 Token == "-blendu" || Token == "-blendv" || Token == "-clamp" ||
							 Token == "-cc" || Token == "-imfchan")
					{
						ArgsToSkip = 1;
					}

					// 파악된 옵션의 인자 개수만큼 다음 토큰들을 무시합니다.
					for (int32 i = 0; i < ArgsToSkip; ++i)
					{
						FStringParser::GetNextWhitespaceToken(Line);
					}
				}
				else
				{
					// '-'로 시작하지 않는 첫 번째 토큰을 만났다면, 이것이 파일명의 시작입니다!
					// 파일명 내부에 띄어쓰기가 있을 수 있으므로 토큰을 뽑기 전의 전체 라인을 가져옵니다.
					FStringParser::TrimLeft(LineBeforeToken);
					TextureFileName = FString(LineBeforeToken);
					break;
				}
			}

			// 문자열 끝에 남아있을지 모르는 쓸데없는 공백이나 탭을 정리합니다. (RTrim)
			size_t LastNonSpace = TextureFileName.find_last_not_of(" \t");
			if (LastNonSpace != FString::npos)
			{
				TextureFileName.erase(LastNonSpace + 1);
			}

			// 최종적으로 추출된 파일명 할당
			if (!TextureFileName.empty())
			{
				OutMtlInfos.back().map_Kd = FPaths::ResolveAssetPath(MtlFilePath, TextureFileName);
			}
		}
		else if (Prefix == "map_Ks" || Prefix == "bump" || Prefix == "map_Bump" || Prefix == "map_bump")
		{
			if (OutMtlInfos.empty())
			{
				continue;
			}

			const FString ParsedTexturePath = ParseMtlTextureMapPath(Line, MtlFilePath);
			if (ParsedTexturePath.empty())
			{
				continue;
			}

			if (Prefix == "map_Ks")
			{
				OutMtlInfos.back().map_Ks = ParsedTexturePath;
			}
			else
			{
				OutMtlInfos.back().bump = ParsedTexturePath;
			}
		}
	}

	return true;
}

// MTL 정보에서 정식 Material .uasset 파일로 변환한다.
// 생성 위치는 Asset/Content/Materials/ 아래이다.
FString FObjImporter::ConvertMtlInfoToMaterialAsset(const FString& SourceFilePath, const FObjMaterialInfo* MtlInfo)
{
	if (!MtlInfo)
	{
		return "None";
	}

	const FString MaterialAssetPath = MakeImportedMaterialAssetPath(SourceFilePath, MtlInfo->MaterialSlotName);

	const std::filesystem::path MaterialAssetPathW = std::filesystem::path(FPaths::RootDir()) / FPaths::ToWide(MaterialAssetPath);

	std::filesystem::create_directories(MaterialAssetPathW.parent_path());

	json::JSON JsonData;
	JsonData["PathFileName"] = MaterialAssetPath;
	JsonData["Origin"] = "ObjImport";
	JsonData["SourceFilePath"] = MakeProjectRelativePath(std::filesystem::path(FPaths::ToWide(SourceFilePath)));
	JsonData["ShaderPath"] = "Shaders/Geometry/UberLit.hlsl";
	JsonData["RenderPass"] = "Opaque";

	const FString DiffuseTextureAssetPath = MtlInfo->map_Kd.empty() ? FString() : UTexture2D::ImportTextureAsset(MtlInfo->map_Kd);
	const FString NormalTextureAssetPath = MtlInfo->bump.empty() ? FString() : UTexture2D::ImportTextureAsset(MtlInfo->bump);
	const FString SpecularTextureAssetPath = MtlInfo->map_Ks.empty() ? FString() : UTexture2D::ImportTextureAsset(MtlInfo->map_Ks);

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
		JsonData["Parameters"]["SectionColor"][0] = MtlInfo->Kd.X;
		JsonData["Parameters"]["SectionColor"][1] = MtlInfo->Kd.Y;
		JsonData["Parameters"]["SectionColor"][2] = MtlInfo->Kd.Z;
		JsonData["Parameters"]["SectionColor"][3] = 1.0f;
	}

	JsonData["Parameters"]["HasNormalMap"] = NormalTextureAssetPath.empty() ? 0.0f : 1.0f;

	if (!NormalTextureAssetPath.empty())
	{
		JsonData["Textures"]["NormalTexture"] = NormalTextureAssetPath;
	}

	if (!SpecularTextureAssetPath.empty())
	{
		JsonData["Textures"]["SpecularTexture"] = SpecularTextureAssetPath;
	}

	FMaterialManager::Get().CreateMaterialAssetFromJson(MaterialAssetPath, JsonData);

	return MaterialAssetPath;
}

FVector FObjImporter::RemapPosition(const FVector& ObjPos, EForwardAxis Axis)
{
	// OBJ 원본 좌표 (Ox, Oy, Oz) → 엔진 (Ex, Ey, Ez)
	// 엔진: X=Forward, Y=Right, Z=Up
	// OBJ 기본: Y-up 우수 좌표계
	switch (Axis)
	{
	case EForwardAxis::X:    // OBJ +X → Engine Forward(+X)
		return FVector(ObjPos.X, ObjPos.Z, ObjPos.Y);
	case EForwardAxis::NegX: // OBJ -X → Engine Forward(+X)
		return FVector(-ObjPos.X, -ObjPos.Z, ObjPos.Y);
	case EForwardAxis::Y:    // OBJ +Y → Engine Forward(+X)
		return FVector(ObjPos.Y, ObjPos.X, ObjPos.Z);
	case EForwardAxis::NegY: // OBJ -Y → Engine Forward(+X) — 블렌더 기본
		return FVector(-ObjPos.Y, -ObjPos.X, ObjPos.Z);
	case EForwardAxis::Z:    // OBJ +Z → Engine Forward(+X)
		return FVector(ObjPos.Z, ObjPos.X, ObjPos.Y);
	case EForwardAxis::NegZ: // OBJ -Z → Engine Forward(+X) — OBJ 기본 (Y-up, -Z forward)
		return FVector(-ObjPos.Z, ObjPos.X, ObjPos.Y);
	default:
		return FVector(ObjPos.X, ObjPos.Z, ObjPos.Y);
	}
}

bool FObjImporter::Convert(const FObjInfo& ObjInfo, const TArray<FObjMaterialInfo>& MtlInfos, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	OutMesh = FStaticMesh();
	OutMaterials.clear();

	// Phase 1: usemtl 등장 순서를 기반으로 FStaticMaterial 배열 및 인덱스 맵 생성
	TArray<FString> OrderedMaterialSlots;
	bool bHasNoneSlot = false;

	// OBJ의 Sections(usemtl) 등장 순서대로 고유 슬롯 수집
	for (const FStaticMeshSection& Section : ObjInfo.Sections)
	{
		const FString& CurrentSlotName = Section.MaterialSlotName;

		if (CurrentSlotName == "None")
		{
			bHasNoneSlot = true;
			continue;
		}

		// 기존에 수집된 슬롯과 중복되지 않는 경우에만 추가
		if (std::find(OrderedMaterialSlots.begin(), OrderedMaterialSlots.end(), CurrentSlotName) == OrderedMaterialSlots.end())
		{
			OrderedMaterialSlots.push_back(CurrentSlotName);
		}
	}

	// 수집된 순서대로 머티리얼 생성 및 인덱스 매핑
	for (const FString& TargetSlotName : OrderedMaterialSlots)
	{
		// 슬롯 이름과 일치하는 파싱된 머티리얼 데이터 선형 탐색
		const FObjMaterialInfo* MatchedMaterial = nullptr;
		auto It = std::find_if(MtlInfos.begin(), MtlInfos.end(),
			[&TargetSlotName](const FObjMaterialInfo& Mat) {
				return Mat.MaterialSlotName == TargetSlotName;
			});

		if (It != MtlInfos.end())
		{
			MatchedMaterial = &(*It);
			// 섹션 머티리얼 슬롯 이름과 일치하는 머티리얼 이름이 MTL 파일에서 발견된 경우, 해당 머티리얼 로드 또는 생성
			UE_LOG_CATEGORY(ObjImporter, Debug, "Importer TargetSlotName: %s;", TargetSlotName.c_str());

			// Convert() 안에서 기존 직접 세팅 대신
			FString MaterialPath = ConvertMtlInfoToMaterialAsset(ObjInfo.SourceFilePath, MatchedMaterial); // Material .uasset 생성

			UMaterial* MaterialObject = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);

			// FStaticMaterial 슬롯 생성 및 OutMaterials에 추가
			FStaticMaterial NewStaticMaterial;
			NewStaticMaterial.MaterialInterface = MaterialObject;
			NewStaticMaterial.MaterialSlotName = TargetSlotName;
			OutMaterials.push_back(NewStaticMaterial);
		}
		else // Material Slot이 MTL 파일에 정의되어 있지 않은 경우
		{
			UMaterial* DefaultMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");

			// FStaticMaterial 슬롯 생성 및 OutMaterials에 추가
			FStaticMaterial NewEmptyStaticMaterial;
			NewEmptyStaticMaterial.MaterialInterface = DefaultMaterial;
			NewEmptyStaticMaterial.MaterialSlotName = TargetSlotName;
			OutMaterials.push_back(NewEmptyStaticMaterial);
		}
	}

	// "None" 슬롯이 존재했다면 맨 마지막에 배치
	if (bHasNoneSlot)
	{
		UMaterial* DefaultMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");

		FStaticMaterial NewDefaultStaticMaterial;
		NewDefaultStaticMaterial.MaterialInterface = DefaultMaterial;
		NewDefaultStaticMaterial.MaterialSlotName = "None";

		OutMaterials.push_back(NewDefaultStaticMaterial);
	}

    // Phase 2: 파편화된 섹션들의 면(Face)을 머티리얼 인덱스 기준으로 재그룹화
	if (OutMaterials.empty() && !ObjInfo.PosIndices.empty())
	{
		UMaterial* DefaultMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");

		FStaticMaterial FallbackMaterial;
		FallbackMaterial.MaterialInterface = DefaultMaterial;
		FallbackMaterial.MaterialSlotName = "None";
		OutMaterials.push_back(FallbackMaterial);
	}

	TArray<TArray<uint32>> FacesPerMaterial;
	FacesPerMaterial.resize(OutMaterials.size());

	for (const FStaticMeshSection& RawSection : ObjInfo.Sections)
	{
		// 섹션의 머티리얼 슬롯 이름과 일치하는 OutMaterials 배열의 인덱스 찾기
		auto It = std::find_if(OutMaterials.begin(), OutMaterials.end(),
			[&RawSection](const FStaticMaterial& Mat) {
				return Mat.MaterialSlotName == RawSection.MaterialSlotName;
			});

		size_t MaterialIndex = 0;
		if (It != OutMaterials.end())
		{
			MaterialIndex = std::distance(OutMaterials.begin(), It);
		}
		else
		{
			// "None" 슬롯이 없고 매칭되는 슬롯도 없는 경우, 기본 머티리얼로 할당
			MaterialIndex = OutMaterials.size() - 1; // "None" 슬롯이 마지막에 배치되어 있다고 가정
			UE_LOG_CATEGORY(ObjImporter, Warning, "Material slot '%s' not found. Assigning to Default slot.", RawSection.MaterialSlotName.c_str());
		}

		for (uint32 i = 0; i < RawSection.NumTriangles; ++i)
		{
			uint32 FaceStartIndex = RawSection.FirstIndex + (i * 3);
			FacesPerMaterial[MaterialIndex].push_back(FaceStartIndex);
		}
	}

    // Phase 3: 재그룹화된 면 데이터를 기반으로 최종 정점/인덱스 버퍼 구축
	TMap<FVertexKey, uint32> VertexMap;

	for (size_t MaterialIndex = 0; MaterialIndex < OutMaterials.size(); ++MaterialIndex)
	{
		const TArray<uint32>& FaceStarts = FacesPerMaterial[MaterialIndex];
		if (FaceStarts.empty()) continue;

		FStaticMeshSection NewSection;
		NewSection.MaterialIndex = static_cast<int32>(MaterialIndex);
		NewSection.MaterialSlotName = OutMaterials[MaterialIndex].MaterialSlotName;
		NewSection.FirstIndex = static_cast<uint32>(OutMesh.Indices.size());
		NewSection.NumTriangles = static_cast<uint32>(FaceStarts.size());

		for (uint32 FaceStartIndex : FaceStarts)
		{
			uint32 TriangleIndices[3];

			// (P1 - P0) X (P2 - P0) 후 정규화
			FVector P0 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex]];
			FVector P1 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex + 1]];
			FVector P2 = ObjInfo.Positions[ObjInfo.PosIndices[FaceStartIndex + 2]];

			FVector Edge1 = P1 - P0;
			FVector Edge2 = P2 - P0;
			FVector FaceNormal = Edge1.Cross(Edge2).Normalized();

			for (int j = 0; j < 3; ++j)
			{
				size_t CurrentIndex = FaceStartIndex + j;
				FVertexKey Key = {
					ObjInfo.PosIndices[CurrentIndex],
					ObjInfo.UVIndices[CurrentIndex],
					ObjInfo.NormalIndices[CurrentIndex]
				};

				if (auto It = VertexMap.find(Key); It != VertexMap.end())
				{
					// 이미 생성된 정점이 있다면 인덱스 재사용
					TriangleIndices[j] = It->second;
				}
				else
				{
					// 새로운 정점 생성
					FNormalVertex NewVertex;

					// 축 리맵 + 스케일 적용
					NewVertex.pos = RemapPosition(ObjInfo.Positions[Key.p], Options.ForwardAxis) * Options.Scale;

					// Normal 리맵
					if (Key.n == -1)
					{
						NewVertex.normal = RemapPosition(FaceNormal, Options.ForwardAxis).Normalized();
					}
					else
					{
						NewVertex.normal = RemapPosition(ObjInfo.Normals[Key.n], Options.ForwardAxis).Normalized();
					}

					// UV 예외 처리
					if (Key.t == -1)
					{
						NewVertex.tex = { 0.0f, 0.0f };
					}
					else
					{
						NewVertex.tex = ObjInfo.UVs[Key.t];
						// UV 변환 (left-bottom -> left-top)
						NewVertex.tex.V = 1.0f - NewVertex.tex.V;
					}

					NewVertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

					uint32 NewIndex = static_cast<uint32>(OutMesh.Vertices.size());
					OutMesh.Vertices.push_back(NewVertex);

					VertexMap[Key] = NewIndex;
					TriangleIndices[j] = NewIndex;
				}
			}

			// 와인딩 오더 처리
			OutMesh.Indices.push_back(TriangleIndices[0]);
			if (Options.WindingOrder == EWindingOrder::CCW_to_CW)
			{
				OutMesh.Indices.push_back(TriangleIndices[2]);
				OutMesh.Indices.push_back(TriangleIndices[1]);
			}
			else
			{
				OutMesh.Indices.push_back(TriangleIndices[1]);
				OutMesh.Indices.push_back(TriangleIndices[2]);
			}
		}

		OutMesh.Sections.push_back(NewSection);
	}

	//일반적인 Tangent계산
	TArray<FVector> TangentSums(OutMesh.Vertices.size(), FVector(0.0f, 0.0f, 0.0f));
	TArray<FVector> BitangentSums(OutMesh.Vertices.size(), FVector(0.0f, 0.0f, 0.0f));

	for (size_t i = 0; i + 2 < OutMesh.Indices.size(); i += 3)
	{
		uint32 I0 = OutMesh.Indices[i + 0];
		uint32 I1 = OutMesh.Indices[i + 1];
		uint32 I2 = OutMesh.Indices[i + 2];

		const FNormalVertex& V0 = OutMesh.Vertices[I0];
		const FNormalVertex& V1 = OutMesh.Vertices[I1];
		const FNormalVertex& V2 = OutMesh.Vertices[I2];

		FVector Edge1 = V1.pos - V0.pos;
		FVector Edge2 = V2.pos - V0.pos;

		FVector2 DeltaUV1 = V1.tex - V0.tex;
		FVector2 DeltaUV2 = V2.tex - V0.tex;

		float Det = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;
		if (std::abs(Det) < 1e-8f)
		{
			continue;
		}

		float InvDet = 1.0f / Det;

		FVector Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDet;
		FVector Bitangent = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * InvDet;

		TangentSums[I0] += Tangent;
		TangentSums[I1] += Tangent;
		TangentSums[I2] += Tangent;

		BitangentSums[I0] += Bitangent;
		BitangentSums[I1] += Bitangent;
		BitangentSums[I2] += Bitangent;
	}

	for (size_t i = 0; i < OutMesh.Vertices.size(); ++i)
	{
		FNormalVertex& V = OutMesh.Vertices[i];

		FVector N = V.normal.Normalized();
		FVector T = TangentSums[i];

		// Gram-Schmidt: tangent를 normal에 직교하게 보정
		T = T - N * N.Dot(T);

		if (T.Length() < 1e-8f)
		{
			// UV가 없거나 degenerate인 경우 fallback tangent 생성
			FVector Axis = std::abs(N.Z) < 0.999f
				? FVector(0.0f, 0.0f, 1.0f)
				: FVector(0.0f, 1.0f, 0.0f);

			T = Axis.Cross(N).Normalized();
		}
		else
		{
			T.Normalize();
		}

		FVector B = BitangentSums[i];
		float Handedness = N.Cross(T).Dot(B) < 0.0f ? -1.0f : 1.0f;

		V.tangent = FVector4(T, Handedness);
	}

    return true;
}

bool FObjImporter::Import(const FString& ObjFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	return Import(ObjFilePath, FImportOptions::Default(), OutMesh, OutMaterials);
}

bool FObjImporter::Import(const FString& ObjFilePath, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	auto StartTime = std::chrono::high_resolution_clock::now();

	OutMaterials.clear();

	FObjInfo ObjInfo;
	if (!FObjImporter::ParseObj(ObjFilePath, ObjInfo))
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "ParseObj failed for: %s", ObjFilePath.c_str());
		return false;
	}

	TArray<FObjMaterialInfo> ParsedMtlInfos;
	if (!ObjInfo.MaterialLibraryFilePath.empty()) {
		if (!FObjImporter::ParseMtl(ObjInfo.MaterialLibraryFilePath, ParsedMtlInfos))
		{
			UE_LOG_CATEGORY(ObjImporter, Warning, "ParseMtl failed for: %s", ObjInfo.MaterialLibraryFilePath.c_str());
			ObjInfo.MaterialLibraryFilePath.clear();
			ParsedMtlInfos.clear();
		}
	}

	if (!FObjImporter::Convert(ObjInfo, ParsedMtlInfos, Options, OutMesh, OutMaterials)){
		UE_LOG_CATEGORY(ObjImporter, Error, "Convert failed for: %s", ObjFilePath.c_str());
		return false;
	}
	OutMesh.PathFileName = ObjFilePath;

	auto EndTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> Duration = EndTime - StartTime;
	UE_LOG_CATEGORY(ObjImporter, Info, "OBJ Imported successfully. File: %s. Time taken: %.4f seconds", ObjFilePath.c_str(), Duration.count());

	return true;
}

bool FObjImporter::ImportMtl(const FString& MtlFilePath, TArray<FString>* OutGeneratedMaterialAssetPaths)
{
	if (OutGeneratedMaterialAssetPaths)
	{
		OutGeneratedMaterialAssetPaths->clear();
	}

	TArray<FObjMaterialInfo> ParsedMtlInfos;
	if (!ParseMtl(MtlFilePath, ParsedMtlInfos))
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "ParseMtl failed for: %s", MtlFilePath.c_str());
		return false;
	}

	if (ParsedMtlInfos.empty())
	{
		UE_LOG_CATEGORY(ObjImporter, Warning, "No materials found in MTL file: %s", MtlFilePath.c_str());
		return false;
	}

	for (const FObjMaterialInfo& MaterialInfo : ParsedMtlInfos)
	{
		if (MaterialInfo.MaterialSlotName.empty())
		{
			continue;
		}

		const FString MaterialAssetPath = ConvertMtlInfoToMaterialAsset(MtlFilePath, &MaterialInfo);
		if (!MaterialAssetPath.empty())
		{
			if (OutGeneratedMaterialAssetPaths)
			{
				OutGeneratedMaterialAssetPaths->push_back(MaterialAssetPath);
			}

			// Force-load so the new .uasset is immediately available in editor/runtime caches.
			FMaterialManager::Get().GetOrCreateMaterial(MaterialAssetPath);
		}
	}

	UE_LOG_CATEGORY(ObjImporter, Info, "MTL Imported successfully. File: %s. Generated %zu material asset(s)", MtlFilePath.c_str(), OutGeneratedMaterialAssetPaths ? OutGeneratedMaterialAssetPaths->size() : ParsedMtlInfos.size());
	return true;
}
