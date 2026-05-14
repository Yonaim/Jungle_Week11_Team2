#include "PCH/LunaticPCH.h"
#include "Texture/Texture2D.h"
#include "Object/ObjectFactory.h"
#include "Core/AsciiUtils.h"
#include "Core/Log.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Platform/DirectoryWatcher.h"
#include "Platform/Paths.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cwctype>
#include <d3d11.h>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <wincodec.h>

IMPLEMENT_CLASS(UTexture2D, UObject)

std::map<FString, UTexture2D*> UTexture2D::TextureCache;
TArray<FTextureAssetListItem> UTexture2D::AvailableTextureFiles;
bool UTexture2D::bTextureAssetListDirty = true;
TSet<FString> UTexture2D::PendingTextureRefreshPaths;
FWatchID UTexture2D::TextureAssetWatchID = 0;
FSubscriptionID UTexture2D::TextureAssetWatchSub = 0;
bool UTexture2D::bTextureAssetWatcherInitialized = false;

namespace
{
	FString NormalizeTexturePath(const FString& FilePath)
	{
		return FPaths::NormalizePath(FilePath);
	}

	std::wstring ResolveTexturePathOnDisk(const FString& FilePath)
	{
		return FPaths::ResolvePathToDisk(FilePath);
	}

	std::filesystem::path TryGetProjectRootFromAssetPath(const FString& PathText)
	{
		std::wstring SlashPath = FPaths::ToWide(PathText);
		std::replace(SlashPath.begin(), SlashPath.end(), L'\\', L'/');

		auto TryMarker = [&](const std::wstring& Marker) -> std::filesystem::path
		{
			const size_t Pos = SlashPath.find(Marker);
			if (Pos == std::wstring::npos || Pos == 0)
			{
				return {};
			}

			std::filesystem::path CandidateRoot(SlashPath.substr(0, Pos));
			std::error_code ErrorCode;
			if (std::filesystem::exists(CandidateRoot / L"Asset" / L"Content", ErrorCode))
			{
				return CandidateRoot.lexically_normal();
			}

			// 존재 여부와 무관하게, 절대 경로 문자열 안에 Asset/Content가 있으면
			// 그 앞부분은 "그 파일이 원래 속했던 프로젝트 루트"로 볼 수 있다.
			if (CandidateRoot.is_absolute())
			{
				return CandidateRoot.lexically_normal();
			}

			return {};
		};

		if (std::filesystem::path Root = TryMarker(L"/Asset/Content/"); !Root.empty())
		{
			return Root;
		}
		if (std::filesystem::path Root = TryMarker(L"Asset/Content/"); !Root.empty())
		{
			return Root;
		}
		if (std::filesystem::path Root = TryMarker(L"/Asset/"); !Root.empty())
		{
			return Root;
		}
		return {};
	}

	FString ToLowerAsciiCopy(FString Value)
	{
		AsciiUtils::ToLowerInPlace(Value);
		return Value;
	}

	void AddUniquePath(std::vector<std::filesystem::path>& Paths, const std::filesystem::path& Path)
	{
		if (Path.empty())
		{
			return;
		}

		const std::filesystem::path Normalized = Path.lexically_normal();
		if (std::find(Paths.begin(), Paths.end(), Normalized) == Paths.end())
		{
			Paths.push_back(Normalized);
		}
	}

	void AddContentSearchRoots(std::vector<std::filesystem::path>& Roots, const FString& SourcePath)
	{
		AddUniquePath(Roots, std::filesystem::path(FPaths::AssetDir()));
		AddUniquePath(Roots, std::filesystem::path(FPaths::EngineSourceDir()));
		AddUniquePath(Roots, std::filesystem::path(FPaths::ContentDir()));
		AddUniquePath(Roots, std::filesystem::current_path() / L"Asset" / L"Content");
		AddUniquePath(Roots, std::filesystem::current_path() / L"Asset" / L"Source");

		if (std::filesystem::path RootFromPath = TryGetProjectRootFromAssetPath(SourcePath); !RootFromPath.empty())
		{
			AddUniquePath(Roots, RootFromPath / L"Asset");
			AddUniquePath(Roots, RootFromPath / L"Asset" / L"Source");
			AddUniquePath(Roots, RootFromPath / L"Asset" / L"Content");
		}
	}

	bool IsRelativeSubpath(const std::filesystem::path& RelativePath)
	{
		return !RelativePath.empty()
			&& RelativePath.native().find(L"..") != 0
			&& !RelativePath.is_absolute();
	}

