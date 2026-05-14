#pragma once

#include "Core/CoreTypes.h"

struct ID3D11Device;
struct ID3D11ShaderResourceView;
class UEditorEngine;

// Level Editor frame 전용 Play/Stop/Undo/Redo 툴바.
// document tab bar 바로 아래에 렌더되며, Asset Editor에는 표시하지 않는다.
class FLevelPlayToolbar
{
public:
	FLevelPlayToolbar() = default;
	~FLevelPlayToolbar() = default;

	void Init(UEditorEngine* InEditor, ID3D11Device* InDevice);
	void Release();

	// 레이아웃이 예약해야 할 툴바 높이 (패딩 포함).
	float GetDesiredHeight() const { return ToolbarHeight; }

	// 지정 영역(ImGui Cursor 위치 기준)에 Play/Stop 버튼을 렌더.
	// 호출 전에 ImGui::SetCursorScreenPos로 원하는 위치를 지정하고 호출하면 된다.
	void Render(float Width);

private:
	UEditorEngine* Editor = nullptr;
	ID3D11ShaderResourceView* PlayIcon = nullptr;
	ID3D11ShaderResourceView* PauseIcon = nullptr;
	ID3D11ShaderResourceView* StopIcon = nullptr;
	ID3D11ShaderResourceView* UndoIcon = nullptr;
	ID3D11ShaderResourceView* RedoIcon = nullptr;

	float ToolbarHeight = 44.0f;
	float IconSize = 20.0f;
	float ButtonSpacing = 8.0f;
};
