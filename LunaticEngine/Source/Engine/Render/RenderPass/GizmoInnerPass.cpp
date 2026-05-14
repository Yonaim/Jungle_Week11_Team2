#include "PCH/LunaticPCH.h"
#include "GizmoInnerPass.h"
#include "RenderPassRegistry.h"

REGISTER_RENDER_PASS(FGizmoInnerPass)

FGizmoInnerPass::FGizmoInnerPass()
{
	PassType    = ERenderPass::GizmoInner;
	RenderState = { EDepthStencilState::GizmoInside, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}
