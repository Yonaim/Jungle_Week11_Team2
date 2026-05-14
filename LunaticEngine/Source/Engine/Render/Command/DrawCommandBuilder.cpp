#include "PCH/LunaticPCH.h"
#include "DrawCommandBuilder.h"
#include "EditorGridRenderer.h"
#include "Resource/ResourceManager.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/FogParams.h"
#include "Render/Types/LODContext.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Proxy/TextRenderSceneProxy.h"
#include "Render/Proxy/DecalSceneProxy.h"
#include "Render/Scene/FScene.h"
#include "Render/Types/RenderConstants.h"
#include "Render/RenderPass/PassRenderStateTable.h"
#include "Render/Pipeline/RenderCollector.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Texture/Texture2D.h"

#include <algorithm>

// UpdateProxyLOD defined in RenderCollector.cpp (shared)
extern void UpdateProxyLOD(FPrimitiveSceneProxy* Proxy, const FLODUpdateContext& LODCtx);

namespace
{
	uint16 PackUIScreenZOrder(int32 ZOrder)
	{
		const int32 Clamped = (std::clamp)(ZOrder, -2048, 2047);
		return static_cast<uint16>(Clamped + 2048);
	}

	FString ResolvePostProcessMaterialPath(const FString& MaterialPath)
	{
		if (MaterialPath.empty() || MaterialPath.rfind("Asset/", 0) == 0)
		{
			return MaterialPath;
		}

		FString ResolvedPath = "Asset/Content/Materials/" + MaterialPath;
		if (ResolvedPath.find(".uasset") == FString::npos)
		{
			ResolvedPath += ".uasset";
		}
		return ResolvedPath;
	}

	uint64 MakePostProcessSortKey(uint16 UserBits)
	{
		return (static_cast<uint64>(ERenderPass::PostProcess) & 0xF) << 56
			| (static_cast<uint64>(UserBits) & 0xFFF);
	}
}

// ============================================================
// Create / Release
// ============================================================

void FDrawCommandBuilder::Create(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, const FPassRenderStateTable* InPassRenderStateTable)
{
	CachedDevice = InDevice;
	CachedContext = InContext;
	PassRenderStateTable = InPassRenderStateTable;

	EditorLines.Create(InDevice);
	EditorLinesNoDepth.Create(InDevice);
	GridLines.Create(InDevice);
	FontGeometry.Create(InDevice);
	ScreenQuads.Create(InDevice);

	GridCB.Create(InDevice, EditorGridRenderer::GetConstantBufferSize());
	GridAxisCB.Create(InDevice, EditorGridRenderer::GetConstantBufferSize());
	FogCB.Create(InDevice, sizeof(FFogConstants));
	FadeCB.Create(InDevice, sizeof(FFadeConstants));
	OutlineCB.Create(InDevice, sizeof(FOutlinePostProcessConstants));
	SceneDepthCB.Create(InDevice, sizeof(FSceneDepthPConstants));
	FXAACB.Create(InDevice, sizeof(FFXAAConstants));
	GammaCorrectionCB.Create(InDevice, sizeof(FGammaCorrectionConstants));
}

void FDrawCommandBuilder::Release()
{
	EditorLines.Release();
	EditorLinesNoDepth.Release();
	GridLines.Release();
	FontGeometry.Release();
	ScreenQuads.Release();

	for (FConstantBuffer& CB : PerObjectCBPool)
	{
		CB.Release();
	}
	PerObjectCBPool.clear();

	GridCB.Release();
	GridAxisCB.Release();
	FogCB.Release();
	FadeCB.Release();
	OutlineCB.Release();
	SceneDepthCB.Release();
	FXAACB.Release();
	GammaCorrectionCB.Release();
}

// ============================================================
// BeginCollect — DrawCommandList + 동적 지오메트리 초기화
// ============================================================
void FDrawCommandBuilder::BeginCollect(const FFrameContext& Frame, uint32 MaxProxyCount)
{
	DrawCommandList.Reset();
	CollectViewMode = Frame.RenderOptions.ViewMode;
	bHasSelectionMaskCommands = false;

	// PerObjectCBPool 미리 할당 — Collect 도중 resize로 FDrawCommand.PerObjectCB
	// 포인터가 무효화되는 것을 방지
	if (MaxProxyCount > 0)
		EnsurePerObjectCBPoolCapacity(MaxProxyCount);

	// 동적 지오메트리 초기화
	EditorLines.Clear();
	EditorLinesNoDepth.Clear();
	GridLines.Clear();
	FontGeometry.Clear();
	FontGeometry.ClearScreen();
	ScreenQuads.Clear();
}