	std::filesystem::path FindByFileNameUnderContentRoots(const FString& SourcePath)
	{
		const std::filesystem::path Original(FPaths::ToWide(SourcePath));
		const std::wstring FileName = Original.filename().wstring();
		if (FileName.empty())
		{
			return {};
		}

		std::vector<std::filesystem::path> SearchRoots;
		AddContentSearchRoots(SearchRoots, SourcePath);

		for (const std::filesystem::path& ContentRoot : SearchRoots)
		{
			std::error_code ErrorCode;
			if (!std::filesystem::exists(ContentRoot, ErrorCode))
			{
				continue;
			}

			for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(ContentRoot, std::filesystem::directory_options::skip_permission_denied, ErrorCode))
			{
				if (ErrorCode)
				{
					break;
				}
				if (Entry.is_regular_file()
					&& ToLowerAsciiCopy(FPaths::ToUtf8(Entry.path().filename().wstring()))
						== ToLowerAsciiCopy(FPaths::ToUtf8(FileName)))
				{
					return Entry.path().lexically_normal();
				}
			}
		}

		return {};
	}

	std::filesystem::path FindTextureSourceOnDisk(const FString& SourcePath)
	{
		std::vector<std::filesystem::path> Candidates;

		const std::filesystem::path Root(FPaths::RootDir());
		const std::filesystem::path Original(FPaths::ToWide(SourcePath));
		const std::filesystem::path Resolved(FPaths::ResolvePathToDisk(SourcePath));

		AddUniquePath(Candidates, Resolved);
		AddUniquePath(Candidates, Original.is_absolute() ? Original : Root / Original);

		// 경로 문자열 안에 Asset/Content/...가 들어있으면, 그 뒤쪽 상대 경로만 떼서
		// 현재 프로젝트 루트와 원본 프로젝트 루트 양쪽에 붙여본다.
		std::wstring SlashPath = FPaths::ToWide(SourcePath);
		std::replace(SlashPath.begin(), SlashPath.end(), L'\\', L'/');
		auto AddAssetSegmentCandidates = [&](const std::wstring& Marker)
		{
			const size_t Pos = SlashPath.find(Marker);
			if (Pos == std::wstring::npos)
			{
				return;
			}

			const std::filesystem::path AssetRelative(SlashPath.substr(Pos));
			AddUniquePath(Candidates, Root / AssetRelative);
			if (std::filesystem::path RootFromPath = TryGetProjectRootFromAssetPath(SourcePath); !RootFromPath.empty())
			{
				AddUniquePath(Candidates, RootFromPath / AssetRelative);
			}
		};
		AddAssetSegmentCandidates(L"Asset/Content/");
		AddAssetSegmentCandidates(L"Asset/");

		for (const std::filesystem::path& Candidate : Candidates)
		{
			std::error_code ErrorCode;
			if (std::filesystem::exists(Candidate, ErrorCode) && std::filesystem::is_regular_file(Candidate, ErrorCode))
			{
				return Candidate;
			}
		}

		if (std::filesystem::path FoundByName = FindByFileNameUnderContentRoots(SourcePath); !FoundByName.empty())
		{
			return FoundByName;
		}

		return Candidates.empty() ? std::filesystem::path() : Candidates.front();
	}

	FString SanitizeAssetName(FString Name)
	{
		if (Name.empty())
		{
			return "Imported";
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

	std::filesystem::path MakeImportedTextureDirectory(const std::filesystem::path& SourceDiskPath)
	{
		const std::filesystem::path ContentRoot = std::filesystem::path(FPaths::ContentDir()).lexically_normal();
		const std::filesystem::path SourceRoot = std::filesystem::path(FPaths::EngineSourceDir()).lexically_normal();
		const std::filesystem::path RelativeToSource = SourceDiskPath.lexically_normal().lexically_relative(SourceRoot);

		std::filesystem::path TextureDirectory = ContentRoot / L"Textures";
		if (IsRelativeSubpath(RelativeToSource))
		{
			TextureDirectory /= RelativeToSource.parent_path();
		}

		return TextureDirectory.lexically_normal();
	}

	bool TryGetTextureWriteTime(const FString& FilePath, std::filesystem::file_time_type& OutWriteTime)
	{
		std::error_code ErrorCode;
		const std::filesystem::path DiskPath(ResolveTexturePathOnDisk(FilePath));
		const std::filesystem::file_time_type WriteTime = std::filesystem::last_write_time(DiskPath, ErrorCode);
		if (ErrorCode)
		{
			return false;
		}

		OutWriteTime = WriteTime;
		return true;
	}

	bool ShouldSkipTextureScanDirectory(const std::filesystem::path& Path)
	{
		const std::wstring Name = Path.filename().wstring();
		return Name == L".git"
			|| Name == L".vs"
			|| Name == L"Bin"
			|| Name == L"Build"
			|| Name == L"Intermediate"
			|| Name == L"Cache";
	}

	bool ShouldIncludeInTextureAssetList(const std::filesystem::path& ProjectRelativePath)
	{
		const std::filesystem::path NormalizedPath = ProjectRelativePath.lexically_normal();
		if (NormalizedPath.empty())
		{
			return false;
		}

		auto It = NormalizedPath.begin();
		if (It == NormalizedPath.end() || *It != L"Asset")
		{
			return true;
		}

		++It;
		if (It == NormalizedPath.end())
		{
			return true;
		}

		if (*It == L"Editor")
		{
			return false;
		}

		if (*It != L"Content")
		{
			return true;
		}

		++It;
		return It == NormalizedPath.end() || *It != L"Font";
	}

	bool IsSupportedTexturePathString(const FString& Path)
	{
		return UTexture2D::IsSupportedTextureExtension(std::filesystem::path(FPaths::ToWide(Path)));
	}

	bool LoadCPUTextureRGBA(const FString& FilePath, uint32& OutWidth, uint32& OutHeight, std::vector<uint8>& OutPixels)
	{
		OutWidth = 0;
		OutHeight = 0;
		OutPixels.clear();
		const std::wstring DiskPath = ResolveTexturePathOnDisk(FilePath);
		if (DiskPath.empty())
		{
			return false;
		}

		IWICImagingFactory* Factory = nullptr;
		HRESULT HR = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&Factory));
		if (FAILED(HR) || !Factory)
		{
			return false;
		}

		IWICBitmapDecoder* Decoder = nullptr;
		HR = Factory->CreateDecoderFromFilename(
			DiskPath.c_str(),
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnDemand,
			&Decoder);
		if (FAILED(HR) || !Decoder)
		{
			Factory->Release();
			return false;
		}

		IWICBitmapFrameDecode* Frame = nullptr;
		HR = Decoder->GetFrame(0, &Frame);
		if (FAILED(HR) || !Frame)
		{
			Decoder->Release();
			Factory->Release();
			return false;
		}

		HR = Frame->GetSize(&OutWidth, &OutHeight);
		if (FAILED(HR) || OutWidth == 0 || OutHeight == 0)
		{
			Frame->Release();
			Decoder->Release();
			Factory->Release();
			return false;
		}

		IWICFormatConverter* Converter = nullptr;
		HR = Factory->CreateFormatConverter(&Converter);
		if (FAILED(HR) || !Converter)
		{
			Frame->Release();
			Decoder->Release();
			Factory->Release();
			return false;
		}

		HR = Converter->Initialize(
			Frame,
			GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeCustom);
		if (FAILED(HR))
		{
			Converter->Release();
			Frame->Release();
			Decoder->Release();
			Factory->Release();
			return false;
		}

		const size_t PixelCount = static_cast<size_t>(OutWidth) * static_cast<size_t>(OutHeight);
		OutPixels.resize(PixelCount * 4ull);
		HR = Converter->CopyPixels(
			nullptr,
			OutWidth * 4u,
			static_cast<UINT>(OutPixels.size()),
			OutPixels.data());

		Converter->Release();
		Frame->Release();
		Decoder->Release();
		Factory->Release();
		return SUCCEEDED(HR);
	}

	bool LoadTextureMetadataWithRuntimeLoader(const FString& FilePath, ID3D11Device* Device, uint32& OutWidth, uint32& OutHeight)
	{
		OutWidth = 0;
		OutHeight = 0;
		if (!Device)
		{
			return false;
		}

		const std::wstring DiskPath = ResolveTexturePathOnDisk(FilePath);
		if (DiskPath.empty())
		{
			return false;
		}

		FString Extension = FPaths::ToUtf8(std::filesystem::path(DiskPath).extension().generic_wstring());
		AsciiUtils::ToLowerInPlace(Extension);

		ID3D11Resource* Resource = nullptr;
		ID3D11ShaderResourceView* TempSRV = nullptr;
		HRESULT HR = S_OK;
		if (Extension == ".dds")
		{
			HR = DirectX::CreateDDSTextureFromFileEx(
				Device, DiskPath.c_str(),
				0,
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_SHADER_RESOURCE,
				0,
				0,
				DirectX::DDS_LOADER_DEFAULT,
				&Resource, &TempSRV);
		}
		else
		{
			HR = DirectX::CreateWICTextureFromFileEx(
				Device, DiskPath.c_str(),
				0,
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_SHADER_RESOURCE,
				0,
				0,
				DirectX::WIC_LOADER_IGNORE_SRGB,
				&Resource, &TempSRV);
		}

		if (FAILED(HR) || !Resource)
		{
			if (Resource)
			{
				Resource->Release();
			}
			if (TempSRV)
			{
				TempSRV->Release();
			}
			return false;
		}

		ID3D11Texture2D* Tex2D = nullptr;
		if (SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Tex2D))) && Tex2D)
		{
			D3D11_TEXTURE2D_DESC Desc;
			Tex2D->GetDesc(&Desc);
			OutWidth = Desc.Width;
			OutHeight = Desc.Height;
			Tex2D->Release();
		}

		Resource->Release();
		if (TempSRV)
		{
			TempSRV->Release();
		}

		return OutWidth > 0 && OutHeight > 0;
	}
}

