#include "PCH/LunaticPCH.h"
#include "Debug/DrawDebugHelpers.h"

#if DEBUG_DRAW_ENABLED

#include "GameFramework/World.h"
#include "Render/Scene/FScene.h"

#include <algorithm>
#include <cmath>

namespace
{
	FDebugDrawQueue* GetDebugDrawQueue(UWorld* World)
	{
		return World ? &World->GetScene().GetDebugDrawQueue() : nullptr;
	}

	FDebugDrawQueue* GetDebugDrawQueue(FScene* Scene)
	{
		return Scene ? &Scene->GetDebugDrawQueue() : nullptr;
	}

	bool IsFiniteVector(const FVector& Value)
	{
		return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
	}

	void AddDebugBoneConnection(FDebugDrawQueue* Queue,
		const FVector& ParentPosition,
		const FVector& ChildPosition,
		float BaseRadius,
		const FColor& Color,
		float Duration,
		bool bDepthTest)
	{
		if (!Queue || !IsFiniteVector(ParentPosition) || !IsFiniteVector(ChildPosition))
		{
			return;
		}

		const FVector BoneVector = ChildPosition - ParentPosition;
		const float BoneLength = BoneVector.Length();
		if (!std::isfinite(BoneLength) || BoneLength <= 1.0e-4f)
		{
			return;
		}

		FVector Direction = BoneVector / BoneLength;
		if (!IsFiniteVector(Direction) || Direction.IsNearlyZero())
		{
			return;
		}
		Direction.Normalize();

		FVector ReferenceAxis = (std::fabs(Direction.Z) < 0.85f) ? FVector::UpVector : FVector::RightVector;
		FVector Right = Direction.Cross(ReferenceAxis);
		if (Right.IsNearlyZero())
		{
			ReferenceAxis = FVector::ForwardVector;
			Right = Direction.Cross(ReferenceAxis);
			if (Right.IsNearlyZero())
			{
				return;
			}
		}
		Right.Normalize();

		FVector Up = Right.Cross(Direction);
		if (Up.IsNearlyZero())
		{
			return;
		}
		Up.Normalize();

		const float MinRadius = (std::max)(0.01f, BoneLength * 0.03f);
		const float MaxRadius = (std::max)(MinRadius, BoneLength * 0.18f);
		const float ResolvedRadius = BaseRadius > 0.0f
			? (std::min)((std::max)(BaseRadius, MinRadius), MaxRadius)
			: (std::min)((std::max)(BoneLength * 0.08f, MinRadius), MaxRadius);
		const float BaseOffset = (std::min)((std::max)(BoneLength * 0.15f, ResolvedRadius * 0.5f), BoneLength * 0.35f);
		const FVector BaseCenter = ParentPosition + Direction * BaseOffset;

		const FVector Corners[4] = {
			BaseCenter + Right * ResolvedRadius + Up * ResolvedRadius,
			BaseCenter - Right * ResolvedRadius + Up * ResolvedRadius,
			BaseCenter - Right * ResolvedRadius - Up * ResolvedRadius,
			BaseCenter + Right * ResolvedRadius - Up * ResolvedRadius,
		};

		Queue->AddLine(ParentPosition, BaseCenter, Color, Duration, bDepthTest);
		Queue->AddLine(Corners[0], Corners[1], Color, Duration, bDepthTest);
		Queue->AddLine(Corners[1], Corners[2], Color, Duration, bDepthTest);
		Queue->AddLine(Corners[2], Corners[3], Color, Duration, bDepthTest);
		Queue->AddLine(Corners[3], Corners[0], Color, Duration, bDepthTest);

		for (const FVector& Corner : Corners)
		{
			Queue->AddLine(Corner, ChildPosition, Color, Duration, bDepthTest);
		}
	}
}

void DrawDebugLine(UWorld* World,
	const FVector& Start, const FVector& End,
	const FColor& Color, float Duration, bool bDepthTest)
{
	if (FDebugDrawQueue* Queue = GetDebugDrawQueue(World))
	{
		Queue->AddLine(Start, End, Color, Duration, bDepthTest);
	}
}

void DrawDebugLine(FScene* Scene,
	const FVector& Start, const FVector& End,
	const FColor& Color, float Duration, bool bDepthTest)
{
	if (FDebugDrawQueue* Queue = GetDebugDrawQueue(Scene))
	{
		Queue->AddLine(Start, End, Color, Duration, bDepthTest);
	}
}

void DrawDebugBox(UWorld* World,
	const FVector& Center, const FVector& Extent,
	const FColor& Color, float Duration)
{
	if (!World) return;
	World->GetScene().GetDebugDrawQueue().AddBox(Center, Extent, Color, Duration);
}

void DrawDebugBox(UWorld* World,
	const FVector& P0, const FVector& P1,
	const FVector& P2, const FVector& P3,
	const FColor& Color, float Duration)
{
	if (!World) return;
	FDebugDrawQueue& Queue = World->GetScene().GetDebugDrawQueue();
	Queue.AddLine(P0, P1, Color, Duration);
	Queue.AddLine(P1, P2, Color, Duration);
	Queue.AddLine(P2, P3, Color, Duration);
	Queue.AddLine(P3, P0, Color, Duration);
}