// ============================================================
// SelectEffectiveShader — ViewMode에 따른 UberLit 셰이더 변형 선택
// ============================================================
FShader* FDrawCommandBuilder::SelectEffectiveShader(FShader* ProxyShader, EViewMode ViewMode)
{
	if (ProxyShader != FShaderManager::Get().GetOrCreate(EShaderPath::UberLit))
		return ProxyShader;

	switch (ViewMode)
	{
	case EViewMode::Unlit:        return FShaderManager::Get().GetOrCreate(FShaderKey(EShaderPath::UberLit, EUberLitDefines::Unlit));
	case EViewMode::Lit_Gouraud:  return FShaderManager::Get().GetOrCreate(FShaderKey(EShaderPath::UberLit, EUberLitDefines::Gouraud));
	case EViewMode::Lit_Lambert:  return FShaderManager::Get().GetOrCreate(FShaderKey(EShaderPath::UberLit, EUberLitDefines::Lambert));
	case EViewMode::Lit_Phong:    return FShaderManager::Get().GetOrCreate(FShaderKey(EShaderPath::UberLit, EUberLitDefines::Phong));
	case EViewMode::LightCulling: return FShaderManager::Get().GetOrCreate(FShaderKey(EShaderPath::UberLit, EUberLitDefines::Phong));
	default:                      return ProxyShader;
	}
}

// ============================================================
// ApplyMaterialRenderState — Material 렌더 상태 오버라이드 (Wireframe 우선)
// ============================================================
void FDrawCommandBuilder::ApplyMaterialRenderState(FDrawCommandRenderState& OutState, const UMaterial* Mat, const FDrawCommandRenderState& BaseState)
{
	OutState.Blend = Mat->GetBlendState();
	OutState.DepthStencil = Mat->GetDepthStencilState();
	if (BaseState.Rasterizer != ERasterizerState::WireFrame)
		OutState.Rasterizer = Mat->GetRasterizerState();
}

// ============================================================
// BuildCommandForProxy — Proxy → FDrawCommand 변환
// ============================================================
void FDrawCommandBuilder::BuildCommandForProxy(const FPrimitiveSceneProxy& Proxy, ERenderPass Pass)
{
	if (!Proxy.HasValidGeometry()) return;

	ID3D11DeviceContext* Ctx = CachedContext;

	// PassState → RenderState 변환 (Wireframe 오버라이드 포함)
	const FDrawCommandRenderState BaseRenderState = PassRenderStateTable->ToDrawCommandState(Pass, CollectViewMode);

	// PerObjectCB 업데이트
	FConstantBuffer* PerObjCB = GetPerObjectCBForProxy(Proxy);
	if (PerObjCB && Proxy.NeedsPerObjectCBUpload())
	{
		PerObjCB->Update(Ctx, &Proxy.GetPerObjectConstants(), sizeof(FPerObjectConstants));
		Proxy.ClearPerObjectCBDirty();
	}

	// SelectionMask 커맨드 존재 추적
	if (Pass == ERenderPass::SelectionMask)
		bHasSelectionMaskCommands = true;

	const bool bDepthOnly = (Pass == ERenderPass::PreDepth);

	// Proxy → FDrawCommandBuffer 변환 (Static/Dynamic 공통 경로)
	FDrawCommandBuffer ProxyBuffer;
	Proxy.FillDrawCommandBuffer(ProxyBuffer);

	// 섹션당 1개 커맨드 (per-section 셰이더)
	for (const FMeshSectionDraw& Section : Proxy.GetSectionDraws())
	{
		if (Section.IndexCount == 0) continue;
		if (!ProxyBuffer.IB) continue;

		// Section Material이 셰이더를 가지면 사용, 없으면 Proxy 폴백
		FShader* SectionShader = (Section.Material && Section.Material->GetShader())
			? Section.Material->GetShader()
			: Proxy.GetShader();
		FShader* EffectiveShader = SelectEffectiveShader(SectionShader, CollectViewMode);

		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		Cmd.Pass = Pass;
		Cmd.Shader = EffectiveShader;
		Cmd.RenderState = BaseRenderState;
		Cmd.Buffer = ProxyBuffer;
		Cmd.PerObjectCB = PerObjCB;
		Cmd.Buffer.FirstIndex = Section.FirstIndex;
		Cmd.Buffer.IndexCount = Section.IndexCount;

		if (!bDepthOnly && Section.Material)
		{
			UMaterial* Mat = Section.Material;

			// dirty CB 업로드 (ConstantBufferMap + PerShaderOverride)
			Mat->FlushDirtyBuffers(CachedDevice, Ctx);

			Cmd.Bindings.PerShaderCB[0] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader0);
			Cmd.Bindings.PerShaderCB[1] = Mat->GetGPUBufferBySlot(ECBSlot::PerShader1);

			// CachedSRVs에서 직접 복사 (map lookup 회피)
			const ID3D11ShaderResourceView* const* MatSRVs = Mat->GetCachedSRVs();
			for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
				Cmd.Bindings.SRVs[s] = const_cast<ID3D11ShaderResourceView*>(MatSRVs[s]);

			// 섹션별 Material의 RenderPass가 현재 Pass와 일치할 때만 렌더 상태 오버라이드
			if (Pass == Mat->GetRenderPass())
				ApplyMaterialRenderState(Cmd.RenderState, Mat, BaseRenderState);
		}

		Cmd.BuildSortKey();
	}
}

