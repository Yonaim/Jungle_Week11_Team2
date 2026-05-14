#include "PCH/LunaticPCH.h"
#include "GizmoOuterPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FGizmoOuterPass)

FGizmoOuterPass::FGizmoOuterPass()
{
	PassType    = ERenderPass::GizmoOuter;
	RenderState = { EDepthStencilState::GizmoOutside, EBlendState::Opaque,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}
