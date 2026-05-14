#include "PCH/LunaticPCH.h"
#include "Render/Proxy/BillboardSceneProxy.h"

#include "Component/BillboardComponent.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Object/ObjectFactory.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/EditorViewportScale.h"
#include "Texture/Texture2D.h"

FBillboardSceneProxy::FBillboardSceneProxy(UBillboardComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;
	ProxyFlags &= ~EPrimitiveProxyFlags::ShowAABB;

	if (InComponent->IsEditorOnly())
	{
		ProxyFlags |= EPrimitiveProxyFlags::EditorOnly;
	}

	if (!InComponent->ParticipatesInRenderSpatialStructure())
	{
		ProxyFlags |= EPrimitiveProxyFlags::NeverCull;
	}
}

UBillboardComponent* FBillboardSceneProxy::GetBillboardComponent() const
{
	return static_cast<UBillboardComponent*>(GetOwner());
}

void FBillboardSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();

	UBillboardComponent* Comp = GetBillboardComponent();
	CachedScale = Comp->GetWorldScale();
	CachedLocation = Comp->GetWorldLocation();
	CachedWidth = Comp->GetWidth();
	CachedHeight = Comp->GetHeight();
}

void FBillboardSceneProxy::UpdateMesh()
{
	UBillboardComponent* Comp = GetBillboardComponent();
	UTexture2D* Texture = Comp ? Comp->GetTexture() : nullptr;

	if (!Texture || !Texture->GetSRV())
	{
		MeshBuffer = GetOwner()->GetMeshBuffer();

		if (!DefaultMaterial || DefaultMaterial->GetShader() != FShaderManager::Get().GetOrCreate(EShaderPath::Primitive))
		{
			if (DefaultMaterial)
			{
				UObjectManager::Get().DestroyObject(DefaultMaterial);
				DefaultMaterial = nullptr;
			}

			DefaultMaterial = UMaterial::CreateTransient(
				ERenderPass::Opaque,
				EBlendState::Opaque,
				EDepthStencilState::Default,
				ERasterizerState::SolidNoCull,
				FShaderManager::Get().GetOrCreate(EShaderPath::Primitive));
		}

		SectionDraws.clear();
		if (MeshBuffer && DefaultMaterial)
		{
			const uint32 IndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
			SectionDraws.push_back({ DefaultMaterial, 0, IndexCount });
		}
		return;
	}

	MeshBuffer = &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::TexturedQuad);

	if (!DefaultMaterial || DefaultMaterial->GetShader() != FShaderManager::Get().GetOrCreate(EShaderPath::Billboard))
	{
		if (DefaultMaterial)
		{
			UObjectManager::Get().DestroyObject(DefaultMaterial);
			DefaultMaterial = nullptr;
		}

		DefaultMaterial = UMaterial::CreateTransient(
			ERenderPass::AlphaBlend,
			EBlendState::AlphaBlend,
			EDepthStencilState::Default,
			ERasterizerState::SolidNoCull,
			FShaderManager::Get().GetOrCreate(EShaderPath::Billboard));
	}

	DefaultMaterial->SetCachedSRV(EMaterialTextureSlot::Diffuse, Texture->GetSRV());

	const uint32 IndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
	SectionDraws.clear();
	SectionDraws.push_back({ DefaultMaterial, 0, IndexCount });
}

void FBillboardSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	if (!bVisible)
	{
		return;
	}

	FVector BillboardForward = Frame.CameraForward * -1.0f;
	FMatrix RotMatrix;
	RotMatrix.SetAxes(BillboardForward, Frame.CameraRight * -1.0f, Frame.CameraUp);
	FVector BillboardScale = CachedScale;
	if (UBillboardComponent* Comp = GetBillboardComponent())
	{
		float EditorScale = 1.0f;
		if (Comp->IsEditorOnlyComponent())
		{
			EditorScale = FEditorViewportScale::ComputeWorldScale(
				Frame.CameraPosition,
				CachedLocation,
				Frame.bIsOrtho,
				Frame.OrthoWidth,
				Frame.ViewportHeight,
				FEditorViewportScaleRules::Billboard)
				* Frame.RenderOptions.ActorHelperBillboardScale;
			BillboardScale *= EditorScale;
		}

		// 렌더에서 사용한 screen-space 보정값을 피킹에도 그대로 사용한다.
		// Ray.Origin만으로 재계산하면 ortho/viewport height/min-max clamp 정보가 빠져 hit area가 렌더 크기와 어긋난다.
		Comp->SetLastRenderedEditorScreenScale(EditorScale);
	}
	BillboardScale.Y *= CachedWidth;
	BillboardScale.Z *= CachedHeight;
	FMatrix BillboardMatrix = FMatrix::MakeScaleMatrix(BillboardScale)
		* RotMatrix * FMatrix::MakeTranslationMatrix(CachedLocation);

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
	MarkPerObjectCBDirty();
}