// ============================================================
// BuildDecalCommandForReceiver
// ============================================================
void FDrawCommandBuilder::BuildDecalCommandForReceiver(const FPrimitiveSceneProxy& ReceiverProxy, const FPrimitiveSceneProxy& DecalProxy)
{
	if (!ReceiverProxy.HasValidGeometry()) return;

	// Decal Material은 SectionDraws[0]에 저장됨
	UMaterial* DecalMat = DecalProxy.GetSectionDraws().empty() ? nullptr : DecalProxy.GetSectionDraws()[0].Material;
	if (!DecalMat || !DecalMat->GetShader()) return;

	ID3D11DeviceContext* Ctx = CachedContext;
	const ERenderPass DecalPass = DecalProxy.GetRenderPass();
	const FDrawCommandRenderState BaseRenderState = PassRenderStateTable->ToDrawCommandState(DecalPass, CollectViewMode);

	FConstantBuffer* ReceiverPerObjCB = GetPerObjectCBForProxy(ReceiverProxy);
	if (ReceiverPerObjCB && ReceiverProxy.NeedsPerObjectCBUpload())
	{
		ReceiverPerObjCB->Update(Ctx, &ReceiverProxy.GetPerObjectConstants(), sizeof(FPerObjectConstants));
		ReceiverProxy.ClearPerObjectCBDirty();
	}

	// Decal Material의 CB 업로드 (PerShaderOverride 포함)
	DecalMat->FlushDirtyBuffers(CachedDevice, Ctx);

	FDrawCommandBuffer ReceiverBuffer;
	ReceiverProxy.FillDrawCommandBuffer(ReceiverBuffer);

	auto AddDraw = [&](uint32 FirstIndex, uint32 IndexCount)
		{
			if (IndexCount == 0) return;

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.Pass = DecalPass;
			Cmd.Shader = DecalMat->GetShader();
			Cmd.RenderState = BaseRenderState;

			// 머티리얼 기반 렌더 상태 오버라이드
			ApplyMaterialRenderState(Cmd.RenderState, DecalMat, BaseRenderState);

			Cmd.Buffer = ReceiverBuffer;
			Cmd.Buffer.FirstIndex = FirstIndex;
			Cmd.Buffer.IndexCount = IndexCount;
			Cmd.PerObjectCB = ReceiverPerObjCB;
			Cmd.Bindings.PerShaderCB[0] = DecalMat->GetGPUBufferBySlot(ECBSlot::PerShader0);

			// Material의 CachedSRVs에서 텍스처 바인딩
			const ID3D11ShaderResourceView* const* MatSRVs = DecalMat->GetCachedSRVs();
			for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
				Cmd.Bindings.SRVs[s] = const_cast<ID3D11ShaderResourceView*>(MatSRVs[s]);

			Cmd.BuildSortKey();
		};

	if (!ReceiverProxy.GetSectionDraws().empty())
	{
		for (const FMeshSectionDraw& Section : ReceiverProxy.GetSectionDraws())
		{
			AddDraw(Section.FirstIndex, Section.IndexCount);
		}
	}
	else if (ReceiverBuffer.IB)
	{
		AddDraw(0, ReceiverBuffer.IndexCount);
	}
}

// ============================================================
// AddWorldText — Font 프록시 배칭
// ============================================================
void FDrawCommandBuilder::AddWorldText(const FTextRenderSceneProxy* TextProxy, const FFrameContext& Frame)
{
	(void)Frame;
	FontGeometry.AddWorldText(
		TextProxy->CachedText,
		TextProxy->CachedTextWorldMatrix.GetLocation(),
		TextProxy->CachedTextRight,
		TextProxy->CachedTextUp,
		TextProxy->CachedTextWorldMatrix.GetScale(),
		TextProxy->CachedColor,
		TextProxy->CachedFont,
		TextProxy->CachedFontScale
	);
}

// ============================================================
// BuildCommands — 프록시 커맨드 + 동적 커맨드 일괄 생성
// ============================================================
void FDrawCommandBuilder::BuildCommands(const FFrameContext& Frame, FScene* Scene, const FCollectOutput& Output)
{
	if (Scene)
		BuildProxyCommands(Frame, *Scene, Output);

	BuildDynamicCommands(Frame, Scene);
}

// ============================================================
// BuildProxyCommands — RenderableProxies → DrawCommand
// ============================================================
void FDrawCommandBuilder::BuildProxyCommands(const FFrameContext& Frame, FScene& Scene, const FCollectOutput& Output)
{
	const bool bShowBoundingVolume = Frame.RenderOptions.ShowFlags.bBoundingVolume;

	for (FPrimitiveSceneProxy* Proxy : Output.RenderableProxies)
	{
		if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::FontBatched))
		{
			const FTextRenderSceneProxy* TextProxy = static_cast<const FTextRenderSceneProxy*>(Proxy);
			if (!TextProxy->CachedText.empty())
			{
				AddWorldText(TextProxy, Frame);
			}
		}
		else if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::Decal))
			BuildDecalCommands(Proxy, Frame, Output);
		else
			BuildMeshCommands(Proxy);

		if (Proxy->IsSelected())
			BuildSelectionCommands(Proxy, bShowBoundingVolume, Scene);
	}
}

