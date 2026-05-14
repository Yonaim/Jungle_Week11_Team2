#include "PCH/LunaticPCH.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "Render/Shader/ShaderManager.h"
#include "Materials/Material.h"
#include "Object/ObjectFactory.h"
#include "Render/Command/DrawCommand.h"

// ============================================================
// FPrimitiveSceneProxy — 기본 구현
// ============================================================
FPrimitiveSceneProxy::FPrimitiveSceneProxy(UPrimitiveComponent* InComponent)
	: Owner(InComponent)
{
	if (!Owner->SupportsOutline())
		ProxyFlags &= ~EPrimitiveProxyFlags::SupportsOutline;
}

FPrimitiveSceneProxy::~FPrimitiveSceneProxy()
{
	if (DefaultMaterial)
	{
		UObjectManager::Get().DestroyObject(DefaultMaterial);
		DefaultMaterial = nullptr;
	}
}

bool FPrimitiveSceneProxy::HasValidGeometry() const
{
	return MeshBuffer && MeshBuffer->IsValid();
}

void FPrimitiveSceneProxy::FillDrawCommandBuffer(FDrawCommandBuffer& OutBuffer) const
{
	OutBuffer.VB         = MeshBuffer->GetVertexBuffer().GetBuffer();
	OutBuffer.VBStride   = MeshBuffer->GetVertexBuffer().GetStride();
	OutBuffer.IB         = MeshBuffer->GetIndexBuffer().GetBuffer();
	OutBuffer.IndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
}

ERenderPass FPrimitiveSceneProxy::GetRenderPass() const
{
	if (!SectionDraws.empty() && SectionDraws[0].Material)
		return SectionDraws[0].Material->GetRenderPass();
	return ERenderPass::Opaque;
}

FShader* FPrimitiveSceneProxy::GetShader() const
{
	if (!SectionDraws.empty() && SectionDraws[0].Material)
		return SectionDraws[0].Material->GetShader();
	return nullptr;
}

void FPrimitiveSceneProxy::UpdateTransform()
{
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(Owner->GetWorldMatrix());
	CachedWorldPos = PerObjectConstants.Model.GetLocation();
	CachedBounds = Owner->GetWorldBoundingBox();
	LastLODUpdateFrame = UINT32_MAX;
	MarkPerObjectCBDirty();
}

void FPrimitiveSceneProxy::UpdateMaterial()
{
	// 기본 PrimitiveComponent는 섹션별 머티리얼이 없음 — 서브클래스에서 오버라이드
}

void FPrimitiveSceneProxy::UpdateVisibility()
{
	bVisible = Owner->IsVisible();
	if (bVisible)
	{
		AActor* OwnerActor = Owner->GetOwner();
		if (OwnerActor && !OwnerActor->IsVisible())
			bVisible = false;
	}
	bCastShadow = Owner->GetCastShadow();
	bCastShadowAsTwoSided = Owner->GetCastShadowAsTwoSided();
}

void FPrimitiveSceneProxy::UpdateMesh()
{
	MeshBuffer = Owner->GetMeshBuffer();

	if (!DefaultMaterial)
	{
		DefaultMaterial = UMaterial::CreateTransient(
			ERenderPass::Opaque, EBlendState::Opaque,
			EDepthStencilState::Default, ERasterizerState::SolidBackCull,
			FShaderManager::Get().GetOrCreate(EShaderPath::Primitive));
	}

	SectionDraws.clear();
	if (MeshBuffer && DefaultMaterial)
	{
		uint32 IdxCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
		SectionDraws.push_back({ DefaultMaterial, 0, IdxCount });
	}
}

