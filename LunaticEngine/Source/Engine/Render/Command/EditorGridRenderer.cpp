#include "PCH/LunaticPCH.h"
#include "EditorGridRenderer.h"

#include "Render/Scene/FScene.h"
#include "Render/Types/FrameContext.h"

#include <algorithm>
#include <cmath>

namespace
{
	// 너무 작은 Spacing은 frac/fwidth 기반 라인 계산에서 수치 불안정을 키우므로 하한을 둔다.
	constexpr float kMinGridSpacing = 0.0001f;
	constexpr float kFrustumPlaneEpsilon = 1.0e-3f;
	constexpr float kRangeMarginCells = 4.0f;
	constexpr float kRangeMarginRatio = 0.05f;
	constexpr float kMinFadeDistanceCells = 16.0f;
	constexpr float kAxisLengthMinCells = 10.0f;

	struct FGridPlaneDesc
	{
		// A0/A1: 그리드 평면 축, N: 평면 법선 축 인덱스.
		int32 A0 = 0;
		int32 A1 = 1;
		int32 N = 2;
		FVector Axis0 = FVector(1.0f, 0.0f, 0.0f);
		FVector Axis1 = FVector(0.0f, 1.0f, 0.0f);
		FVector4 AxisColor0 = FVector4(1.0f, 0.2f, 0.2f, 1.0f);
		FVector4 AxisColor1 = FVector4(0.2f, 1.0f, 0.2f, 1.0f);
	};

	struct FGridDrawParams
	{
		// Grid.hlsl은 GridCenter + AxisA/B + Range로 평면 버텍스를 재구성한다.
		FGridPlaneDesc Plane;
		FVector Center;
		float Spacing = 1.0f;
		float Range = 100.0f;
		float MaxDistance = 125.0f;
		float AxisLength = 100.0f;
	};

	struct FGridPlaneCoverage
	{
		bool bValid = false;
		int32 PointCount = 0;
		float Min0 = 0.0f;
		float Max0 = 0.0f;
		float Min1 = 0.0f;
		float Max1 = 0.0f;
		float MaxViewDistance = 0.0f;
	};

	float GetAxisComponent(const FVector& V, int32 AxisIndex)
	{
		return (&V.X)[AxisIndex];
	}

	void SetAxisComponent(FVector& V, int32 AxisIndex, float Value)
	{
		(&V.X)[AxisIndex] = Value;
	}

	int32 FindDominantAxis(const FVector& V)
	{
		const float AX = std::fabs(V.X);
		const float AY = std::fabs(V.Y);
		const float AZ = std::fabs(V.Z);
		if (AX >= AY && AX >= AZ)
		{
			return 0;
		}
		if (AY >= AX && AY >= AZ)
		{
			return 1;
		}
		return 2;
	}

	FVector MakeUnitAxis(int32 AxisIndex)
	{
		FVector Axis;
		SetAxisComponent(Axis, AxisIndex, 1.0f);
		return Axis;
	}

	FVector4 GetAxisColor(int32 AxisIndex)
	{
		switch (AxisIndex)
		{
		case 0: return FVector4(1.0f, 0.2f, 0.2f, 1.0f);
		case 1: return FVector4(0.2f, 1.0f, 0.2f, 1.0f);
		default: return FVector4(0.2f, 0.2f, 1.0f, 1.0f);
		}
	}

	FVector MakePlanePoint(const FGridPlaneDesc& Plane, float V0, float V1, float VN)
	{
		FVector P;
		SetAxisComponent(P, Plane.A0, V0);
		SetAxisComponent(P, Plane.A1, V1);
		SetAxisComponent(P, Plane.N, VN);
		return P;
	}

	float SnapToGrid(float Value, float GridSpacing)
	{
		return std::round(Value / GridSpacing) * GridSpacing;
	}

	bool IsFiniteVector(const FVector& V)
	{
		return std::isfinite(V.X) && std::isfinite(V.Y) && std::isfinite(V.Z);
	}

	FGridPlaneDesc BuildGridPlaneDesc(bool bFixedOrtho, const FVector& CameraForward)
	{
		const int32 N = bFixedOrtho ? FindDominantAxis(CameraForward) : 2;
		const int32 A0 = (N == 0) ? 1 : 0;
		const int32 A1 = (N == 2) ? 1 : 2;

		FGridPlaneDesc Desc;
		Desc.A0 = A0;
		Desc.A1 = A1;
		Desc.N = N;
		Desc.Axis0 = MakeUnitAxis(A0);
		Desc.Axis1 = MakeUnitAxis(A1);
		Desc.AxisColor0 = GetAxisColor(A0);
		Desc.AxisColor1 = GetAxisColor(A1);
		return Desc;
	}

