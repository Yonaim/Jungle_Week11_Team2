#pragma once
#include "Render/Pipeline/IRenderPipeline.h"
#include "Render/Pipeline/RenderCollector.h"
#include "Render/Types/FrameContext.h"
#include "Camera/MinimalViewInfo.h"
#include "Render/Culling/GPUOcclusionCulling.h"
#include <memory>

class UEditorEngine;
class FViewport;
class UCameraComponent;
class FEditorViewportCamera;
class FLevelEditorViewportClient;
class FEditorViewportClient;
struct FEditorViewportRenderRequest;

class FEditorRenderPipeline : public IRenderPipeline
{
public:
	FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer);
	~FEditorRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;
	void OnSceneCleared() override;

private:
	// 단일 뷰포트 렌더 오케스트레이션
	void RenderViewportRequest(const FEditorViewportRenderRequest& Request, FRenderer& Renderer);

	// RenderViewport 내부 단계
	void PrepareViewport(FViewport* VP, ID3D11DeviceContext* Ctx);
	void BuildFrame(FLevelEditorViewportClient* VC, const FMinimalViewInfo& ViewInfo, UCameraComponent* LightReferenceCamera, FViewport* VP, UWorld* World);
	void BuildFrameFromRequest(const FEditorViewportRenderRequest& Request);
	void CollectCommands(FLevelEditorViewportClient* VC, UWorld* World, FRenderer& Renderer, FCollectOutput& Output);
	void CollectSceneCommands(const FEditorViewportRenderRequest& Request, FRenderer& Renderer, FCollectOutput& Output);

	// 뷰포트별 GPUOcclusion 인스턴스 (lazy init)
	FGPUOcclusionCulling& GetOcclusionForViewport(FLevelEditorViewportClient* VC);

private:
	UEditorEngine* Editor = nullptr;
	ID3D11Device* CachedDevice = nullptr;
	FRenderCollector Collector;
	FFrameContext Frame;
	TMap<FLevelEditorViewportClient*, std::unique_ptr<FGPUOcclusionCulling>> GPUOcclusionMap;
};
