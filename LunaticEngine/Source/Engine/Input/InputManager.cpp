#include "PCH/LunaticPCH.h"
#include "Input/InputManager.h"
#include "ImGui/imgui.h"
#include <cstring>
#include <cmath>
#include <algorithm>

FInputManager* FInputManager::Instance = nullptr;
// Function : ProcessMessage handle raw input from windows 
//and store input event to event queue
void FInputManager::ProcessMessage(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	switch (Msg)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		const int32 Key = static_cast<int32>(WParam);
		if (Key >= 0 && Key < MAX_KEYS)
		{
			EventQueue.push_back({ EInputEventType::KeyDown, Key });
		}

		const int32 SpecificKey = GetSpecificModifierKey(WParam, LParam);
		if (SpecificKey != Key && SpecificKey >= 0 && SpecificKey < MAX_KEYS)
		{
			EventQueue.push_back({ EInputEventType::KeyDown, SpecificKey });
		}
		break;
	}

	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		const int32 Key = static_cast<int32>(WParam);
		if (Key >= 0 && Key < MAX_KEYS)
		{
			EventQueue.push_back({ EInputEventType::KeyUp, Key });
		}

		const int32 SpecificKey = GetSpecificModifierKey(WParam, LParam);
		if (SpecificKey != Key && SpecificKey >= 0 && SpecificKey < MAX_KEYS)
		{
			EventQueue.push_back({ EInputEventType::KeyUp, SpecificKey });
		}
		break;
	}

	case WM_SETFOCUS:
		bWindowFocused = true;
		ResetMouseDelta();
		ResetWheelDelta();
		break;

	case WM_KILLFOCUS:
		bWindowFocused = false;
		ResetAllStates();
		if (GetCapture() == Hwnd)
		{
			ReleaseCapture();
		}
		break;

	case WM_ACTIVATEAPP:
		if (WParam == FALSE)
		{
			bWindowFocused = false;
			ResetAllStates();
			if (GetCapture() == Hwnd)
			{
				ReleaseCapture();
			}
		}
		else
		{
			bWindowFocused = true;
			ResetMouseDelta();
			ResetWheelDelta();
		}
		break;

	case WM_CANCELMODE:
		ResetAllStates();
		if (GetCapture() == Hwnd)
		{
			ReleaseCapture();
		}
		break;

	case WM_LBUTTONDOWN:
		EventQueue.push_back({ EInputEventType::MouseButtonDown, MOUSE_LEFT });
		SetCapture(Hwnd);
		break;

	case WM_LBUTTONUP:
		EventQueue.push_back({ EInputEventType::MouseButtonUp, MOUSE_LEFT });
		ReleaseCapture();
		break;

	case WM_RBUTTONDOWN:
		EventQueue.push_back({ EInputEventType::MouseButtonDown, MOUSE_RIGHT });
		GetCursorPos(&LastMousePos);
		bTrackingMouse = true;
		SetCapture(Hwnd);
		break;

	case WM_RBUTTONUP:
		EventQueue.push_back({ EInputEventType::MouseButtonUp, MOUSE_RIGHT });
		bTrackingMouse = false;
		ReleaseCapture();
		break;

	case WM_MBUTTONDOWN:
		EventQueue.push_back({ EInputEventType::MouseButtonDown, MOUSE_MIDDLE });
		SetCapture(Hwnd);
		break;

	case WM_MBUTTONUP:
		EventQueue.push_back({ EInputEventType::MouseButtonUp, MOUSE_MIDDLE });
		ReleaseCapture();
		break;

	case WM_XBUTTONDOWN:
	{
		int32 Btn = (GET_XBUTTON_WPARAM(WParam) == XBUTTON1) ? MOUSE_X1 : MOUSE_X2;
		EventQueue.push_back({ EInputEventType::MouseButtonDown, Btn });
		SetCapture(Hwnd);
		break;
	}
	case WM_XBUTTONUP:
	{
		int32 Btn = (GET_XBUTTON_WPARAM(WParam) == XBUTTON1) ? MOUSE_X1 : MOUSE_X2;
		EventQueue.push_back({ EInputEventType::MouseButtonUp, Btn });
		ReleaseCapture();
		break;
	}

	case WM_MOUSEWHEEL:
		PendingWheelDelta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(WParam)) / static_cast<float>(WHEEL_DELTA);
		break;

	case WM_INPUT:
	{
		// RIDEV_INPUTSINK keeps delivering raw mouse deltas while unfocused.
		// Ignore them so focus regain starts from a clean cursor baseline.
		if (!IsOwnerWindowForeground())
		{
			break;
		}

		UINT Size = 0;
		GetRawInputData((HRAWINPUT)LParam, RID_INPUT, NULL, &Size, sizeof(RAWINPUTHEADER));
		if (Size > 0)
		{
			std::vector<BYTE> Data(Size);
			if (GetRawInputData((HRAWINPUT)LParam, RID_INPUT, Data.data(), &Size, sizeof(RAWINPUTHEADER)) == Size)
			{
				RAWINPUT* Raw = (RAWINPUT*)Data.data();
				if (Raw->header.dwType == RIM_TYPEMOUSE)
				{
					RawMouseDeltaAccumX += static_cast<float>(Raw->data.mouse.lLastX);
					RawMouseDeltaAccumY += static_cast<float>(Raw->data.mouse.lLastY);
				}
			}
		}
		break;
	}
	}
}

