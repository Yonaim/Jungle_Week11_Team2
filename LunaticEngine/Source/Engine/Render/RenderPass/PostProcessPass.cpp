#include "PCH/LunaticPCH.h"
#include "PostProcessPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"

REGISTER_RENDER_PASS(FPostProcessPass)
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Resource/RenderResources.h"

FPostProcessPass::FPostProcessPass()
{
	PassType    = ERenderPass::PostProcess;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

bool FPostProcessPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.DepthTexture || !Frame.DepthCopyTexture || !Frame.StencilCopySRV)
		return false;

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	DC->OMSetRenderTargets(0, nullptr, nullptr);
	Ctx.Resources.UnbindSystemTextures(Ctx.Device);

	DC->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);

	if (Frame.SceneColorCopyTexture && Frame.ViewportRenderTexture)
	{
		DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
	}

	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

	// PostProcess 시작 시 UnbindSystemTextures()가 t16~t20을 모두 비우므로,
	// 풀스크린 디버그 뷰모드(SceneDepth/WorldNormal/LightCulling)가 읽는
	// 시스템 텍스처를 여기서 다시 바인딩해야 한다.
	// - SceneDepth   : t16
	// - SceneColor   : t17
	// - GBufferNormal: t18
	// - Stencil      : t19
	if (Frame.DepthCopySRV)
	{
		ID3D11ShaderResourceView* depthSRV = Frame.DepthCopySRV;
		DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &depthSRV);
	}

	if (Frame.SceneColorCopySRV)
	{
		ID3D11ShaderResourceView* sceneColorSRV = Frame.SceneColorCopySRV;
		DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &sceneColorSRV);
	}

	if (Frame.NormalSRV)
	{
		ID3D11ShaderResourceView* normalSRV = Frame.NormalSRV;
		DC->PSSetShaderResources(ESystemTexSlot::GBufferNormal, 1, &normalSRV);
	}

	ID3D11ShaderResourceView* stencilSRV = Frame.StencilCopySRV;
	DC->PSSetShaderResources(ESystemTexSlot::Stencil, 1, &stencilSRV);

	Cache.bForceAll = true;
	return true;
}