// ============================================================
// BuildDecalCommands — Decal → Receiver 순회 + 커맨드 생성
// ============================================================
void FDrawCommandBuilder::BuildDecalCommands(FPrimitiveSceneProxy* Proxy, const FFrameContext& Frame, const FCollectOutput& Output)
{
	FDecalSceneProxy* DecalProxy = static_cast<FDecalSceneProxy*>(Proxy);

	for (FPrimitiveSceneProxy* ReceiverProxy : DecalProxy->GetReceiverProxies())
	{
		if (!ReceiverProxy || Output.VisibleProxySet.find(ReceiverProxy) == Output.VisibleProxySet.end())
			continue;

		UpdateProxyLOD(ReceiverProxy, Frame.LODContext);

		if (ReceiverProxy->HasProxyFlag(EPrimitiveProxyFlags::PerViewportUpdate))
			ReceiverProxy->UpdatePerViewport(Frame);

		BuildDecalCommandForReceiver(*ReceiverProxy, *DecalProxy);
	}
}

// ============================================================
// BuildMeshCommands — 일반 메시 (PreDepth + 메인 패스)
// ============================================================
void FDrawCommandBuilder::BuildMeshCommands(const FPrimitiveSceneProxy* Proxy)
{
	const ERenderPass ProxyPass = Proxy->GetRenderPass();
	const bool bDebugViewMode =
		CollectViewMode == EViewMode::SceneDepth ||
		CollectViewMode == EViewMode::WorldNormal ||
		CollectViewMode == EViewMode::LightCulling;

	// 디버그 뷰(SceneDepth/WorldNormal/LightCulling)는 최종 화면을 포스트프로세스가 덮어쓴다.
	// 따라서 반투명 머티리얼이어도 Opaque 경로로 한 번 제출해 depth/normal MRT를 채워 주는 편이
	// 에셋 프리뷰에서 훨씬 기대한 결과에 가깝다.
	if (bDebugViewMode && ProxyPass != ERenderPass::Opaque)
	{
		BuildCommandForProxy(*Proxy, ERenderPass::Opaque);
		return;
	}

	if (ProxyPass == ERenderPass::Opaque)
		BuildCommandForProxy(*Proxy, ERenderPass::PreDepth);

	BuildCommandForProxy(*Proxy, ProxyPass);
}

// ============================================================
// BuildSelectionCommands — 아웃라인 + AABB
// ============================================================
void FDrawCommandBuilder::BuildSelectionCommands(FPrimitiveSceneProxy* Proxy, bool bShowBoundingVolume, FScene& Scene)
{
	if (Proxy->HasProxyFlag(EPrimitiveProxyFlags::SupportsOutline))
		BuildCommandForProxy(*Proxy, ERenderPass::SelectionMask);

	if (bShowBoundingVolume && Proxy->HasProxyFlag(EPrimitiveProxyFlags::ShowAABB))
		Scene.AddDebugAABB(Proxy->GetCachedBounds().Min, Proxy->GetCachedBounds().Max, FColor::White());
}

// ============================================================
// BuildDynamicCommands — Scene 경량 데이터 → 동적 지오메트리 → FDrawCommand
// ============================================================
void FDrawCommandBuilder::BuildDynamicCommands(const FFrameContext& Frame, const FScene* Scene)
{
	PrepareDynamicGeometry(Frame, Scene);
	BuildDynamicDrawCommands(Frame, Scene);
}

// ============================================================
// PrepareDynamicGeometry — FScene의 경량 데이터 → 라인/폰트 지오메트리
// ============================================================
void FDrawCommandBuilder::PrepareDynamicGeometry(const FFrameContext& Frame, const FScene* Scene)
{
	if (!Scene) return;

	// --- Editor 패스: AABB 디버그 박스 + DebugDraw 라인 ---
	for (const auto& AABB : Scene->GetDebugAABBs())
	{
		EditorLines.AddBillboardAABB(FBoundingBox{ AABB.Min, AABB.Max }, AABB.Color, Frame, Frame.RenderOptions.DebugLineThickness);
	}
	for (const auto& Line : Scene->GetDebugLines())
	{
		FLineGeometry& TargetLines = Line.bDepthTest ? EditorLines : EditorLinesNoDepth;
		TargetLines.AddBillboardLine(Line.Start, Line.End, Line.Color.ToVector4(), Frame, Frame.RenderOptions.DebugLineThickness);
	}

	// 픽셀 셰이더 기반 그리드는 BuildEditorGridCommand에서 생성합니다.

	// --- ScreenText 패스: 스크린 공간 텍스트 ---
	for (const auto& Quad : Scene->GetScreenQuads())
	{
		ScreenQuads.AddScreenQuad(
			Quad.Position.X,
			Quad.Position.Y,
			Quad.Size.X,
			Quad.Size.Y,
			Frame.ViewportWidth,
			Frame.ViewportHeight,
			Quad.TopColor,
			Quad.BottomColor,
			Quad.UVMin,
			Quad.UVMax,
			Quad.TextureSRV,
			PackUIScreenZOrder(Quad.ZOrder),
			Quad.bSolidColorOnly);
	}

	// --- ScreenText 패스: 스크린 공간 텍스트 ---
	for (const auto& Text : Scene->GetScreenTexts())
	{
		if (!Text.Text.empty())
		{
			FontGeometry.AddScreenText(
				Text.Text,
				Text.Position.X,
				Text.Position.Y,
				Frame.ViewportWidth,
				Frame.ViewportHeight,
				Text.Color,
				Text.Font,
				Text.Scale,
				Text.LineSpacing,
				Text.LetterSpacing
			);
		}
	}
}

