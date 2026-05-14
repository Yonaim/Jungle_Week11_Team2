#pragma once

#include "Math/Vector.h"

#include <algorithm>

struct FEditorViewportScaleRule
{
    // Viewport height 대비 목표 화면 비율.
    float TargetScreenRatio = 0.1f;

    // 화면상 최소/최대 크기(px). 0 이하이면 해당 clamp를 비활성화한다.
    float MinScreenPixels = 0.0f;
    float MaxScreenPixels = 0.0f;

    constexpr FEditorViewportScaleRule() = default;
    constexpr FEditorViewportScaleRule(float InTargetScreenRatio, float InMinScreenPixels, float InMaxScreenPixels)
        : TargetScreenRatio(InTargetScreenRatio)
        , MinScreenPixels(InMinScreenPixels)
        , MaxScreenPixels(InMaxScreenPixels)
    {
    }
};

namespace FEditorViewportScale
{
    inline float ClampScreenRatio(float ViewportHeight, const FEditorViewportScaleRule& Rule)
    {
        float Ratio = (std::max)(0.0001f, Rule.TargetScreenRatio);
        if (ViewportHeight > 1.0f)
        {
            float TargetPixels = ViewportHeight * Ratio;
            if (Rule.MinScreenPixels > 0.0f)
            {
                TargetPixels = (std::max)(TargetPixels, Rule.MinScreenPixels);
            }
            if (Rule.MaxScreenPixels > 0.0f)
            {
                TargetPixels = (std::min)(TargetPixels, Rule.MaxScreenPixels);
            }
            Ratio = TargetPixels / ViewportHeight;
        }
        return Ratio;
    }

    inline float ComputeWorldScale(
        const FVector& CameraLocation,
        const FVector& TargetLocation,
        bool bIsOrtho,
        float OrthoWidth,
        float ViewportHeight,
        const FEditorViewportScaleRule& Rule)
    {
        const float ScreenRatio = ClampScreenRatio(ViewportHeight, Rule);
        float WorldScale = bIsOrtho
            ? OrthoWidth * ScreenRatio
            : FVector::Distance(CameraLocation, TargetLocation) * ScreenRatio;

        return (std::max)(0.01f, WorldScale);
    }
}

namespace FEditorViewportScaleRules
{
    // Actor helper billboard는 멀리서도 너무 커지지 않도록 56px 상한을 둔다.
    inline constexpr FEditorViewportScaleRule Billboard{ 0.085f, 40.0f, 56.0f };

    // Transform gizmo는 레벨/에셋 에디터 뷰포트에서 동일한 화면 크기를 사용한다.
    inline constexpr FEditorViewportScaleRule Gizmo{ 0.085f, 48.0f, 96.0f };
}