UTexture2D::~UTexture2D()
{
	if (SRV)
	{
		if (TrackedTextureMemory > 0)
		{
			MemoryStats::SubTextureMemory(TrackedTextureMemory);
			TrackedTextureMemory = 0;
		}

		SRV->Release();
		SRV = nullptr;
	}

	// 캐시에서 제거
	auto It = TextureCache.find(CacheKeyPath);
	if (It != TextureCache.end() && It->second == this)
	{
		TextureCache.erase(It);
	}
}

void UTexture2D::ReleaseAllGPU()
{
	for (auto& [Path, Texture] : TextureCache)
	{
		if (Texture && Texture->SRV)
		{
			if (Texture->TrackedTextureMemory > 0)
			{
				MemoryStats::SubTextureMemory(Texture->TrackedTextureMemory);
				Texture->TrackedTextureMemory = 0;
			}
			Texture->SRV->Release();
			Texture->SRV = nullptr;
		}
	}
	TextureCache.clear();
}

bool UTexture2D::HasPendingTextureRefresh()
{
	EnsureTextureAssetWatcher();
	return !PendingTextureRefreshPaths.empty();
}

void UTexture2D::RefreshChangedTextures(ID3D11Device* Device)
{
	EnsureTextureAssetWatcher();

	if (!Device)
	{
		return;
	}

	if (PendingTextureRefreshPaths.empty())
	{
		return;
	}

	TSet<FString> ChangedPaths;
	std::swap(ChangedPaths, PendingTextureRefreshPaths);

	for (const FString& ChangedPath : ChangedPaths)
	{
		auto It = TextureCache.find(NormalizeTexturePath(ChangedPath));
		if (It != TextureCache.end() && It->second && It->second->HasSourceFileChanged())
		{
			It->second->LoadInternal(It->first, Device);
		}
	}
}

