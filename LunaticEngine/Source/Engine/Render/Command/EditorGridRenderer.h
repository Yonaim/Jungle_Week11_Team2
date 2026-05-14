#pragma once

#include "Math/Vector.h"
#include "Render/Types/ViewTypes.h"

struct FFrameContext;
class FScene;

namespace EditorGridRenderer
{
	// EditorGrid PS가 참조하는 CPU 측 파라미터 묶음.
	// b2(GridBuffer) 레이아웃과 1:1 대응하므로 멤버 순서를 바꾸면 안 된다.
	struct FGridShaderConstants
	{
		float GridSize = 1.0f;
		float Range = 100.0f;
		float LineThickness = 1.0f;
		float MajorLineInterval = 10.0f;
		float MajorLineThickness = 1.25f;
		float MinorIntensity = 0.45f;
		float MajorIntensity = 0.9f;
		float MaxDistance = 125.0f;
		float AxisThickness = 1.5f;
		float AxisLength = 100.0f;
		float AxisIntensity = 1.0f;
		float DrawAxisPass = 0.0f;
		FVector4 GridCenter = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		FVector4 GridAxisA = FVector4(1.0f, 0.0f, 0.0f, 0.0f);
		FVector4 GridAxisB = FVector4(0.0f, 1.0f, 0.0f, 0.0f);
		FVector4 AxisColorA = FVector4(1.0f, 0.2f, 0.2f, 1.0f);
		FVector4 AxisColorB = FVector4(0.2f, 1.0f, 0.2f, 1.0f);
		FVector4 AxisColorN = FVector4(0.2f, 0.2f, 1.0f, 1.0f);
	};

	static_assert(sizeof(FGridShaderConstants) % 16 == 0, "FGridShaderConstants must be 16-byte aligned");

	// DrawCommandBuilder에서 그리드/축 패스별 상수 버퍼를 생성할 때 사용한다.
	uint32 GetConstantBufferSize();
	// UI/ini 값을 셰이더 안전 범위로 정규화한다.
	FGridRenderSettings SanitizeSettings(FGridRenderSettings Settings);
	// 현재 카메라/씬 상태로 Grid.hlsl 입력 상수를 구성한다.
	// bDrawGrid/bDrawAxis/bDrawAxisPass 조합으로 같은 셰이더를 그리드/축 렌더에 재사용한다.
	void BuildShaderConstants(
		const FFrameContext& Frame,
		const FScene& Scene,
		const FGridRenderSettings& Settings,
		bool bDrawGrid,
		bool bDrawAxis,
		bool bDrawAxisPass,
		FGridShaderConstants& OutConstants);
}
