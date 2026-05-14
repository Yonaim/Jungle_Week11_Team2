#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

struct FDebugDrawItem
{
	FVector Start;
	FVector End;
	FColor Color;
	float RemainingTime = 0.0f;
	bool bOneFrame = false;
	bool bDepthTest = true;
};

class FDebugDrawQueue
{
public:
	void AddLine(const FVector& Start, const FVector& End,
		const FColor& Color, float Duration, bool bDepthTest = true);

	void AddBox(const FVector& Center, const FVector& Extent,
		const FColor& Color, float Duration, bool bDepthTest = true);

	void AddSphere(const FVector& Center, float Radius, int32 Segments,
		const FColor& Color, float Duration, bool bDepthTest = true);

	void Tick(float DeltaTime);

	const TArray<FDebugDrawItem>& GetItems() const { return Items; }

	void Clear() { Items.clear(); }

private:
	TArray<FDebugDrawItem> Items;
};