void FInputManager::Tick()
{
	// Focus check
	bool bFocused = IsOwnerWindowForeground();
	if (bFocused != bWindowFocused)
	{
		bWindowFocused = bFocused;
		if (!bWindowFocused)
		{
			ResetAllStates();
			return;
		}

		ResetMouseDelta();
		ResetWheelDelta();
	}

	if (!bWindowFocused) return;

	// Save previous frame state
	std::memcpy(PrevKeyState, KeyState, sizeof(KeyState));

	// Clear transient states
	std::memset(PressedState, 0, sizeof(PressedState));
	std::memset(ReleasedState, 0, sizeof(ReleasedState));
	for (int i = 0; i < MAX_KEYS; ++i)
	{
		bWasDragStarted[i] = false;
		bWasDragEnded[i] = false;
	}

	// Flush event queue
	for (const FInputEvent& Event : EventQueue)
	{
		const int32 Key = Event.KeyOrButton;
		if (Key < 0 || Key >= MAX_KEYS) continue;

		switch (Event.Type)
		{
		case EInputEventType::KeyDown:
			ApplyKeyState(Key, true, false);
			break;
		case EInputEventType::MouseButtonDown:
			ApplyKeyState(Key, true, true);
			break;
		case EInputEventType::KeyUp:
			ApplyKeyState(Key, false, false);
			break;
		case EInputEventType::MouseButtonUp:
			ApplyKeyState(Key, false, true);
			break;
		default:
			break;
		}
	}
	EventQueue.clear();

	// If Windows or ImGui swallowed a modifier or mouse-up message, reconcile with the physical state.
	SynchronizeModifierKeyStates();
	SynchronizeMouseButtonStates();

	// Mouse Wheel
	MouseWheelDelta = PendingWheelDelta;
	PendingWheelDelta = 0.0f;

	// Mouse delta
	POINT CurrentPos;
	GetCursorPos(&CurrentPos);
	
	// If we have raw delta, use it. Otherwise use cursor pos.
	if (std::abs(RawMouseDeltaAccumX) > 0.0f || std::abs(RawMouseDeltaAccumY) > 0.0f)
	{
		MouseDeltaX = RawMouseDeltaAccumX;
		MouseDeltaY = RawMouseDeltaAccumY;
	}
	else
	{
		MouseDeltaX = static_cast<float>(CurrentPos.x - LastMousePos.x);
		MouseDeltaY = static_cast<float>(CurrentPos.y - LastMousePos.y);
	}
	
	LastMousePos = CurrentPos;
	RawMouseDeltaAccumX = 0.0f;
	RawMouseDeltaAccumY = 0.0f;

	UpdateDragging();
}

bool FInputManager::IsOwnerWindowForeground() const
{
	return !OwnerHWnd || GetForegroundWindow() == OwnerHWnd;
}

