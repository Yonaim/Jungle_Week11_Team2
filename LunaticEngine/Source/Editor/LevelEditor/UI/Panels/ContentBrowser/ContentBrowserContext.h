#pragma once
#include "ImGui/imgui.h"
#include "Platform/Paths.h"
#include <memory>
#include <filesystem>

class ContentBrowserElement;
class UEditorEngine;

struct ContentBrowserContext final
{
	std::wstring CurrentPath = FPaths::RootDir();
	std::wstring PendingRevealPath;
	ImVec2 ContentSize = ImVec2(92.0f, 126.0f);
	std::shared_ptr<ContentBrowserElement> SelectedElement;

	// UI item callbacks are executed while CachedBrowserElements is being iterated.
	// Operations such as source import can refresh/rebuild the browser immediately,
	// so queue them and process them after the content grid has finished rendering.
	std::filesystem::path PendingImportSourcePath;

	UEditorEngine* EditorEngine;

	bool bIsNeedRefresh = false;
};
