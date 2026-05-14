#pragma once

#include "Component/CameraComponent.h"
#include "Core/EngineTypes.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/FName.h"

// PIE 세션 실행 위치.
// 현재 구현은 에디터 프로세스 내부에서 실행하는 InProcess만 사용한다.
enum class EPIESessionDestination : uint8
{
    InProcess,
};

// PIE 시작 모드.
// Simulate는 에디터 카메라를 유지하고, PlayInViewport는 게임 카메라로 전환한다.
enum class EPIEPlayMode : uint8
{
    PlayInViewport,
    Simulate,
};

// PIE 중 입력 제어 모드.
enum class EPIEControlMode : uint8
{
    Possessed,
    Ejected
};

// Play 요청 시 넘기는 파라미터.
struct FRequestPlaySessionParams
{
    EPIESessionDestination SessionDestination = EPIESessionDestination::InProcess;
    EPIEPlayMode PlayMode = EPIEPlayMode::PlayInViewport;
};

struct FPIEViewportCameraSnapshot
{
    // PIE 시작 직전 활성 에디터 뷰포트 카메라 상태 백업.
    // 종료 시 원래 편집 카메라 위치/회전을 복원할 때 사용한다.
    FVector Location;
    FRotator Rotation;
    FMinimalViewInfo CameraState;
    bool bValid = false;
};

// 활성 PIE 세션 상태.
struct FPlayInEditorSessionInfo
{
    FRequestPlaySessionParams OriginalRequestParams;
    double PIEStartTime = 0.0;
    // PIE 시작 직전 활성 월드 핸들. 종료 시 에디터 월드 복귀에 사용한다.
    FName PreviousActiveWorldHandle;
    FPIEViewportCameraSnapshot SavedViewportCamera;
};