void UTexture2D::ScanTextureAssets()
{
	EnsureTextureAssetWatcher();
	AvailableTextureFiles.clear();
	TSet<FString> SeenTexturePaths;

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	const std::filesystem::path AssetRoot(FPaths::AssetDir());
	if (!std::filesystem::exists(AssetRoot))
	{
		return;
	}

	std::error_code ErrorCode;
	std::filesystem::recursive_directory_iterator It(
		AssetRoot,
		std::filesystem::directory_options::skip_permission_denied,
		ErrorCode);
	std::filesystem::recursive_directory_iterator End;

	while (It != End)
	{
		if (ErrorCode)
		{
			ErrorCode.clear();
			It.increment(ErrorCode);
			continue;
		}

		const std::filesystem::directory_entry& Entry = *It;
		if (Entry.is_directory(ErrorCode) && ShouldSkipTextureScanDirectory(Entry.path()))
		{
			It.disable_recursion_pending();
		}
		else if (Entry.is_regular_file(ErrorCode))
		{
			bool bIsTextureListItem = IsSupportedTextureExtension(Entry.path());
			if (!bIsTextureListItem && Entry.path().extension() == L".uasset")
			{
				FAssetFileHeader Header;
				bIsTextureListItem = FAssetFileSerializer::ReadAssetHeader(Entry.path(), Header)
					&& Header.ClassId == EAssetClassId::Texture;
			}

			if (bIsTextureListItem)
			{
				const std::filesystem::path RelativePath = Entry.path().lexically_relative(ProjectRoot).lexically_normal();
				if (!ShouldIncludeInTextureAssetList(RelativePath))
				{
					It.increment(ErrorCode);
					continue;
				}

				const FString NormalizedFullPath = FPaths::NormalizePath(FPaths::ToUtf8(RelativePath.generic_wstring()));
				if (!SeenTexturePaths.insert(NormalizedFullPath).second)
				{
					It.increment(ErrorCode);
					continue;
				}

				FTextureAssetListItem Item;
				Item.DisplayName = FPaths::ToUtf8(Entry.path().filename().wstring());
				Item.FullPath = NormalizedFullPath;
				Item.SourceFolder = FPaths::NormalizePath(FPaths::ToUtf8(RelativePath.parent_path().generic_wstring()));
				AvailableTextureFiles.push_back(std::move(Item));
			}
		}

		It.increment(ErrorCode);
	}

	std::sort(
		AvailableTextureFiles.begin(),
		AvailableTextureFiles.end(),
		[](const FTextureAssetListItem& A, const FTextureAssetListItem& B)
		{
			if (A.SourceFolder != B.SourceFolder)
			{
				return A.SourceFolder < B.SourceFolder;
			}

			if (A.DisplayName != B.DisplayName)
			{
				return A.DisplayName < B.DisplayName;
			}

			return A.FullPath < B.FullPath;
		});

	bTextureAssetListDirty = false;
}

const TArray<FTextureAssetListItem>& UTexture2D::GetAvailableTextureFiles()
{
	EnsureTextureAssetWatcher();
	if (bTextureAssetListDirty)
	{
		ScanTextureAssets();
	}
	return AvailableTextureFiles;
}

bool UTexture2D::IsSupportedTextureExtension(const std::filesystem::path& Path)
{
	std::wstring Ext = Path.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), towlower);

	return Ext == L".png"
		|| Ext == L".jpg"
		|| Ext == L".jpeg"
		|| Ext == L".bmp"
		|| Ext == L".tga"
		|| Ext == L".dds";
}

