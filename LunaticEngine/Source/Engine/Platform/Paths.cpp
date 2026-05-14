#include "PCH/LunaticPCH.h"
#include "Engine/Platform/Paths.h"

#include <algorithm>
#include <filesystem>
#include <vector>

namespace
{
	void ReplaceAll(std::wstring& InOutText, const std::wstring& From, const std::wstring& To)
	{
		if (From.empty() || From == To)
		{
			return;
		}

		size_t SearchPos = 0;
		while ((SearchPos = InOutText.find(From, SearchPos)) != std::wstring::npos)
		{
			InOutText.replace(SearchPos, From.length(), To);
			SearchPos += To.length();
		}
	}

	std::wstring RemapLegacyAssetPathString(const std::wstring& InputPath)
	{
		std::wstring Result = InputPath;
		ReplaceAll(Result, L"\\", L"/");

		ReplaceAll(Result, L"/Asset/Game/Content/", L"/Asset/Content/");
		ReplaceAll(Result, L"/Asset/Engine/Content/", L"/Asset/Content/");
		ReplaceAll(Result, L"/Asset/Game/Source/", L"/Asset/Source/");
		ReplaceAll(Result, L"/Asset/Engine/Source/", L"/Asset/Source/");

		ReplaceAll(Result, L"Asset/Game/Content/", L"Asset/Content/");
		ReplaceAll(Result, L"Asset/Engine/Content/", L"Asset/Content/");
		ReplaceAll(Result, L"Asset/Game/Source/", L"Asset/Source/");
		ReplaceAll(Result, L"Asset/Engine/Source/", L"Asset/Source/");

		ReplaceAll(Result, L"/Asset/Content/Sound/", L"/Asset/Content/Sounds/");
		ReplaceAll(Result, L"Asset/Content/Sound/", L"Asset/Content/Sounds/");

		if (Result == L"Asset/Game/Content")
		{
			Result = L"Asset/Content";
		}
		else if (Result == L"Asset/Engine/Content")
		{
			Result = L"Asset/Content";
		}
		else if (Result == L"Asset/Game/Source")
		{
			Result = L"Asset/Source";
		}
		else if (Result == L"Asset/Engine/Source")
		{
			Result = L"Asset/Source";
		}
		else if (Result == L"Asset/Content/Sound")
		{
			Result = L"Asset/Content/Sounds";
		}

		return Result;
	}

	std::filesystem::path RemapLegacyAssetPath(const std::filesystem::path& InputPath)
	{
		if (InputPath.empty())
		{
			return {};
		}

		return std::filesystem::path(RemapLegacyAssetPathString(InputPath.generic_wstring())).lexically_normal();
	}

	bool IsProjectRootCandidate(const std::filesystem::path& Path)
	{
		return std::filesystem::exists(Path / L"Shaders")
			&& std::filesystem::exists(Path / L"Settings")
			&& std::filesystem::exists(Path / L"Asset");
	}

	std::filesystem::path FindProjectRoot(const std::filesystem::path& StartPath)
	{
		if (StartPath.empty())
		{
			return {};
		}

		std::filesystem::path Current = StartPath;
		if (!std::filesystem::is_directory(Current))
		{
			Current = Current.parent_path();
		}

		while (!Current.empty())
		{
			if (IsProjectRootCandidate(Current))
			{
				return Current;
			}

			const std::filesystem::path Parent = Current.parent_path();
			if (Parent == Current)
			{
				break;
			}

			Current = Parent;
		}

		return {};
	}
}

std::wstring FPaths::RootDir()
{
	static std::wstring Cached;
	if (Cached.empty())
	{
		WCHAR Buffer[MAX_PATH];
		GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
		const std::filesystem::path ExePath(Buffer);

		std::filesystem::path Root = FindProjectRoot(ExePath);
		if (Root.empty())
		{
			Root = FindProjectRoot(std::filesystem::current_path());
		}
		if (Root.empty())
		{
			Root = ExePath.parent_path();
		}

		Cached = Root.lexically_normal().wstring() + L"\\";
	}
	return Cached;
}