void DrawDebugBox(UWorld* World,
	const FVector& P0, const FVector& P1,
	const FVector& P2, const FVector& P3,
	const FVector& P4, const FVector& P5,
	const FVector& P6, const FVector& P7,
	const FColor& Color, float Duration)
{
	if (!World) return;
	FDebugDrawQueue& Queue = World->GetScene().GetDebugDrawQueue();
	Queue.AddLine(P0, P1, Color, Duration);
	Queue.AddLine(P1, P2, Color, Duration);
	Queue.AddLine(P2, P3, Color, Duration);
	Queue.AddLine(P3, P0, Color, Duration);
	Queue.AddLine(P4, P5, Color, Duration);
	Queue.AddLine(P5, P6, Color, Duration);
	Queue.AddLine(P6, P7, Color, Duration);
	Queue.AddLine(P7, P4, Color, Duration);
	Queue.AddLine(P0, P4, Color, Duration);
	Queue.AddLine(P1, P5, Color, Duration);
	Queue.AddLine(P2, P6, Color, Duration);
	Queue.AddLine(P3, P7, Color, Duration);
}

void DrawDebugSphere(UWorld* World,
	const FVector& Center, float Radius,
	int32 Segments, const FColor& Color, float Duration, bool bDepthTest)
{
	if (FDebugDrawQueue* Queue = GetDebugDrawQueue(World))
	{
		Queue->AddSphere(Center, Radius, Segments, Color, Duration, bDepthTest);
	}
}

void DrawDebugSphere(FScene* Scene,
	const FVector& Center, float Radius,
	int32 Segments, const FColor& Color, float Duration, bool bDepthTest)
{
	if (FDebugDrawQueue* Queue = GetDebugDrawQueue(Scene))
	{
		Queue->AddSphere(Center, Radius, Segments, Color, Duration, bDepthTest);
	}
}

void DrawDebugPoint(UWorld* World,
	const FVector& Position, float Size,
	const FColor& Color, float Duration, bool bDepthTest)
{
	if (!World) return;

	World->GetScene().GetDebugDrawQueue().AddLine(
		Position - FVector(Size, 0, 0), Position + FVector(Size, 0, 0), Color, Duration, bDepthTest);
	World->GetScene().GetDebugDrawQueue().AddLine(
		Position - FVector(0, Size, 0), Position + FVector(0, Size, 0), Color, Duration, bDepthTest);
	World->GetScene().GetDebugDrawQueue().AddLine(
		Position - FVector(0, 0, Size), Position + FVector(0, 0, Size), Color, Duration, bDepthTest);
}

void DrawDebugPoint(FScene* Scene,
	const FVector& Position, float Size,
	const FColor& Color, float Duration, bool bDepthTest)
{
	if (!Scene) return;

	Scene->GetDebugDrawQueue().AddLine(
		Position - FVector(Size, 0, 0), Position + FVector(Size, 0, 0), Color, Duration, bDepthTest);
	Scene->GetDebugDrawQueue().AddLine(
		Position - FVector(0, Size, 0), Position + FVector(0, Size, 0), Color, Duration, bDepthTest);
	Scene->GetDebugDrawQueue().AddLine(
		Position - FVector(0, 0, Size), Position + FVector(0, 0, Size), Color, Duration, bDepthTest);
}

void DrawDebugFrustum(UWorld* World,
	const FMatrix& ViewProj,
	const FColor& Color, float Duration)
{
	if (!World) return;

	FMatrix InvVP = ViewProj.GetInverse();

	static const FVector NDC[8] = {
		FVector(-1, -1, 1), FVector( 1, -1, 1), FVector( 1,  1, 1), FVector(-1,  1, 1),
		FVector(-1, -1, 0), FVector( 1, -1, 0), FVector( 1,  1, 0), FVector(-1,  1, 0),
	};

	FVector W[8];
	for (int i = 0; i < 8; ++i)
	{
		W[i] = InvVP.TransformPositionWithW(NDC[i]);
	}

	DrawDebugBox(World, W[0], W[1], W[2], W[3], W[4], W[5], W[6], W[7], Color, Duration);
}

void DrawDebugBoneConnection(UWorld* World,
	const FVector& ParentPosition,
	const FVector& ChildPosition,
	float BaseRadius,
	const FColor& Color,
	float Duration,
	bool bDepthTest)
{
	AddDebugBoneConnection(GetDebugDrawQueue(World), ParentPosition, ChildPosition, BaseRadius, Color, Duration, bDepthTest);
}

void DrawDebugBoneConnection(FScene* Scene,
	const FVector& ParentPosition,
	const FVector& ChildPosition,
	float BaseRadius,
	const FColor& Color,
	float Duration,
	bool bDepthTest)
{
	AddDebugBoneConnection(GetDebugDrawQueue(Scene), ParentPosition, ChildPosition, BaseRadius, Color, Duration, bDepthTest);
}

#endif // _DEBUG