UTexture2D* UTexture2D::LoadFromAssetFile(const FString& AssetPath, ID3D11Device* Device)
{
	if (AssetPath.empty())
	{
		return nullptr;
	}

	const FString NormalizedAssetPath = NormalizeTexturePath(AssetPath);
	auto CachedIt = TextureCache.find(NormalizedAssetPath);
	if (CachedIt != TextureCache.end())
	{
		if (CachedIt->second && Device && !CachedIt->second->EnsureGPUTexture(Device))
		{
			return nullptr;
		}
		return CachedIt->second;
	}

	FString Error;
	UObject* LoadedObject = FAssetFileSerializer::LoadObjectFromAssetFile(
		std::filesystem::path(FPaths::ResolvePathToDisk(NormalizedAssetPath)),
		&Error);
	UTexture2D* LoadedTextureAsset = Cast<UTexture2D>(LoadedObject);
	if (!LoadedTextureAsset)
	{
		if (LoadedObject)
		{
			UObjectManager::Get().DestroyObject(LoadedObject);
		}
		return nullptr;
	}

	LoadedTextureAsset->AssetFilePath = NormalizedAssetPath;
	LoadedTextureAsset->CacheKeyPath = NormalizedAssetPath;

	std::filesystem::file_time_type SourceWriteTime{};
	LoadedTextureAsset->bHasSourceFileWriteTime = TryGetTextureWriteTime(LoadedTextureAsset->SourceFilePath, SourceWriteTime);
	if (LoadedTextureAsset->bHasSourceFileWriteTime)
	{
		LoadedTextureAsset->SourceFileWriteTime = SourceWriteTime;
	}

	if (Device && !LoadedTextureAsset->EnsureGPUTexture(Device))
	{
		UObjectManager::Get().DestroyObject(LoadedTextureAsset);
		return nullptr;
	}

	TextureCache[NormalizedAssetPath] = LoadedTextureAsset;
	return LoadedTextureAsset;
}

UTexture2D* UTexture2D::LoadFromFile(const FString& FilePath, ID3D11Device* Device)
{
	if (FilePath.empty())
	{
		return nullptr;
	}

	const FString NormalizedPath = NormalizeTexturePath(FilePath);
	const std::wstring Extension = std::filesystem::path(FPaths::ToWide(NormalizedPath)).extension().wstring();
	if (_wcsicmp(Extension.c_str(), L".uasset") == 0)
	{
		return LoadFromAssetFile(NormalizedPath, Device);
	}

	// 캐시 히트
	auto It = TextureCache.find(NormalizedPath);
	if (It != TextureCache.end())
	{
		if (It->second && It->second->HasSourceFileChanged())
		{
			It->second->LoadInternal(NormalizedPath, Device);
		}
		else if (It->second && Device)
		{
			It->second->EnsureGPUTexture(Device);
		}
		return It->second;
	}

	// 새 UTexture2D 생성
	UTexture2D* Texture = UObjectManager::Get().CreateObject<UTexture2D>();
	if (!Texture->LoadInternal(FilePath, Device))
	{
		UObjectManager::Get().DestroyObject(Texture);
		return nullptr;
	}

	Texture->AssetFilePath.clear();
	Texture->CacheKeyPath = NormalizedPath;
	TextureCache[NormalizedPath] = Texture;
	return Texture;
}

UTexture2D* UTexture2D::LoadFromCached(const FString& FilePath)
{
	if (FilePath.empty()) return nullptr;

	auto It = TextureCache.find(NormalizeTexturePath(FilePath));
	if (It != TextureCache.end())
	{
		return It->second;
	}

	return nullptr;
}