	void AddCoveragePoint(
		FGridPlaneCoverage& Coverage,
		const FVector& Point,
		const FGridPlaneDesc& Plane,
		const FVector& CameraPosition)
	{
		if (!IsFiniteVector(Point))
		{
			return;
		}

		const float P0 = GetAxisComponent(Point, Plane.A0);
		const float P1 = GetAxisComponent(Point, Plane.A1);

		if (!Coverage.bValid)
		{
			Coverage.bValid = true;
			Coverage.Min0 = Coverage.Max0 = P0;
			Coverage.Min1 = Coverage.Max1 = P1;
		}
		else
		{
			Coverage.Min0 = (std::min)(Coverage.Min0, P0);
			Coverage.Max0 = (std::max)(Coverage.Max0, P0);
			Coverage.Min1 = (std::min)(Coverage.Min1, P1);
			Coverage.Max1 = (std::max)(Coverage.Max1, P1);
		}

		Coverage.MaxViewDistance = (std::max)(Coverage.MaxViewDistance, FVector::Distance(CameraPosition, Point));
		++Coverage.PointCount;
	}

	bool TryBuildFrustumCoverage(
		const FFrameContext& Frame,
		const FGridPlaneDesc& Plane,
		FGridPlaneCoverage& OutCoverage)
	{
		// NDC 코너를 역변환해 뷰 프러스텀과 그리드 평면의 교차 범위를 추정한다.
		// 이 범위로 Range를 잡아, 카메라 주변에서 항상 충분한 그리드 면적을 확보한다.
		const FMatrix InvViewProj = (Frame.View * Frame.Proj).GetInverse();
		static const FVector NDCCorners[8] = {
			FVector(-1.0f, -1.0f, 1.0f), FVector(1.0f, -1.0f, 1.0f),
			FVector(1.0f, 1.0f, 1.0f), FVector(-1.0f, 1.0f, 1.0f),
			FVector(-1.0f, -1.0f, 0.0f), FVector(1.0f, -1.0f, 0.0f),
			FVector(1.0f, 1.0f, 0.0f), FVector(-1.0f, 1.0f, 0.0f),
		};
		static const int32 FrustumEdges[12][2] = {
			{0, 1}, {1, 2}, {2, 3}, {3, 0},
			{4, 5}, {5, 6}, {6, 7}, {7, 4},
			{0, 4}, {1, 5}, {2, 6}, {3, 7},
		};

		FVector WorldCorners[8];
		for (int32 Index = 0; Index < 8; ++Index)
		{
			WorldCorners[Index] = InvViewProj.TransformPositionWithW(NDCCorners[Index]);
		}

		for (const auto& Edge : FrustumEdges)
		{
			const FVector& P0 = WorldCorners[Edge[0]];
			const FVector& P1 = WorldCorners[Edge[1]];
			const float D0 = GetAxisComponent(P0, Plane.N);
			const float D1 = GetAxisComponent(P1, Plane.N);

			if (std::fabs(D0) <= kFrustumPlaneEpsilon)
			{
				AddCoveragePoint(OutCoverage, P0, Plane, Frame.CameraPosition);
			}
			if (std::fabs(D1) <= kFrustumPlaneEpsilon)
			{
				AddCoveragePoint(OutCoverage, P1, Plane, Frame.CameraPosition);
			}

			const bool bCrossPlane =
				(D0 < -kFrustumPlaneEpsilon && D1 > kFrustumPlaneEpsilon) ||
				(D0 > kFrustumPlaneEpsilon && D1 < -kFrustumPlaneEpsilon);

			if (bCrossPlane)
			{
				const float T = D0 / (D0 - D1);
				const FVector Intersection = P0 + (P1 - P0) * T;
				AddCoveragePoint(OutCoverage, Intersection, Plane, Frame.CameraPosition);
			}
		}

		return OutCoverage.bValid && OutCoverage.PointCount >= 2;
	}

