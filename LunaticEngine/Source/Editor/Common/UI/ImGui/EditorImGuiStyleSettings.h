#pragma once

/**
 * Editor ImGui 스타일 설정 패널과 저장/로드를 담당한다.
 *
 * 역할:
 * - ImGui Style 값을 편집하는 설정 패널을 그린다.
 * - 색상/라운딩/테두리 값을 Settings/ImGuiStyle.ini에 저장한다.
 * - Editor ImGui 초기화 시 저장된 스타일을 다시 적용한다.
 *
 * 주의:
 * - ImGui Context나 Win32/DX11 backend 생명주기는 소유하지 않는다.
 * - 실제 ImGui 생명주기는 FEditorImGuiSystem이 단독으로 관리한다.
 */
class FEditorImGuiStyleSettings
{
  public:
    static void ShowPanel(bool *bOpen = nullptr);
    static void Save();
    static void Load();
};
