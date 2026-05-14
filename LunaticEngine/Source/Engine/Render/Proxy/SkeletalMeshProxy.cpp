#include "PCH/LunaticPCH.h"
#include "SkeletalMeshProxy.h"
#include "Component/SkinnedMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/StaticMeshCommon.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Engine/Runtime/Engine.h"

namespace
{
	UMaterial* ResolveSkeletalSectionMaterial(
		const TArray<FStaticMaterial>& Slots,
		const TArray<UMaterial*>& Overrides,
		int32 MaterialIndex,
		UMaterial*& InOutFallbackMaterial)
	{
		if (MaterialIndex >= 0 && MaterialIndex < static_cast<int32>(Slots.size()))
		{
			if (MaterialIndex < static_cast<int32>(Overrides.size()) && Overrides[MaterialIndex])
			{
				return Overrides[MaterialIndex];
			}
			if (Slots[MaterialIndex].MaterialInterface)
			{
				return Slots[MaterialIndex].MaterialInterface;
			}
		}

		if (!Overrides.empty() && Overrides[0])
		{
			return Overrides[0];
		}
		if (!Slots.empty() && Slots[0].MaterialInterface)
		{
			return Slots[0].MaterialInterface;
		}

		if (!InOutFallbackMaterial)
		{
			InOutFallbackMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");
		}
		return InOutFallbackMaterial;
	}
}

FSkeletalMeshProxy::FSkeletalMeshProxy(USkinnedMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	// CPU Skinning 결과는 매 프레임 뷰포트 기준으로 갱신될 수 있어 플래그를 활성화한다.
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate | EPrimitiveProxyFlags::SkeletalMesh;
}

USkinnedMeshComponent* FSkeletalMeshProxy::GetSkinnedMeshComponent() const
{
	return static_cast<USkinnedMeshComponent*>(GetOwner());
}

bool FSkeletalMeshProxy::HasValidGeometry() const
{
	const FSkeletalLODDrawData& LOD = LODData[CurrentLOD];
	return LOD.DynamicVB.GetBuffer() != nullptr && LOD.VertexCount > 0;
}

void FSkeletalMeshProxy::FillDrawCommandBuffer(FDrawCommandBuffer& OutBuffer) const
{
	const FSkeletalLODDrawData& LOD = LODData[CurrentLOD];
	OutBuffer.VB          = LOD.DynamicVB.GetBuffer();
	OutBuffer.VBStride    = LOD.DynamicVB.GetStride();
	OutBuffer.IB          = LOD.StaticIB.GetBuffer();
	OutBuffer.IndexCount  = LOD.IndexCount;
	OutBuffer.VertexCount = LOD.VertexCount;
}

void FSkeletalMeshProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

void FSkeletalMeshProxy::UpdateMesh()
{
	// 렌더 제출 경계:
	// 컴포넌트/애셋 데이터를 프록시 전용 버퍼로 변환해 캐시한다.
	// DrawCommandBuilder는 이 프록시 캐시만 읽어 커맨드를 만든다.
	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device) return;

	USkinnedMeshComponent* Comp = GetSkinnedMeshComponent();
	USkeletalMesh* SkelMesh = Comp ? Comp->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = SkelMesh ? SkelMesh->GetSkeletalMeshAsset() : nullptr;

	// 현재 SkeletalMesh는 LOD0만 사용한다.
	for (uint32 lod = 0; lod < LODCount; ++lod)
	{
		FSkeletalLODDrawData& LOD = LODData[lod];

		if (LOD.DynamicVB.GetBuffer() == nullptr)
		{
			uint32 InitialCount = Asset ? (uint32)Asset->Vertices.size() : 256;
			LOD.DynamicVB.Create(Device, InitialCount, sizeof(FNormalVertex));
		}

		if (Asset && !Asset->Indices.empty() && LOD.StaticIB.GetBuffer() == nullptr)
		{
			uint32 IdxCount = (uint32)Asset->Indices.size();
			LOD.StaticIB.Create(Device, Asset->Indices.data(), IdxCount, IdxCount * sizeof(uint32));
			LOD.IndexCount  = IdxCount;
			LOD.VertexCount = (uint32)Asset->Vertices.size();
		}
	}

	RebuildSectionDraws();
}