	FGridDrawParams BuildGridDrawParams(const FFrameContext& Frame, const FScene& Scene)
	{
		FGridDrawParams Params;
		Params.Spacing = (std::max)(Scene.GetGridSpacing(), kMinGridSpacing);

		FVector CameraForward = Frame.CameraRight.Cross(Frame.CameraUp);
		CameraForward.Normalize();
		// Perspective는 월드 Z-up 기준(XY plane)으로 고정, 고정 Ortho는 카메라 정면축에 맞춰 평면을 선택한다.
		Params.Plane = BuildGridPlaneDesc(Frame.IsFixedOrtho(), CameraForward);

		const int32 BaseHalfCount = (std::max)(Scene.GetGridHalfLineCount(), 1);
		FGridPlaneCoverage Coverage;
		if (TryBuildFrustumCoverage(Frame, Params.Plane, Coverage))
		{
			// 프러스텀 교차 기반 범위: 화면에 보이는 영역 중심으로 스냅해 그리드 "점프"를 줄인다.
			const float Span0 = (std::max)(Coverage.Max0 - Coverage.Min0, 0.0f);
			const float Span1 = (std::max)(Coverage.Max1 - Coverage.Min1, 0.0f);
			const float HalfExtent = (std::max)(Span0, Span1) * 0.5f;
			const float Margin = (std::max)(Params.Spacing * kRangeMarginCells, HalfExtent * kRangeMarginRatio);
			const int32 HalfCount = (std::max)(
				BaseHalfCount,
				static_cast<int32>(std::ceil((HalfExtent + Margin) / Params.Spacing)));

			const float Center0 = SnapToGrid((Coverage.Min0 + Coverage.Max0) * 0.5f, Params.Spacing);
			const float Center1 = SnapToGrid((Coverage.Min1 + Coverage.Max1) * 0.5f, Params.Spacing);
			Params.Center = MakePlanePoint(Params.Plane, Center0, Center1, 0.0f);
			Params.Range = Params.Spacing * static_cast<float>(HalfCount);
			Params.MaxDistance = (std::max)(
				(std::max)(Params.Range * 1.25f, Coverage.MaxViewDistance + Params.Spacing * 8.0f),
				Params.Spacing * kMinFadeDistanceCells);
			Params.AxisLength = (std::max)(Params.Range, Params.Spacing * kAxisLengthMinCells);
			return Params;
		}

		// 교차 계산 실패 시(수치 경계 상황) 카메라 위치 기반 보수 경로로 안전하게 폴백한다.
		const FVector FocusPoint = Frame.CameraPosition;
		const float Center0 = SnapToGrid(GetAxisComponent(FocusPoint, Params.Plane.A0), Params.Spacing);
		const float Center1 = SnapToGrid(GetAxisComponent(FocusPoint, Params.Plane.A1), Params.Spacing);
		const float HeightDrivenExtent = std::fabs(GetAxisComponent(Frame.CameraPosition, Params.Plane.N)) * 2.0f
			+ Params.Spacing * kRangeMarginCells;
		const int32 HalfCount = (std::max)(
			BaseHalfCount,
			static_cast<int32>(std::ceil(HeightDrivenExtent / Params.Spacing)));

		Params.Center = MakePlanePoint(Params.Plane, Center0, Center1, 0.0f);
		Params.Range = Params.Spacing * static_cast<float>(HalfCount);
		Params.MaxDistance = (std::max)(Params.Range * 1.25f, Params.Spacing * kMinFadeDistanceCells);
		Params.AxisLength = (std::max)(Params.Range, Params.Spacing * kAxisLengthMinCells);
		return Params;
	}
}

uint32 EditorGridRenderer::GetConstantBufferSize()
{
	return sizeof(FGridShaderConstants);
}

FGridRenderSettings EditorGridRenderer::SanitizeSettings(FGridRenderSettings Settings)
{
	Settings.LineThickness = std::clamp(Settings.LineThickness, 0.0f, 8.0f);
	Settings.MajorLineThickness = std::clamp(Settings.MajorLineThickness, 0.0f, 12.0f);
	Settings.MajorLineInterval = std::clamp<int32>(Settings.MajorLineInterval, 1, 100);
	Settings.MinorIntensity = std::clamp(Settings.MinorIntensity, 0.0f, 2.0f);
	Settings.MajorIntensity = std::clamp(Settings.MajorIntensity, 0.0f, 2.0f);
	Settings.AxisThickness = std::clamp(Settings.AxisThickness, 0.0f, 12.0f);
	Settings.AxisIntensity = std::clamp(Settings.AxisIntensity, 0.0f, 2.0f);
	return Settings;
}

void EditorGridRenderer::BuildShaderConstants(
	const FFrameContext& Frame,
	const FScene& Scene,
	const FGridRenderSettings& Settings,
	bool bDrawGrid,
	bool bDrawAxis,
	bool bDrawAxisPass,
	FGridShaderConstants& OutConstants)
{
	const FGridDrawParams Params = BuildGridDrawParams(Frame, Scene);

	// Grid pass와 Axis pass가 같은 cbuffer를 공유하므로, 사용하지 않는 항목은 0으로 명시한다.
	OutConstants.GridSize = Params.Spacing;
	OutConstants.Range = Params.Range;
	OutConstants.LineThickness = Settings.LineThickness;
	OutConstants.MajorLineInterval = static_cast<float>(Settings.MajorLineInterval);
	OutConstants.MajorLineThickness = Settings.MajorLineThickness;
	OutConstants.MinorIntensity = bDrawGrid ? Settings.MinorIntensity : 0.0f;
	OutConstants.MajorIntensity = bDrawGrid ? Settings.MajorIntensity : 0.0f;
	OutConstants.MaxDistance = Params.MaxDistance;
	OutConstants.AxisThickness = bDrawAxis ? Settings.AxisThickness : 0.0f;
	OutConstants.AxisLength = Params.AxisLength;
	OutConstants.AxisIntensity = bDrawAxis ? Settings.AxisIntensity : 0.0f;
	OutConstants.DrawAxisPass = bDrawAxisPass ? 1.0f : 0.0f;
	OutConstants.GridCenter = FVector4(Params.Center.X, Params.Center.Y, Params.Center.Z, 0.0f);
	OutConstants.GridAxisA = FVector4(Params.Plane.Axis0.X, Params.Plane.Axis0.Y, Params.Plane.Axis0.Z, 0.0f);
	OutConstants.GridAxisB = FVector4(Params.Plane.Axis1.X, Params.Plane.Axis1.Y, Params.Plane.Axis1.Z, 0.0f);
	OutConstants.AxisColorA = Params.Plane.AxisColor0;
	OutConstants.AxisColorB = Params.Plane.AxisColor1;
	OutConstants.AxisColorN = GetAxisColor(2);
}