std::wstring FPaths::ShaderDir()   { return RootDir() + L"Shaders\\"; }
std::wstring FPaths::AssetDir()            { return RootDir() + L"Asset\\"; }
std::wstring FPaths::ContentDir()          { return RootDir() + L"Asset\\Content\\"; }
std::wstring FPaths::EngineDir()           { return RootDir() + L"Asset\\"; }
std::wstring FPaths::EngineContentDir()    { return EngineDir() + L"Content\\"; }
std::wstring FPaths::EngineSourceDir()     { return EngineDir() + L"Source\\"; }
std::wstring FPaths::ShaderCacheDir()      { return RootDir() + L"Shaders\\Cache\\"; }
std::wstring FPaths::BasicShapeDir()       { return EngineContentDir() + L"BasicShape\\"; }
std::wstring FPaths::EngineBasicShapeSourceDir() { return EngineSourceDir() + L"BasicShape\\"; }
std::wstring FPaths::SceneDir()            { return ContentDir() + L"Scene\\"; }
std::wstring FPaths::DataDir()             { return ContentDir() + L"Data\\"; }
std::wstring FPaths::SaveDir()     { return RootDir() + L"Saves\\"; }
std::wstring FPaths::DumpDir()     { return RootDir() + L"Saves\\Dump\\"; }
std::wstring FPaths::LogDir()      { return RootDir() + L"Saves\\Logs\\"; }
std::wstring FPaths::SettingsDir() { return RootDir() + L"Settings\\"; }
// 스크립트는 프로젝트 루트 아래 한 곳에 고정해 둬야
// 에디터 생성 파일, 런타임 로드, 디렉터리 감시가 모두 같은 위치를 바라본다.
std::wstring FPaths::ScriptsDir()  { return RootDir() + L"Scripts\\"; }

std::wstring FPaths::SettingsFilePath() { return RootDir() + L"Settings\\Editor.ini"; }
std::wstring FPaths::ResourceFilePath() { return BuiltinAssetCatalogFilePath(); }
std::wstring FPaths::ResourceSettingsDir() { return AssetSettingsDir(); }
std::wstring FPaths::AssetSettingsDir() { return RootDir() + L"Settings\\"; }
std::wstring FPaths::ImportSettingsDir() { return RootDir() + L"Settings\\"; }
std::wstring FPaths::AssetSettingsFilePath() { return AssetSettingsDir() + L"AssetSettings.ini"; }
std::wstring FPaths::AssetCatalogFilePath() { return GameAssetCatalogFilePath(); }
std::wstring FPaths::BuiltinAssetCatalogFilePath() { return AssetSettingsDir() + L"BuiltinAssetCatalog.ini"; }
std::wstring FPaths::EditorAssetCatalogFilePath() { return BuiltinAssetCatalogFilePath(); }
std::wstring FPaths::GameAssetCatalogFilePath() { return AssetSettingsDir() + L"GameAssetCatalog.ini"; }
std::wstring FPaths::ImportAssetSourcesFilePath() { return ImportSettingsDir() + L"ImportAssetSources.ini"; }
std::wstring FPaths::ImportSettingsFilePath() { return ImportAssetSourcesFilePath(); }
std::wstring FPaths::EditorResourceFilePath() { return EditorAssetCatalogFilePath(); }
std::wstring FPaths::DefaultContentResourceFilePath() { return BuiltinAssetCatalogFilePath(); }
std::wstring FPaths::ProjectResourcePathsFilePath() { return ImportSettingsFilePath(); }
std::wstring FPaths::ProjectResourceRegistryFilePath() { return AssetCatalogFilePath(); }
std::wstring FPaths::ProjectSettingsFilePath() { return RootDir() + L"Settings\\ProjectSettings.ini"; }

std::wstring FPaths::ProjectDir() { return RootDir(); }
std::wstring FPaths::ProjectContentDir() { return ContentDir(); }
std::wstring FPaths::ProjectConfigDir() { return SettingsDir(); }
std::wstring FPaths::ProjectSavedDir() { return SaveDir(); }

std::string FPaths::ConvertRelativePathToFull(const std::string& RelativePath)
{
	return ToUtf8(ResolvePathToDisk(RelativePath));
}

