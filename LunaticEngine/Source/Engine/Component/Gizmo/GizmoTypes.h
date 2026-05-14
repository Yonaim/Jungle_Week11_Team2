#pragma once

#include "Core/CoreTypes.h"

// 렌더/피킹용 기즈모 컴포넌트와 에디터 기즈모 매니저가 공유하는 공용 타입.
enum class EGizmoMode : uint8
{
    Select = 0,
    Translate,
    Rotate,
    Scale,
    End,
};

enum class EGizmoSpace : uint8
{
    World = 0,
    Local,
};