bool UTexture2D::LoadInternal(const FString& FilePath, ID3D11Device* Device)
{
	const FString NormalizedPath = NormalizeTexturePath(FilePath);
	const std::wstring WidePath = ResolveTexturePathOnDisk(NormalizedPath);
	if (WidePath.empty() || !std::filesystem::exists(WidePath))
	{
		UE_LOG_CATEGORY(Texture, Error, "Failed to load texture: %s", FilePath.c_str());
		return false;
	}

	uint32 NewWidth = 0;
	uint32 NewHeight = 0;
	std::vector<uint8> NewCPUTextureRGBA;
	LoadCPUTextureRGBA(NormalizedPath, NewWidth, NewHeight, NewCPUTextureRGBA);

	std::filesystem::file_time_type NewWriteTime{};
	const bool bHasNewWriteTime = TryGetTextureWriteTime(NormalizedPath, NewWriteTime);

	if (!Device)
	{
		if (NewCPUTextureRGBA.empty() || NewWidth == 0 || NewHeight == 0)
		{
			UE_LOG_CATEGORY(Texture, Error, "Failed to load texture pixels without device: %s", FilePath.c_str());
			return false;
		}

		if (SRV)
		{
			if (TrackedTextureMemory > 0)
			{
				MemoryStats::SubTextureMemory(TrackedTextureMemory);
				TrackedTextureMemory = 0;
			}
			SRV->Release();
			SRV = nullptr;
		}

		Width = NewWidth;
		Height = NewHeight;
		CPUTextureRGBA = std::move(NewCPUTextureRGBA);
		SourceFilePath = NormalizedPath;
		SourceFileWriteTime = NewWriteTime;
		bHasSourceFileWriteTime = bHasNewWriteTime;
		return true;
	}

	const std::filesystem::path ExtensionPath = std::filesystem::path(WidePath).extension();
	FString Extension = FPaths::ToUtf8(ExtensionPath.generic_wstring());
	AsciiUtils::ToLowerInPlace(Extension);

	ID3D11Resource* Resource = nullptr;
	ID3D11ShaderResourceView* NewSRV = nullptr;
	HRESULT hr = S_OK;
	if (Extension == ".dds")
	{
		hr = DirectX::CreateDDSTextureFromFileEx(
			Device, WidePath.c_str(),
			0,                           // maxsize
			D3D11_USAGE_DEFAULT,         // usage
			D3D11_BIND_SHADER_RESOURCE,  // bindFlags
			0,                           // cpuAccessFlags
			0,                           // miscFlags
			DirectX::DDS_LOADER_DEFAULT,
			&Resource, &NewSRV);
	}
	else
	{
		hr = DirectX::CreateWICTextureFromFileEx(
			Device, WidePath.c_str(),
			0,                                    // maxsize
			D3D11_USAGE_DEFAULT,                  // usage
			D3D11_BIND_SHADER_RESOURCE,           // bindFlags
			0,                                    // cpuAccessFlags
			0,                                    // miscFlags
			DirectX::WIC_LOADER_IGNORE_SRGB,      // sRGB 메타데이터 무시 → UNORM 포맷 강제
			&Resource, &NewSRV);
	}

	if (FAILED(hr) || !NewSRV)
	{
		UE_LOG_CATEGORY(Texture, Error, "Failed to load texture: %s", FilePath.c_str());
		if (Resource)
		{
			Resource->Release();
		}
		if (NewSRV)
		{
			NewSRV->Release();
		}
		return false;
	}

	uint64 NewTrackedTextureMemory = 0;

	// 텍스처 크기 추출
	if (Resource)
	{
		NewTrackedTextureMemory = MemoryStats::CalculateTextureMemory(Resource);

		ID3D11Texture2D* Tex2D = nullptr;
		if (SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&Tex2D)))
		{
			D3D11_TEXTURE2D_DESC Desc;
			Tex2D->GetDesc(&Desc);
			NewWidth = Desc.Width;
			NewHeight = Desc.Height;
			Tex2D->Release();
		}
		Resource->Release();
	}

	if (SRV)
	{
		if (TrackedTextureMemory > 0)
		{
			MemoryStats::SubTextureMemory(TrackedTextureMemory);
			TrackedTextureMemory = 0;
		}
		SRV->Release();
		SRV = nullptr;
	}

	SRV = NewSRV;
	Width = NewWidth;
	Height = NewHeight;
	TrackedTextureMemory = NewTrackedTextureMemory;
	CPUTextureRGBA = std::move(NewCPUTextureRGBA);
	SourceFilePath = NormalizedPath;
	SourceFileWriteTime = NewWriteTime;
	bHasSourceFileWriteTime = bHasNewWriteTime;

	if (TrackedTextureMemory > 0)
	{
		MemoryStats::AddTextureMemory(TrackedTextureMemory);
	}

	return true;
}

bool UTexture2D::EnsureGPUTexture(ID3D11Device* Device)
{
	if (SRV)
	{
		return true;
	}

	if (!Device)
	{
		return !CPUTextureRGBA.empty();
	}

	if (CreateSRVFromStoredPixels(Device))
	{
		return true;
	}

	if (!SourceFilePath.empty())
	{
		return LoadInternal(SourceFilePath, Device);
	}

	return false;
}

bool UTexture2D::CreateSRVFromStoredPixels(ID3D11Device* Device)
{
	if (!Device || Width == 0 || Height == 0 || CPUTextureRGBA.empty())
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitialData = {};
	InitialData.pSysMem = CPUTextureRGBA.data();
	InitialData.SysMemPitch = Width * 4u;

	ID3D11Texture2D* TextureResource = nullptr;
	HRESULT HR = Device->CreateTexture2D(&Desc, &InitialData, &TextureResource);
	if (FAILED(HR) || !TextureResource)
	{
		return false;
	}

	ID3D11ShaderResourceView* NewSRV = nullptr;
	HR = Device->CreateShaderResourceView(TextureResource, nullptr, &NewSRV);
	const uint64 NewTrackedTextureMemory = MemoryStats::CalculateTextureMemory(TextureResource);
	TextureResource->Release();

	if (FAILED(HR) || !NewSRV)
	{
		if (NewSRV)
		{
			NewSRV->Release();
		}
		return false;
	}

	if (SRV)
	{
		if (TrackedTextureMemory > 0)
		{
			MemoryStats::SubTextureMemory(TrackedTextureMemory);
		}
		SRV->Release();
	}

	SRV = NewSRV;
	TrackedTextureMemory = NewTrackedTextureMemory;
	if (TrackedTextureMemory > 0)
	{
		MemoryStats::AddTextureMemory(TrackedTextureMemory);
	}

	return true;
}

