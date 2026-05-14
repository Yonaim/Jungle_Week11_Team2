#pragma once

#include <string>
#include <Windows.h>

// 엔진 전역 경로를 관리합니다.
// 모든 경로는 실행 파일 기준 상대 경로이며, 한글 경로를 위해 wstring 기반입니다.
class FPaths
{
public:
	// 프로젝트 루트 (실행 파일이 있는 디렉터리)
	static std::wstring RootDir();

	// 주요 디렉터리
	static std::wstring ShaderDir();      // Shaders/
	static std::wstring AssetDir();             // Asset/
	static std::wstring ContentDir();           // Asset/Content/
	static std::wstring EngineDir();            // Legacy alias -> Asset/
	static std::wstring EngineContentDir();     // Asset/Content/
	static std::wstring EngineSourceDir();      // Asset/Source/
	static std::wstring ShaderCacheDir();       // Shaders/Cache/
	static std::wstring BasicShapeDir();        // Asset/Content/BasicShape/
	static std::wstring EngineBasicShapeSourceDir(); // Asset/Source/BasicShape/
	static std::wstring SceneDir();             // Asset/Content/Scene/
	static std::wstring DataDir();              // Asset/Content/Data/
	static std::wstring SaveDir();        // Saves/
	static std::wstring DumpDir();        // Saves/Dump/
	static std::wstring LogDir();         // Saves/Logs/
	static std::wstring SettingsDir();    // Settings/
	static std::wstring ScriptsDir();     // Scripts/

	// 주요 파일 경로
	static std::wstring SettingsFilePath();         // Settings/Editor.ini
	static std::wstring ResourceFilePath();              // Legacy alias -> BuiltinAssetCatalog
	static std::wstring ResourceSettingsDir();           // Legacy alias -> Settings/
	static std::wstring AssetSettingsDir();              // Settings/
	static std::wstring ImportSettingsDir();             // Settings/
	static std::wstring AssetSettingsFilePath();         // Settings/AssetSettings.ini
	static std::wstring AssetCatalogFilePath();          // Legacy alias -> GameAssetCatalog.ini
	static std::wstring BuiltinAssetCatalogFilePath();   // Settings/BuiltinAssetCatalog.ini
	static std::wstring EditorAssetCatalogFilePath();    // Legacy alias -> BuiltinAssetCatalog.ini
	static std::wstring GameAssetCatalogFilePath();      // Settings/GameAssetCatalog.ini
	static std::wstring ImportAssetSourcesFilePath();    // Settings/ImportAssetSources.ini
	static std::wstring ImportSettingsFilePath();        // Legacy alias -> ImportAssetSources.ini
	static std::wstring EditorResourceFilePath();        // Legacy alias -> EditorAssetCatalog.ini
	static std::wstring DefaultContentResourceFilePath();// Legacy alias -> BuiltinAssetCatalog.ini
	static std::wstring ProjectResourcePathsFilePath();  // Legacy alias -> ImportAssetSources.ini
	static std::wstring ProjectResourceRegistryFilePath();// Legacy alias -> GameAssetCatalog.ini
	static std::wstring ProjectSettingsFilePath();       // Settings/ProjectSettings.ini

	static std::wstring ProjectDir();
	static std::wstring ProjectContentDir();
	static std::wstring ProjectConfigDir();
	static std::wstring ProjectSavedDir();

	// Path Utilities
	static std::string ConvertRelativePathToFull(const std::string& RelativePath);
	static std::string NormalizePath(const std::string& Path);
	static std::wstring ResolvePathToDisk(const std::string& Path);

	//  FPaths::Combine(L"Asset/Content/Scene", L"Default.umap")
	static std::wstring Combine(const std::wstring& Base, const std::wstring& Child);


	static void CreateDir(const std::wstring& Path);


	static std::wstring ToWide(const std::string& Utf8Str);
	static std::string ToUtf8(const std::wstring& WideStr);

	static std::string ResolveAssetPath(const std::string& BaseFilePath, const std::string& TargetPath);
};