// ============================================================
// BuildDynamicDrawCommands — 오케스트레이터
// ============================================================
void FDrawCommandBuilder::BuildDynamicDrawCommands(const FFrameContext& Frame, const FScene* Scene)
{
	EViewMode ViewMode = Frame.RenderOptions.ViewMode;
	BuildEditorGridCommand(Frame, Scene);
	BuildEditorLineCommands(ViewMode);
	BuildPostProcessCommands(Frame, Scene);
	BuildUICommands(ViewMode);
	BuildFontCommands(ViewMode);
}

// ============================================================
// EmitLineCommand — 라인 지오메트리 → FDrawCommand 공통 헬퍼
// ============================================================
void FDrawCommandBuilder::EmitLineCommand(FLineGeometry& Lines, FShader* Shader, const FDrawCommandRenderState& RS)
{
	if (Lines.GetLineCount() > 0 && Lines.UploadBuffers(CachedContext))
	{
		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		Cmd.Pass = ERenderPass::EditorLines;
		Cmd.Shader = Shader;
		Cmd.RenderState = RS;
		Cmd.RenderState.Topology = Lines.GetTopology();
		Cmd.Buffer = { Lines.GetVBBuffer(), Lines.GetVBStride(), Lines.GetIBBuffer() };
		Cmd.Buffer.IndexCount = Lines.GetIndexCount();
		Cmd.BuildSortKey();
	}
}

// ============================================================
// BuildEditorLineCommands — EditorLines + GridLines
// ============================================================
void FDrawCommandBuilder::BuildEditorLineCommands(EViewMode ViewMode)
{
	FShader* EditorShader = FShaderManager::Get().GetOrCreate(EShaderPath::Editor);
	const FDrawCommandRenderState EditorLinesRS = PassRenderStateTable->ToDrawCommandState(ERenderPass::EditorLines, ViewMode);
	FDrawCommandRenderState NoDepthLinesRS = EditorLinesRS;
	NoDepthLinesRS.DepthStencil = EDepthStencilState::NoDepth;

	EmitLineCommand(EditorLines, EditorShader, EditorLinesRS);
	EmitLineCommand(EditorLinesNoDepth, EditorShader, NoDepthLinesRS);
	EmitLineCommand(GridLines, EditorShader, EditorLinesRS);
}

void FDrawCommandBuilder::BuildEditorGridCommand(const FFrameContext& Frame, const FScene* Scene)
{
	// Grid는 Editor에서 수집된 Scene 경량 데이터가 있을 때만 그린다.
	if (!Scene || !Scene->HasGrid())
	{
		return;
	}

	const FShowFlags& ShowFlags = Frame.RenderOptions.ShowFlags;
	// Grid/WorldAxis가 모두 꺼진 경우에는 pass 자체를 생성하지 않는다.
	if (!ShowFlags.bGrid && !ShowFlags.bWorldAxis)
	{
		return;
	}

	// Grid는 전용 HLSL(Shaders/Editor/Grid.hlsl)로 렌더한다.
	FShader* GridShader = FShaderManager::Get().GetOrCreate(EShaderPath::Grid);
	if (!GridShader)
	{
		return;
	}

	const FGridRenderSettings Settings = EditorGridRenderer::SanitizeSettings(Frame.RenderOptions.GridRenderSettings);
	const FDrawCommandRenderState GridRS = PassRenderStateTable->ToDrawCommandState(ERenderPass::EditorGrid, Frame.RenderOptions.ViewMode);

	if (ShowFlags.bGrid)
	{
		EditorGridRenderer::FGridShaderConstants GridConstants;
		// DrawAxisPass=0 경로: 그리드 평면 + 평면 위 Axis 강조(X/Y 또는 선택 평면 축).
		EditorGridRenderer::BuildShaderConstants(
			Frame, *Scene, Settings,
			true, false, false,
			GridConstants);

		GridCB.Update(CachedContext, &GridConstants, sizeof(GridConstants));

		FDrawCommand& GridCmd = DrawCommandList.AddCommand();
		GridCmd.Pass = ERenderPass::EditorGrid;
		GridCmd.Shader = GridShader;
		GridCmd.RenderState = GridRS;
		// Grid.hlsl VS는 VertexID만 사용하므로 VB 없이 2개 삼각형(6 vertices)으로 평면을 만든다.
		GridCmd.Buffer.VertexCount = 6;
		GridCmd.Bindings.PerShaderCB[0] = &GridCB;
		GridCmd.BuildSortKey();
	}

	if (ShowFlags.bWorldAxis)
	{
		EditorGridRenderer::FGridShaderConstants AxisConstants;
		// DrawAxisPass=1 경로: 월드 축 스트립 전용 도형을 별도 DrawCall로 렌더한다.
		EditorGridRenderer::BuildShaderConstants(
			Frame, *Scene, Settings,
			false, true, true,
			AxisConstants);

		GridAxisCB.Update(CachedContext, &AxisConstants, sizeof(AxisConstants));

		FDrawCommand& AxisCmd = DrawCommandList.AddCommand();
		AxisCmd.Pass = ERenderPass::EditorGrid;
		AxisCmd.Shader = GridShader;
		AxisCmd.RenderState = GridRS;
		// X/Y/Z 3개 축 스트립(축당 6 vertices) = 18 vertices.
		AxisCmd.Buffer.VertexCount = 18;
		AxisCmd.Bindings.PerShaderCB[0] = &GridAxisCB;
		AxisCmd.BuildSortKey();
	}
}