bool UTexture2D::HasSourceFileChanged() const
{
	if (!AssetFilePath.empty() || SourceFilePath.empty())
	{
		return false;
	}

	std::filesystem::file_time_type CurrentWriteTime{};
	if (!TryGetTextureWriteTime(SourceFilePath, CurrentWriteTime))
	{
		return false;
	}

	if (!bHasSourceFileWriteTime)
	{
		return true;
	}

	return CurrentWriteTime != SourceFileWriteTime;
}

FString UTexture2D::ImportTextureAsset(const FString& SourcePath)
{
	const std::filesystem::path SourceDiskPathW = FindTextureSourceOnDisk(SourcePath);
	if (SourceDiskPathW.empty() || !std::filesystem::exists(SourceDiskPathW))
	{
		UE_LOG_CATEGORY(Texture, Error, "Texture import source not found: source=%s resolved=%s contentRoot=%s cwd=%s", SourcePath.c_str(), FPaths::ToUtf8(SourceDiskPathW.generic_wstring()).c_str(), FPaths::ToUtf8(std::filesystem::path(FPaths::ContentDir()).generic_wstring()).c_str(), FPaths::ToUtf8(std::filesystem::current_path().generic_wstring()).c_str());
		return {};
	}

	const std::filesystem::path ProjectRoot = std::filesystem::path(FPaths::RootDir()).lexically_normal();
	const std::filesystem::path TextureDirectory = MakeImportedTextureDirectory(SourceDiskPathW);
	std::error_code DirectoryError;
	std::filesystem::create_directories(TextureDirectory, DirectoryError);
	if (DirectoryError)
	{
		UE_LOG_CATEGORY(Texture, Error, "Failed to create texture output directory: %s (%s)", FPaths::ToUtf8(TextureDirectory.generic_wstring()).c_str(), DirectoryError.message().c_str());
		return {};
	}

	const FString SourceStem = SanitizeAssetName(FPaths::ToUtf8(SourceDiskPathW.stem().wstring()));
	const std::filesystem::path AssetDiskPath = (TextureDirectory / (L"T_" + FPaths::ToWide(SourceStem) + L".uasset")).lexically_normal();
	const std::filesystem::path RelativeAssetPath = AssetDiskPath.lexically_relative(ProjectRoot);
	const FString AssetPath = NormalizeTexturePath(FPaths::ToUtf8(RelativeAssetPath.generic_wstring()));

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	uint32 ImportedWidth = 0;
	uint32 ImportedHeight = 0;
	std::vector<uint8> ImportedPixels;
	const FString SourceDiskPathString = NormalizeTexturePath(FPaths::ToUtf8(SourceDiskPathW.generic_wstring()));
	const bool bHasEmbeddedPixels =
		LoadCPUTextureRGBA(SourceDiskPathString, ImportedWidth, ImportedHeight, ImportedPixels)
		&& !ImportedPixels.empty()
		&& ImportedWidth > 0
		&& ImportedHeight > 0;

	UTexture2D* Texture = nullptr;
	auto CachedIt = TextureCache.find(AssetPath);
	const bool bCreatedNewTexture = CachedIt == TextureCache.end();
	if (CachedIt != TextureCache.end())
	{
		Texture = CachedIt->second;
	}
	else
	{
		Texture = UObjectManager::Get().CreateObject<UTexture2D>();
	}

	if (!Texture)
	{
		return {};
	}

	Texture->SetFName(FName("T_" + SourceStem));
	Texture->AssetFilePath = AssetPath;
	Texture->CacheKeyPath = AssetPath;
	Texture->SourceFilePath = SourceDiskPathString;
	if (bHasEmbeddedPixels)
	{
		Texture->Width = ImportedWidth;
		Texture->Height = ImportedHeight;
		Texture->CPUTextureRGBA = std::move(ImportedPixels);
	}
	else
	{
		Texture->CPUTextureRGBA.clear();
		Texture->Width = 0;
		Texture->Height = 0;

		if (!LoadTextureMetadataWithRuntimeLoader(SourceDiskPathString, Device, ImportedWidth, ImportedHeight))
		{
			if (bCreatedNewTexture)
			{
				UObjectManager::Get().DestroyObject(Texture);
			}
			UE_LOG_CATEGORY(Texture, Error, "Texture import failed before save: no CPU decode and runtime loader also failed source=%s", SourceDiskPathString.c_str());
			return {};
		}

		Texture->Width = ImportedWidth;
		Texture->Height = ImportedHeight;
		UE_LOG_CATEGORY(Texture, Warning, "Texture import fallback: saving asset without embedded CPU pixels source=%s", SourceDiskPathString.c_str());
	}

	std::filesystem::file_time_type SourceWriteTime{};
	Texture->bHasSourceFileWriteTime = TryGetTextureWriteTime(SourceDiskPathString, SourceWriteTime);
	if (Texture->bHasSourceFileWriteTime)
	{
		Texture->SourceFileWriteTime = SourceWriteTime;
	}

	if (Device)
	{
		Texture->CreateSRVFromStoredPixels(Device); // 실패해도 .uasset 저장은 계속한다.
	}

	TextureCache[AssetPath] = Texture;

	FString Error;
	if (!FAssetFileSerializer::SaveObjectToAssetFile(AssetDiskPath, Texture, &Error))
	{
		if (bCreatedNewTexture)
		{
			TextureCache.erase(AssetPath);
			UObjectManager::Get().DestroyObject(Texture);
		}
		UE_LOG_CATEGORY(Texture, Error, "Failed to save texture asset: assetPath=%s diskPath=%s error=%s", AssetPath.c_str(), FPaths::ToUtf8(AssetDiskPath.generic_wstring()).c_str(), Error.c_str());
		return {};
	}

	MarkTextureAssetListDirty();
	QueueTextureRefresh(AssetPath);
	UE_LOG_CATEGORY(Texture, Info, "Saved texture asset: source=%s assetPath=%s diskPath=%s size=%ux%u bytes=%zu", SourceDiskPathString.c_str(), AssetPath.c_str(), FPaths::ToUtf8(AssetDiskPath.generic_wstring()).c_str(), Texture->Width, Texture->Height, Texture->CPUTextureRGBA.size());
	return AssetPath;
}

