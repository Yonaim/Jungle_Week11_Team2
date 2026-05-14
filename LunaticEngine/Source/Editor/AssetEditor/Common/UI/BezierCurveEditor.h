#pragma once

#include "ImGui/imgui.h"

namespace ImGui
{
	// ocornut/imgui 이슈 #786을 바탕으로 가져온 3차 베지어 곡선 위젯.
	// P[0..3]에는 P1.x, P1.y, P2.x, P2.y를 저장한다. P0=(0,0), P3=(1,1)이다.
	int Bezier(const char* Label, float P[4]);
	float BezierValue(float Dt01, const float P[4]);
}