// ============================================================
// BuildPostProcessCommands — HeightFog, Outline, SceneDepth, WorldNormal, FXAA
// ============================================================
void FDrawCommandBuilder::BuildPostProcessCommands(const FFrameContext& Frame, const FScene* CollectScene)
{
	ID3D11DeviceContext* Ctx = CachedContext;
	EViewMode ViewMode = Frame.RenderOptions.ViewMode;
	const FDrawCommandRenderState PPRS = PassRenderStateTable->ToDrawCommandState(ERenderPass::PostProcess, ViewMode);
	FDrawCommandRenderState DebugViewRS = PPRS;
	// SceneDepth / WorldNormal / LightCulling은 최종 디버그 화면을 그대로 덮어써야 한다.
	// PostProcessPass의 기본 AlphaBlend를 그대로 쓰면 기존 SceneColor와 섞여 보일 수 있다.
	DebugViewRS.Blend = EBlendState::Opaque;

	// HeightFog (UserBits=0 → Outline보다 먼저)
	if (Frame.RenderOptions.ShowFlags.bFog && CollectScene && CollectScene->GetEnvironment().HasFog())
	{
		FShader* FogShader = FShaderManager::Get().GetOrCreate(EShaderPath::HeightFog);
		if (FogShader)
		{
			const FFogParams& FogParams = CollectScene->GetEnvironment().GetFogParams();
			FFogConstants fogData = {};
			fogData.InscatteringColor = FogParams.InscatteringColor;
			fogData.Density = FogParams.Density;
			fogData.HeightFalloff = FogParams.HeightFalloff;
			fogData.FogBaseHeight = FogParams.FogBaseHeight;
			fogData.StartDistance = FogParams.StartDistance;
			fogData.CutoffDistance = FogParams.CutoffDistance;
			fogData.MaxOpacity = FogParams.MaxOpacity;
			FogCB.Update(Ctx, &fogData, sizeof(FFogConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(FogShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = &FogCB;
			Cmd.SortKey = MakePostProcessSortKey(0);
		}
	}

	// Outline (UserBits=1 → HeightFog 뒤)
	if (bHasSelectionMaskCommands)
	{
		FShader* PPShader = FShaderManager::Get().GetOrCreate(EShaderPath::Outline);
		if (PPShader)
		{
			FOutlinePostProcessConstants ppConstants;
			ppConstants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
			ppConstants.OutlineThickness = 3.0f;
			OutlineCB.Update(Ctx, &ppConstants, sizeof(ppConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(PPShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = &OutlineCB;
			Cmd.SortKey = MakePostProcessSortKey(1);
		}
	}

	// SceneDepth (UserBits=2 → Outline 뒤)
	if (CollectViewMode == EViewMode::SceneDepth)
	{
		FShader* DepthShader = FShaderManager::Get().GetOrCreate(EShaderPath::SceneDepth);
		if (DepthShader)
		{
			FViewportRenderOptions Opts = Frame.RenderOptions;
			FSceneDepthPConstants depthData = {};
			depthData.Exponent = Opts.Exponent;
			depthData.NearClip = Frame.NearClip;
			depthData.FarClip = Frame.FarClip;
			depthData.Mode = Opts.SceneDepthVisMode;
			SceneDepthCB.Update(Ctx, &depthData, sizeof(FSceneDepthPConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(DepthShader, ERenderPass::PostProcess, DebugViewRS);
			Cmd.Bindings.PerShaderCB[0] = &SceneDepthCB;
			Cmd.SortKey = MakePostProcessSortKey(2);
		}
	}

	// WorldNormal (UserBits=3 → SceneDepth 뒤)
	if (CollectViewMode == EViewMode::WorldNormal)
	{
		FShader* NormalShader = FShaderManager::Get().GetOrCreate(EShaderPath::SceneNormal);
		if (NormalShader)
		{
			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(NormalShader, ERenderPass::PostProcess, DebugViewRS);
			Cmd.SortKey = MakePostProcessSortKey(3);
		}
	}

	// LightCulling (UserBits=4 → WorldNormal 뒤)
	if (CollectViewMode == EViewMode::LightCulling || Frame.RenderOptions.ShowFlags.bLightHitMap)
	{
		FShader* CullingShader = FShaderManager::Get().GetOrCreate(EShaderPath::LightCulling);
		if (CullingShader)
		{
			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(CullingShader, ERenderPass::PostProcess, DebugViewRS);
			Cmd.SortKey = MakePostProcessSortKey(4);
		}
	}
	uint16 PostProcessMaterialSort = 5;
	for (const FPostProcessSettings::FMaterialEntry& Entry : Frame.PostProcessSettings.Materials)
	{
		if (Entry.MaterialPath.empty() || Entry.BlendWeight <= 0.0f)
		{
			continue;
		}

		UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(ResolvePostProcessMaterialPath(Entry.MaterialPath));
		if (!Material || !Material->GetShader())
		{
			continue;
		}

		for (const auto& Pair : Entry.Parameters.ScalarParameter)
		{
			Material->SetScalarParameter(Pair.first.ToString(), Pair.second);
		}

		Material->FlushDirtyBuffers(CachedDevice, Ctx);

		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		Cmd.InitFullscreenTriangle(Material->GetShader(), ERenderPass::PostProcess, PPRS);
		Cmd.Bindings.PerShaderCB[0] = Material->GetGPUBufferBySlot(ECBSlot::PerShader0);
		Cmd.Bindings.PerShaderCB[1] = Material->GetGPUBufferBySlot(ECBSlot::PerShader1);

		const ID3D11ShaderResourceView* const* MatSRVs = Material->GetCachedSRVs();
		for (int s = 0; s < (int)EMaterialTextureSlot::Max; s++)
		{
			Cmd.Bindings.SRVs[s] = const_cast<ID3D11ShaderResourceView*>(MatSRVs[s]);
		}

		Cmd.SortKey = MakePostProcessSortKey(PostProcessMaterialSort++);
	}

	const float CameraFadeAmount = std::clamp(Frame.PostProcessSettings.FadeAmount, 0.0f, 1.0f);
	if (CameraFadeAmount > 0.0f)
	{
		FShader* FadeShader = FShaderManager::Get().GetOrCreate(EShaderPath::Fade);
		if (FadeShader)
		{
			FFadeConstants fadeConstants = {};
			fadeConstants.FadeColor = Frame.PostProcessSettings.FadeColor.ToVector4();
			fadeConstants.FadeAmount = CameraFadeAmount;
			FadeCB.Update(Ctx, &fadeConstants, sizeof(FFadeConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(FadeShader, ERenderPass::PostProcess, PPRS);
			Cmd.Bindings.PerShaderCB[0] = &FadeCB;
			Cmd.SortKey = MakePostProcessSortKey(PostProcessMaterialSort++);
		}
	}

	const bool bDebugViewMode =
		ViewMode == EViewMode::SceneDepth ||
		ViewMode == EViewMode::WorldNormal ||
		ViewMode == EViewMode::LightCulling;

	// FXAA는 lit/unlit 최종 컬러용이다. Depth/Normal 같은 디버그 버퍼에는
	// 경계 보정이 오히려 값을 흐리게 만들기 때문에 적용하지 않는다.
	if (!bDebugViewMode && Frame.RenderOptions.ShowFlags.bFXAA)
	{
		FShader* FXAAShader = FShaderManager::Get().GetOrCreate(EShaderPath::FXAA);
		if (FXAAShader)
		{
			FViewportRenderOptions Opts = Frame.RenderOptions;
			FFXAAConstants FXAAData = {};
			FXAAData.EdgeThreshold = Opts.EdgeThreshold;
			FXAAData.EdgeThresholdMin = Opts.EdgeThresholdMin;
			FXAACB.Update(Ctx, &FXAAData, sizeof(FFXAAConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(FXAAShader, ERenderPass::FXAA,
				PassRenderStateTable->ToDrawCommandState(ERenderPass::FXAA, ViewMode));
			Cmd.Bindings.PerShaderCB[0] = &FXAACB;
			Cmd.BuildSortKey(0);
		}
	}
	// Debug view color는 이미 화면 표시용 값이므로 감마 보정을 다시 태우지 않는다.
	if (!bDebugViewMode && Frame.RenderOptions.ShowFlags.bGammaCorrection)
	{
		FShader* GammaShader = FShaderManager::Get().GetOrCreate(EShaderPath::GammaCorrection);
		if (GammaShader)
		{
			const FViewportRenderOptions Opts = Frame.RenderOptions;
			FGammaCorrectionConstants GammaData = {};
			GammaData.DisplayGamma = (std::max)(Opts.DisplayGamma, 0.001f);
			GammaData.BlendWeight = (std::clamp)(Opts.GammaCorrectionBlend, 0.0f, 1.0f);
			GammaData.bUseSRGBCurve = Opts.bUseSRGBCurve ? 1u : 0u;
			GammaCorrectionCB.Update(Ctx, &GammaData, sizeof(FGammaCorrectionConstants));

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.InitFullscreenTriangle(
				GammaShader,
				ERenderPass::GammaCorrection,
				PassRenderStateTable->ToDrawCommandState(ERenderPass::GammaCorrection, ViewMode));
			Cmd.Bindings.PerShaderCB[0] = &GammaCorrectionCB;
			Cmd.BuildSortKey(0);
		}
	}

}

void FDrawCommandBuilder::BuildUICommands(EViewMode ViewMode)
{
	if (!ScreenQuads.HasAnyQuads() || !ScreenQuads.UploadBuffers(CachedContext))
	{
		return;
	}

	FShader* UIShader = FShaderManager::Get().GetOrCreate(EShaderPath::Image);
	if (!UIShader)
	{
		return;
	}

	const FDrawCommandRenderState UIRS = PassRenderStateTable->ToDrawCommandState(ERenderPass::UI, ViewMode);
	for (const FScreenQuadGeometry::FBatch& Batch : ScreenQuads.GetBatches())
	{
		if (Batch.IndexCount == 0)
		{
			continue;
		}

		if (!Batch.SRV && !Batch.bSolidColorOnly)
		{
			continue;
		}

		FDrawCommand& Cmd = DrawCommandList.AddCommand();
		Cmd.Pass = ERenderPass::UI;
		Cmd.Shader = UIShader;
		Cmd.RenderState = UIRS;
		Cmd.Buffer = { ScreenQuads.GetVBBuffer(), ScreenQuads.GetVBStride(), ScreenQuads.GetIBBuffer() };
		Cmd.Buffer.FirstIndex = Batch.FirstIndex;
		Cmd.Buffer.IndexCount = Batch.IndexCount;
		Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = Batch.SRV;
		Cmd.SortKey = FDrawCommand::ComputeUISortKey(Cmd.Pass, Batch.ZOrder, Cmd.Shader, Batch.SRV);
	}
}

// ============================================================
// BuildFontCommands — World text (WorldText) + Screen text (ScreenText)
// ============================================================
void FDrawCommandBuilder::BuildFontCommands(EViewMode ViewMode)
{
	ID3D11DeviceContext* Ctx = CachedContext;
	const FDrawCommandRenderState WorldTextRS = PassRenderStateTable->ToDrawCommandState(ERenderPass::WorldText, ViewMode);
	const FDrawCommandRenderState ScreenTextRS = PassRenderStateTable->ToDrawCommandState(ERenderPass::ScreenText, ViewMode);
	FShader* WorldTextShader = FShaderManager::Get().GetOrCreate(EShaderPath::Font);
	FShader* ScreenTextShader = FShaderManager::Get().GetOrCreate(EShaderPath::ScreenText);

	if (FontGeometry.GetWorldQuadCount() > 0 && FontGeometry.UploadWorldBuffers(Ctx) && WorldTextShader)
	{
		for (const FFontGeometry::FTextBatch& Batch : FontGeometry.GetWorldBatches())
		{
			if (!Batch.Font || !Batch.Font->IsLoaded() || !Batch.Font->SRV || Batch.IndexCount == 0)
			{
				continue;
			}

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.Pass = ERenderPass::WorldText;
			Cmd.Shader = WorldTextShader;
			Cmd.RenderState = WorldTextRS;
			Cmd.Buffer = { FontGeometry.GetWorldVBBuffer(), FontGeometry.GetWorldVBStride(), FontGeometry.GetWorldIBBuffer() };
			Cmd.Buffer.FirstIndex = Batch.FirstIndex;
			Cmd.Buffer.IndexCount = Batch.IndexCount;
			Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = Batch.Font->SRV;
			Cmd.BuildSortKey();
		}
	}

	if (FontGeometry.GetScreenQuadCount() > 0 && FontGeometry.UploadScreenBuffers(Ctx) && ScreenTextShader)
	{
		for (const FFontGeometry::FTextBatch& Batch : FontGeometry.GetScreenBatches())
		{
			if (!Batch.Font || !Batch.Font->IsLoaded() || !Batch.Font->SRV || Batch.IndexCount == 0)
			{
				continue;
			}

			FDrawCommand& Cmd = DrawCommandList.AddCommand();
			Cmd.Pass = ERenderPass::ScreenText;
			Cmd.Shader = ScreenTextShader;
			Cmd.RenderState = ScreenTextRS;
			Cmd.Buffer = { FontGeometry.GetScreenVBBuffer(), FontGeometry.GetScreenVBStride(), FontGeometry.GetScreenIBBuffer() };
			Cmd.Buffer.FirstIndex = Batch.FirstIndex;
			Cmd.Buffer.IndexCount = Batch.IndexCount;
			Cmd.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse] = Batch.Font->SRV;
			Cmd.BuildSortKey();
		}
	}
}

// ============================================================
// PerObjectCB 풀 관리
// ============================================================
void FDrawCommandBuilder::EnsurePerObjectCBPoolCapacity(uint32 RequiredCount)
{
	if (PerObjectCBPool.size() >= RequiredCount)
	{
		return;
	}

	const size_t OldCount = PerObjectCBPool.size();
	PerObjectCBPool.resize(RequiredCount);

	for (size_t Index = OldCount; Index < PerObjectCBPool.size(); ++Index)
	{
		PerObjectCBPool[Index].Create(CachedDevice, sizeof(FPerObjectConstants));
	}
}

FConstantBuffer* FDrawCommandBuilder::GetPerObjectCBForProxy(const FPrimitiveSceneProxy& Proxy)
{
	if (Proxy.GetProxyId() == UINT32_MAX)
	{
		return nullptr;
	}

	EnsurePerObjectCBPoolCapacity(Proxy.GetProxyId() + 1);
	return &PerObjectCBPool[Proxy.GetProxyId()];
}