void FInputManager::ApplyKeyState(int32 Key, bool bDown, bool bIsMouseButton)
{
	if (Key < 0 || Key >= MAX_KEYS) return;

	if (bDown)
	{
		if (!KeyState[Key])
		{
			PressedState[Key] = true;
		}
		KeyState[Key] = true;

		if (bIsMouseButton && !bDragCandidate[Key])
		{
			bDragCandidate[Key] = true;
			MouseDownPos[Key] = LastMousePos;
		}
	}
	else
	{
		if (KeyState[Key])
		{
			ReleasedState[Key] = true;
		}
		KeyState[Key] = false;

		if (bIsMouseButton)
		{
			if (bIsDragging[Key])
			{
				bIsDragging[Key] = false;
				bWasDragEnded[Key] = true;
			}
			bDragCandidate[Key] = false;
		}
	}
}

void FInputManager::SynchronizeModifierKeyStates()
{
	const auto IsDown = [](int32 Key) -> bool
	{
		return (GetAsyncKeyState(Key) & 0x8000) != 0;
	};

	const bool bLeftCtrl = IsDown(VK_LCONTROL);
	const bool bRightCtrl = IsDown(VK_RCONTROL);
	ApplyKeyState(VK_LCONTROL, bLeftCtrl, false);
	ApplyKeyState(VK_RCONTROL, bRightCtrl, false);
	ApplyKeyState(VK_CONTROL, bLeftCtrl || bRightCtrl || IsDown(VK_CONTROL), false);

	const bool bLeftAlt = IsDown(VK_LMENU);
	const bool bRightAlt = IsDown(VK_RMENU);
	ApplyKeyState(VK_LMENU, bLeftAlt, false);
	ApplyKeyState(VK_RMENU, bRightAlt, false);
	ApplyKeyState(VK_MENU, bLeftAlt || bRightAlt || IsDown(VK_MENU), false);

	const bool bLeftShift = IsDown(VK_LSHIFT);
	const bool bRightShift = IsDown(VK_RSHIFT);
	ApplyKeyState(VK_LSHIFT, bLeftShift, false);
	ApplyKeyState(VK_RSHIFT, bRightShift, false);
	ApplyKeyState(VK_SHIFT, bLeftShift || bRightShift || IsDown(VK_SHIFT), false);
}

void FInputManager::SynchronizeMouseButtonStates()
{
	const auto IsDown = [](int32 Key) -> bool
	{
		return (GetAsyncKeyState(Key) & 0x8000) != 0;
	};

	ApplyKeyState(MOUSE_LEFT, IsDown(MOUSE_LEFT), true);
	ApplyKeyState(MOUSE_RIGHT, IsDown(MOUSE_RIGHT), true);
	ApplyKeyState(MOUSE_MIDDLE, IsDown(MOUSE_MIDDLE), true);
	ApplyKeyState(MOUSE_X1, IsDown(MOUSE_X1), true);
	ApplyKeyState(MOUSE_X2, IsDown(MOUSE_X2), true);
}

int32 FInputManager::GetSpecificModifierKey(WPARAM WParam, LPARAM LParam)
{
	const int32 Key = static_cast<int32>(WParam);
	if (Key == VK_SHIFT)
	{
		const UINT ScanCode = static_cast<UINT>((LParam >> 16) & 0xff);
		const UINT SpecificKey = MapVirtualKey(ScanCode, MAPVK_VSC_TO_VK_EX);
		return SpecificKey != 0 ? static_cast<int32>(SpecificKey) : VK_SHIFT;
	}
	if (Key == VK_CONTROL)
	{
		return (LParam & (1 << 24)) ? VK_RCONTROL : VK_LCONTROL;
	}
	if (Key == VK_MENU)
	{
		return (LParam & (1 << 24)) ? VK_RMENU : VK_LMENU;
	}
	return Key;
}

bool FInputManager::IsMouseButtonKey(int32 Key)
{
	return Key == MOUSE_LEFT || Key == MOUSE_RIGHT || Key == MOUSE_MIDDLE || Key == MOUSE_X1 || Key == MOUSE_X2;
}

void FInputManager::UpdateDragging()
{
	for (int i = 0; i < MAX_KEYS; ++i)
	{
		if (bDragCandidate[i] && !bIsDragging[i])
		{
			float DX = static_cast<float>(LastMousePos.x - MouseDownPos[i].x);
			float DY = static_cast<float>(LastMousePos.y - MouseDownPos[i].y);
			if (std::sqrt(DX * DX + DY * DY) >= static_cast<float>(DRAG_THRESHOLD))
			{
				bIsDragging[i] = true;
				bWasDragStarted[i] = true;
			}
		}
	}
}

