#include "PCH/LunaticPCH.h"
#include "EditorGridPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FEditorGridPass)

FEditorGridPass::FEditorGridPass()
{
	PassType = ERenderPass::EditorGrid;
	// EditorGrid는 씬 깊이를 읽어 오브젝트 뒤/앞 관계를 유지하고,
	// 알파 블렌딩으로 라인 강도를 누적한다. NoCull은 평면/스트립 양면 가시성 보장용.
	RenderState = { EDepthStencilState::DepthReadOnly, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}