void FSkeletalMeshProxy::UpdateLOD(uint32 LODLevel)
{
	if (LODLevel >= LODCount) LODLevel = LODCount - 1;
	CurrentLOD = LODLevel;
}

void FSkeletalMeshProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	(void)Frame;

	// CPU Skinning 결과 정점을 동적 VB로 업로드한다.
	// 원본 애셋 정점은 변경하지 않고 컴포넌트 런타임 버퍼만 소비한다.
	USkinnedMeshComponent* Comp = GetSkinnedMeshComponent();
	TArray<FNormalVertex>* Verts = Comp ? Comp->GetCPUSkinnedVertices() : nullptr;
	if (!Verts || Verts->empty()) return;

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	ID3D11DeviceContext* Context = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext() : nullptr;
	if (!Device || !Context) return;

	FSkeletalLODDrawData& LOD = LODData[CurrentLOD];
	uint32 VertCount = (uint32)Verts->size();

	if (LOD.DynamicVB.GetBuffer() == nullptr)
		LOD.DynamicVB.Create(Device, VertCount, sizeof(FNormalVertex));

	LOD.DynamicVB.EnsureCapacity(Device, VertCount);
	LOD.DynamicVB.Update(Context, Verts->data(), VertCount);
	LOD.VertexCount = VertCount;
}

void FSkeletalMeshProxy::RebuildSectionDraws()
{
	USkinnedMeshComponent* Comp = GetSkinnedMeshComponent();
	USkeletalMesh* SkelMesh = Comp ? Comp->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = SkelMesh ? SkelMesh->GetSkeletalMeshAsset() : nullptr;

	SectionDraws.clear();

	if (!Asset || Asset->Sections.empty())
	{
		// 섹션 정보가 없는 임시/예외 경로는 단일 fallback draw를 사용한다.
		UMaterial* Material = nullptr;
		if (SkelMesh)
		{
			const TArray<FStaticMaterial>& Slots = SkelMesh->GetStaticMaterials();
			const TArray<UMaterial*>& Overrides = Comp->GetOverrideMaterials();
			UMaterial* FallbackMaterial = nullptr;
			Material = ResolveSkeletalSectionMaterial(Slots, Overrides, 0, FallbackMaterial);
		}

		if (!Material)
		{
			if (!DefaultMaterial)
			{
				DefaultMaterial = UMaterial::CreateTransient(
					ERenderPass::Opaque,
					EBlendState::Opaque,
					EDepthStencilState::Default,
					ERasterizerState::SolidBackCull,
					FShaderManager::Get().GetOrCreate(EShaderPath::UberLit));
			}
			Material = DefaultMaterial;
		}
		if (!Material) return;

		const FSkeletalLODDrawData& LOD = LODData[CurrentLOD];
		uint32 DrawCount = LOD.IndexCount > 0 ? LOD.IndexCount : LOD.VertexCount;
		SectionDraws.push_back({ Material, 0, DrawCount });
		return;
	}

	const TArray<FStaticMaterial>& Slots = SkelMesh->GetStaticMaterials();
	const TArray<UMaterial*>& Overrides = Comp->GetOverrideMaterials();
	UMaterial* FallbackMaterial = nullptr;

	for (const FSkeletalMeshSection& Section : Asset->Sections)
	{
		UMaterial* Mat = ResolveSkeletalSectionMaterial(Slots, Overrides, Section.MaterialIndex, FallbackMaterial);

		// Section.IndexStart/IndexCount는 임포트 시 확정된 제출 단위다.
		SectionDraws.push_back({ Mat, Section.IndexStart, Section.IndexCount });
	}
}
