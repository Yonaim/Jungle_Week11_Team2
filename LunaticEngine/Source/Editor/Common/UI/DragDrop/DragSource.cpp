#include "PCH/LunaticPCH.h"
#include "Common/UI/DragDrop/DragSource.h"
#include "ImGui/imgui.h"

void FDragSource::Render(ImVec2 InSize)
{
	RenderSource(InSize);

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload(DragID.c_str(), &DragSourceInfo, sizeof(FDragSourceInfo));
		RenderSource(InSize);
		ImGui::EndDragDropSource();
	}
}
