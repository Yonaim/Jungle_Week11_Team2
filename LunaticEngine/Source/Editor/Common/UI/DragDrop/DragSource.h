#pragma once

#include "Object/Object.h"
#include "ImGui/imgui.h"

struct FDragSourceInfo final
{
	UObject* Object;
};

class FDragSource
{
public:
	void Render(ImVec2 InSize);
	void SetDragSourceInfo(FDragSourceInfo* info) { DragSourceInfo = info; }
	void SetID(FString ID) { DragID = ID; }

protected:
	virtual void RenderSource(ImVec2 InSize) = 0;

	FDragSourceInfo* DragSourceInfo;
private:
	FString DragID;
};