std::string FPaths::NormalizePath(const std::string& Path)
{
	if (Path.empty())
	{
		return {};
	}

	std::filesystem::path FsPath(ToWide(Path));
	FsPath = RemapLegacyAssetPath(FsPath.lexically_normal());
	return ToUtf8(FsPath.generic_wstring());
}

std::wstring FPaths::ResolvePathToDisk(const std::string& Path)
{
	if (Path.empty())
	{
		return {};
	}

	std::vector<std::filesystem::path> Candidates;
	auto AddCandidate = [&Candidates](const std::filesystem::path& CandidatePath)
	{
		if (CandidatePath.empty())
		{
			return;
		}

		const std::filesystem::path NormalizedCandidate = CandidatePath.lexically_normal();
		if (std::find(Candidates.begin(), Candidates.end(), NormalizedCandidate) == Candidates.end())
		{
			Candidates.push_back(NormalizedCandidate);
		}
	};

	const std::filesystem::path OriginalPath(ToWide(Path));
	const std::filesystem::path RemappedPath(ToWide(NormalizePath(Path)));
	const std::filesystem::path ProjectRoot(RootDir());

	if (RemappedPath.is_absolute())
	{
		AddCandidate(RemappedPath);
	}
	else
	{
		AddCandidate(ProjectRoot / RemappedPath);
	}

	if (OriginalPath.is_absolute())
	{
		AddCandidate(OriginalPath);
	}
	else
	{
		AddCandidate(ProjectRoot / OriginalPath);
	}

	for (const std::filesystem::path& Candidate : Candidates)
	{
		if (std::filesystem::exists(Candidate))
		{
			return Candidate.wstring();
		}
	}

	if (!Candidates.empty())
	{
		return Candidates.front().wstring();
	}

	return OriginalPath.is_absolute()
		? OriginalPath.lexically_normal().wstring()
		: (ProjectRoot / OriginalPath).lexically_normal().wstring();
}

std::wstring FPaths::Combine(const std::wstring& Base, const std::wstring& Child)
{
	std::filesystem::path Result(Base);
	Result /= Child;
	return Result.wstring();
}

void FPaths::CreateDir(const std::wstring& Path)
{
	std::filesystem::create_directories(Path);
}

std::wstring FPaths::ToWide(const std::string& Utf8Str)
{
	if (Utf8Str.empty()) return {};
	int32_t Size = MultiByteToWideChar(CP_UTF8, 0, Utf8Str.c_str(), -1, nullptr, 0);
	std::wstring Result(Size - 1, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, Utf8Str.c_str(), -1, &Result[0], Size);
	return Result;
}

std::string FPaths::ToUtf8(const std::wstring& WideStr)
{
	if (WideStr.empty()) return {};
	int32_t Size = WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string Result(Size - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, WideStr.c_str(), -1, &Result[0], Size, nullptr, nullptr);
	return Result;
}

std::string FPaths::ResolveAssetPath(const std::string& BaseFilePath, const std::string& TargetPath)
{
	if (TargetPath.empty())
	{
		return {};
	}

	const std::filesystem::path ProjectRoot(RootDir());
	std::filesystem::path BasePath(ToWide(BaseFilePath));
	if (!BasePath.is_absolute())
	{
		BasePath = (ProjectRoot / BasePath).lexically_normal();
	}

	const std::filesystem::path BaseDir = std::filesystem::is_directory(BasePath) ? BasePath : BasePath.parent_path();
	const std::filesystem::path Target(ToWide(TargetPath));
	const std::filesystem::path FullPath = (Target.is_absolute() ? Target : (BaseDir / Target)).lexically_normal();

	std::error_code ErrorCode;
	const std::filesystem::path Relative = std::filesystem::relative(FullPath, ProjectRoot, ErrorCode);
	if (!ErrorCode && !Relative.empty() && Relative.native().find(L"..") != 0)
	{
		return NormalizePath(ToUtf8(Relative.generic_wstring()));
	}

	// Project 밖의 import source는 상대경로로 망가뜨리지 말고 절대경로를 유지한다.
	return NormalizePath(ToUtf8(FullPath.generic_wstring()));
}