bool FInputManager::IsGuiUsingMouse() const
{
	if (bHasGuiCaptureOverride)
	{
		return bGuiUsingMouseOverride;
	}
	return ImGui::GetIO().WantCaptureMouse;
}

bool FInputManager::IsGuiUsingKeyboard() const
{
	if (bHasGuiCaptureOverride)
	{
		return bGuiUsingKeyboardOverride || bGuiUsingTextInputOverride;
	}
	return ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput;
}

void FInputManager::SetGuiCaptureOverride(bool bInUsingMouse, bool bInUsingKeyboard, bool bInUsingTextInput)
{
	bHasGuiCaptureOverride = true;
	bGuiUsingMouseOverride = bInUsingMouse;
	bGuiUsingKeyboardOverride = bInUsingKeyboard;
	bGuiUsingTextInputOverride = bInUsingTextInput;
}

void FInputManager::ClearGuiCaptureOverride()
{
	bHasGuiCaptureOverride = false;
	bGuiUsingMouseOverride = false;
	bGuiUsingKeyboardOverride = false;
	bGuiUsingTextInputOverride = false;
}

void FInputManager::ResetAllStates()
{
	ResetAllKeyStates();
	ResetMouseDelta();
	ResetWheelDelta();
}

void FInputManager::ResetMouseDelta()
{
	MouseDeltaX = 0.0f;
	MouseDeltaY = 0.0f;
	RawMouseDeltaAccumX = 0.0f;
	RawMouseDeltaAccumY = 0.0f;
	GetCursorPos(&LastMousePos);
}

void FInputManager::ResetWheelDelta()
{
	MouseWheelDelta = 0.0f;
	PendingWheelDelta = 0.0f;
}

void FInputManager::ResetAllKeyStates()
{
	std::memset(KeyState, 0, sizeof(KeyState));
	std::memset(PrevKeyState, 0, sizeof(PrevKeyState));
	std::memset(PressedState, 0, sizeof(PressedState));
	std::memset(ReleasedState, 0, sizeof(ReleasedState));
	std::memset(bIsDragging, 0, sizeof(bIsDragging));
	std::memset(bDragCandidate, 0, sizeof(bDragCandidate));
	EventQueue.clear();
}

bool FInputManager::IsKeyDown(int32 Key) const
{
	if (Key < 0 || Key >= MAX_KEYS) return false;
	return KeyState[Key];
}

bool FInputManager::IsKeyPressed(int32 Key) const
{
	if (Key < 0 || Key >= MAX_KEYS) return false;
	return PressedState[Key];
}

bool FInputManager::IsKeyReleased(int32 Key) const
{
	if (Key < 0 || Key >= MAX_KEYS) return false;
	return ReleasedState[Key];
}

bool FInputManager::IsMouseButtonDown(int32 Button) const
{
	return IsKeyDown(Button);
}

bool FInputManager::IsMouseButtonPressed(int32 Button) const
{
	return IsKeyPressed(Button);
}

bool FInputManager::IsMouseButtonReleased(int32 Button) const
{
	return IsKeyReleased(Button);
}

bool FInputManager::IsDragging(int32 Button) const
{
	if (Button < 0 || Button >= MAX_KEYS) return false;
	return bIsDragging[Button];
}

bool FInputManager::WasDragStarted(int32 Button) const
{
	if (Button < 0 || Button >= MAX_KEYS) return false;
	return bWasDragStarted[Button];
}

bool FInputManager::WasDragEnded(int32 Button) const
{
	if (Button < 0 || Button >= MAX_KEYS) return false;
	return bWasDragEnded[Button];
}

POINT FInputManager::GetDragDelta(int32 Button) const
{
	POINT Delta = { 0, 0 };
	if (Button >= 0 && Button < MAX_KEYS && bIsDragging[Button])
	{
		Delta.x = LastMousePos.x - MouseDownPos[Button].x;
		Delta.y = LastMousePos.y - MouseDownPos[Button].y;
	}
	return Delta;
}

float FInputManager::GetDragDistance(int32 Button) const
{
	POINT Delta = GetDragDelta(Button);
	return std::sqrt(static_cast<float>(Delta.x * Delta.x + Delta.y * Delta.y));
}