void UTexture2D::EnsureTextureAssetWatcher()
{
	if (bTextureAssetWatcherInitialized)
	{
		return;
	}

	bTextureAssetWatcherInitialized = true;
	TextureAssetWatchID = FDirectoryWatcher::Get().Watch(FPaths::AssetDir(), "Asset/");
	if (TextureAssetWatchID == 0)
	{
		return;
	}

	TextureAssetWatchSub = FDirectoryWatcher::Get().Subscribe(
		TextureAssetWatchID,
		[](const TSet<FString>& ChangedPaths)
		{
			for (const FString& Path : ChangedPaths)
			{
				if (IsSupportedTexturePathString(Path))
				{
					MarkTextureAssetListDirty();
					QueueTextureRefresh(Path);
				}
			}
		});
}

void UTexture2D::MarkTextureAssetListDirty()
{
	bTextureAssetListDirty = true;
}

void UTexture2D::QueueTextureRefresh(const FString& TexturePath)
{
	if (TexturePath.empty())
	{
		return;
	}

	PendingTextureRefreshPaths.insert(NormalizeTexturePath(TexturePath));
}

void UTexture2D::Serialize(FArchive& Ar)
{
	Ar << SourceFilePath;
	Ar << Width;
	Ar << Height;

	const uint32 AssetVersion = FAssetFileSerializer::GetCurrentAssetSerializationVersion();
	if (AssetVersion >= 4)
	{
		uint32 PixelByteCount = static_cast<uint32>(CPUTextureRGBA.size());
		Ar << PixelByteCount;
		if (Ar.IsLoading())
		{
			CPUTextureRGBA.resize(PixelByteCount);
		}
		if (PixelByteCount > 0)
		{
			Ar.Serialize(CPUTextureRGBA.data(), PixelByteCount);
		}
	}
	else if (Ar.IsLoading())
	{
		CPUTextureRGBA.clear();
	}

	if (Ar.IsLoading())
	{
		AssetFilePath.clear();
		CacheKeyPath.clear();
		SourceFileWriteTime = {};
		bHasSourceFileWriteTime = TryGetTextureWriteTime(SourceFilePath, SourceFileWriteTime);
	}
}

bool UTexture2D::SampleAlpha(float U, float V, float& OutAlpha) const
{
	OutAlpha = 1.0f;

	if (CPUTextureRGBA.empty() || Width == 0 || Height == 0)
	{
		return false;
	}

	const float ClampedU = std::clamp(U, 0.0f, 1.0f);
	const float ClampedV = std::clamp(V, 0.0f, 1.0f);
	const uint32 X = std::min<uint32>(static_cast<uint32>(ClampedU * static_cast<float>(Width - 1)), Width - 1);
	const uint32 Y = std::min<uint32>(static_cast<uint32>(ClampedV * static_cast<float>(Height - 1)), Height - 1);
	const size_t PixelIndex = (static_cast<size_t>(Y) * static_cast<size_t>(Width) + static_cast<size_t>(X)) * 4ull;

	if (PixelIndex + 3ull >= CPUTextureRGBA.size())
	{
		return false;
	}

	OutAlpha = static_cast<float>(CPUTextureRGBA[PixelIndex + 3ull]) / 255.0f;
	return true;
}
