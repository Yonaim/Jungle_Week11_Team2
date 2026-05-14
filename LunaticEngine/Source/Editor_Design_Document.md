# Editor

> 문서 버전: 1.0  
> 최종 수정일: 2026-05-11

## 세부 문서 바로가기

- [에디터 구조: Editor Structure](./Docs/editor_structure.md)
- [레벨 에디터: Level Editor](./Docs/editor_level_editor.md)
- [에셋 에디터: Asset Editor](./Docs/editor_asset_editor.md)
- [스켈레탈 메쉬 에디터: Skeletal Mesh Editor](./Docs/editor_skeletal_mesh_editor.md)

## 설계 요약

> 분리형 에디터 구조: 하나의 MainFrame 위에서 Level editing과 Asset editing 흐름을 분리해 관리한다.

에디터는 하나의 최상위 `UEditorEngine` 아래에서 실행되며,  
전체 UI 루트인 `FEditorMainFrame`을 기준으로 `FLevelEditor`와 `FAssetEditorManager`를 분리한다.

`FLevelEditor`는 현재 Level / World를 편집하는 고정 에디터이며,  
`FAssetEditorManager`는 Content Browser에서 열린 Asset의 종류에 따라 적절한 Asset Editor를 선택하고 관리한다.

## 모듈 구성

에디터 모듈은 다음 계층으로 구성된다.

- `Core`
  - `UEditorEngine`, `FEditorMainFrame` 등 에디터 전체 생명주기와 루트 UI 관리
- `LevelEditor`
  - Level Viewport, Selection, Outliner, Details, Content Browser 등 Level 편집 기능 관리
- `AssetEditor`
  - Asset 열기, ActiveEditor 관리, Asset별 전용 Editor 분기
- `CameraModifierStackEditor`
  - Camera Modifier Preset / Stack Asset 편집
- `SkeletalMeshEditor`
  - SkeletalMesh Preview, Skeleton Tree, Bone 선택, LOD / Section / Material 정보 표시
- `Common`
  - 공용 Editor Interface, Panel, Toolbar, Selection Event 등 에디터 공용 타입 관리

에디터는 하나의 거대한 패널로 구성하지 않고,  
**Logic / Management 계층과 UI Layer를 분리**하는 방식으로 구성한다.

## 핵심 용어

| 용어 | 의미 |
|---|---|
| `UEditorEngine` | 에디터 전체를 관리하는 최상위 객체 |
| `FEditorMainFrame` | 에디터 전체 UI를 감싸는 루트 컨테이너 |
| `FLevelEditor` | Level / World 편집을 담당하는 에디터 |
| `FAssetEditorManager` | 열린 Asset의 종류에 따라 적절한 Asset Editor를 선택 · 관리하는 계층 |
| `IAssetEditor` | Asset Editor들이 공통으로 따르는 인터페이스 |
| `FSelectionManager` | 현재 선택된 Actor / Component / Bone 등의 선택 상태 관리 |
| `FLevelViewportLayout` | Level Viewport들의 생성, 배치, 갱신 관리 |
| `FLevelEditorViewportClient` | Viewport 하나의 입력, 카메라, Picking, Gizmo 동작 담당 |
| `FSkeletalMeshEditor` | SkeletalMesh 전용 Asset Editor |
| `FCameraModifierStackEditor` | Camera Modifier Stack 전용 Asset Editor |

---

# Editor Structure

| 구분 | 내용 |
|---|---|
| 최초 작성자 | 김연하 |
| 최초 작성일 | 2026-05-11 |
| 최근 수정자 | 김연하 |
| 최근 수정일 | 2026-05-11 |
| 버전 | 1.0 |

## 1. 개요

> 분리형 에디터 구조: 하나의 MainFrame 위에서 Level editing과 Asset editing 흐름을 분리해 관리한다.

에디터는 크게 `Level Editor`와 `Asset Editor` 계층으로 나뉜다.

`Level Editor`는 현재 World를 대상으로 Actor 배치, 선택, Viewport 조작, Details 표시 등을 담당한다.  
반면 `Asset Editor`는 Content Browser에서 열린 특정 Asset을 독립적으로 편집하는 전용 에디터다.

이 문서의 목적은 에디터 전체 구조가 어떤 객체들로 구성되고,  
각 객체가 어떤 책임을 가지며, UI와 Logic이 어떻게 분리되는지 설명하는 것이다.

## 2. 에디터 구조 핵심

### 2.1 최상위 실행 단위: `UEditorEngine`

`UEditorEngine`은 에디터 전체를 관리하는 최상위 객체다.

```text
UEditorEngine
└─ FEditorMainFrame
   ├─ FLevelEditor
   └─ FAssetEditorManager
```

`UEditorEngine`은 직접 모든 UI 패널을 조작하기보다,  
에디터 실행에 필요한 핵심 객체를 생성하고 생명주기를 관리한다.

주요 역할은 다음과 같다.

- Editor World 생성
- Viewport 초기화
- SelectionManager 초기화
- MainFrame 생성
- AssetEditorManager 관리
- PIE 시작 / 종료 관리

즉, `UEditorEngine`은 에디터의 실행 루트이며,  
실제 Level 편집과 Asset 편집은 하위 객체에게 위임한다.

### 2.2 루트 UI 컨테이너: `FEditorMainFrame`

`FEditorMainFrame`은 에디터 전체 UI를 감싸는 루트 컨테이너다.

```text
FEditorMainFrame
├─ FLevelEditor
└─ FAssetEditorManager
```

하나의 에디터 프로세스에서 `FEditorMainFrame`은 하나만 존재한다.

`FEditorMainFrame`의 역할은 개별 편집 기능을 직접 수행하는 것이 아니라,  
여러 Editor 계층이 배치될 수 있는 최상위 UI 틀을 제공하는 것이다.

따라서 `FEditorMainFrame`은 다음 객체들을 소유하거나 연결한다.

- `FLevelEditor`
- `FAssetEditorManager`
- Main Toolbar 영역
- Docking / Panel Layout 영역
- Editor Window 루트 영역

## 3. 전체 객체 구조

현재 에디터 설계는 다음 구조를 따른다.

```text
UEditorEngine
└─ FEditorMainFrame
   ├─ FLevelEditor
   │  ├─ Logic / Management
   │  │  ├─ FSelectionManager
   │  │  ├─ FLevelViewportLayout
   │  │  │  └─ FLevelEditorViewportClient[]
   │  │  └─ FOverlayStatSystem
   │  │
   │  └─ UI
   │     ├─ FLevelEditorToolbar
   │     ├─ FLevelViewportArea
   │     ├─ FEditorOutlinerPanel
   │     ├─ FEditorDetailsPanel
   │     ├─ FEditorContentBrowser
   │     ├─ FEditorPlaceActorsPanel
   │     ├─ FEditorConsolePanel
   │     ├─ FEditorStatPanel
   │     └─ FEditorBottomToolbar
   │
   └─ FAssetEditorManager
      ├─ ActiveEditor : IAssetEditor*
      │
      ├─ FCameraModifierStackEditor
      │  ├─ Logic / State
      │  │  ├─ EditingAsset
      │  │  ├─ SelectedModifier
      │  │  └─ bDirty
      │  │
      │  └─ UI
      │     ├─ FCameraModifierStackToolbar
      │     ├─ FCameraModifierStackContentsPanel
      │     └─ FCameraModifierStackDetailsPanel
      │
      └─ FSkeletalMeshEditor
         ├─ Logic / State
         │  ├─ EditingMesh
         │  ├─ EditingSkeleton
         │  ├─ SelectedBoneIndex
         │  ├─ CurrentLODIndex
         │  ├─ bShowBones
         │  └─ PreviewMode
         │
         └─ UI
            ├─ FSkeletalMeshEditorToolbar
            ├─ FSkeletalMeshPreviewViewport
            ├─ FSkeletonTreePanel
            └─ FSkeletalMeshDetailsPanel
```

각 Editor는 크게 다음 두 영역으로 나뉜다.

| 영역 | 역할 |
|---|---|
| Logic / Management | 실제 상태, 선택 정보, 편집 대상, 실행 흐름 관리 |
| UI Layer | Toolbar, Panel, Viewport 등 화면 표시와 사용자 입력 전달 |

UI는 가능한 한 상태를 직접 소유하지 않고,  
상태는 Editor의 Logic 계층에 두는 것을 기준으로 한다.

## 4. Level Editor

### 4.1 역할

`FLevelEditor`는 현재 Level / World를 편집하는 에디터다.

다중 Level Editor 인스턴스를 관리하는 구조를 아직 고려하지 않으므로,  
이름은 `FLevelEditorManager`가 아니라 `FLevelEditor`로 둔다.

```text
FLevelEditor
├─ Logic / Management
│  ├─ FSelectionManager
│  ├─ FLevelViewportLayout
│  │  └─ FLevelEditorViewportClient[]
│  └─ FOverlayStatSystem
│
└─ UI
   ├─ FLevelEditorToolbar
   ├─ FLevelViewportArea
   ├─ FEditorOutlinerPanel
   ├─ FEditorDetailsPanel
   ├─ FEditorContentBrowser
   ├─ FEditorPlaceActorsPanel
   ├─ FEditorConsolePanel
   ├─ FEditorStatPanel
   └─ FEditorBottomToolbar
```

`FLevelEditor`는 Level 편집에 필요한 상태와 UI를 하나로 묶는 단위다.

주요 역할은 다음과 같다.

- Actor 선택
- Component 선택
- Viewport Layout 관리
- Viewport Client 갱신
- Outliner / Details 연동
- Editor Overlay Stat 표시
- Content Browser와 Asset Editor 연결

### 4.2 `FSelectionManager`

`FSelectionManager`는 현재 에디터에서 무엇이 선택되어 있는지 관리한다.

관리 대상 예시는 다음과 같다.

- 선택된 Actor
- 선택된 Component
- 다중 선택 목록
- 선택 해제
- 선택 변경 이벤트

`FSelectionManager`는 UI Panel이 아니라 Logic 객체다.  
따라서 Outliner, Details, Viewport는 직접 선택 상태를 각각 들고 있지 않고,  
공통 SelectionManager를 통해 현재 선택 상태를 참조한다.

### 4.3 `FLevelViewportLayout`

`FLevelViewportLayout`은 Level Editor Viewport들을 생성하고 배치하는 관리자다.

```text
FLevelViewportLayout
├─ Viewport 0: Perspective
├─ Viewport 1: Top
├─ Viewport 2: Front
└─ Viewport 3: Side
```

`FLevelViewportLayout`은 개별 Viewport의 카메라 이동이나 Picking을 직접 처리하지 않는다.

대신 다음 역할을 담당한다.

- Viewport 개수 관리
- Viewport 배치 관리
- Perspective / Top / Front / Side Viewport 구성
- 각 Viewport에 대응하는 `FLevelEditorViewportClient` 생성
- 전체 Viewport 갱신 요청

### 4.4 `FLevelEditorViewportClient`

`FLevelEditorViewportClient`는 Viewport 하나의 실제 동작을 담당한다.

```text
FLevelViewportLayout
└─ FLevelEditorViewportClient[]
   ├─ Perspective Viewport Client
   ├─ Top Viewport Client
   └─ Front Viewport Client
```

주요 역할은 다음과 같다.

- Viewport Camera 이동
- 마우스 / 키보드 입력 처리
- Actor Picking
- Gizmo 조작
- Viewport 렌더 옵션 관리
- Grid 표시 여부 관리
- ViewMode / RenderMode 관리

즉, `FLevelViewportLayout`은 여러 Viewport를 배치하는 쪽이고,  
`FLevelEditorViewportClient`는 Viewport 하나의 동작을 처리하는 쪽이다.

### 4.5 `FOverlayStatSystem`

`FOverlayStatSystem`은 화면 위에 표시되는 통계와 디버그 정보를 관리한다.

표시 대상 예시는 다음과 같다.

```text
FPS
FrameTime
DrawCall
TriangleCount
Memory
Viewport Stats
```

Viewport에 직접 문자열을 흩뿌리지 않고,  
Overlay Stat 전용 시스템을 통해 표시할 정보를 수집하고 렌더링한다.

## 5. Asset Editor

### 5.1 역할

`FAssetEditorManager`는 Asset을 열 때 어떤 에디터로 열지 결정하는 관리자다.

```text
FAssetEditorManager
├─ ActiveEditor : IAssetEditor*
├─ FCameraModifierStackEditor
└─ FSkeletalMeshEditor
```

`FLevelEditor`와 달리, Asset Editor는 여러 종류의 Asset에 대해 서로 다른 전용 Editor가 필요하다.

따라서 `FAssetEditorManager`는 다음 역할을 가진다.

- Asset Type 판별
- Asset에 맞는 Editor 선택
- ActiveEditor 관리
- Editor 열기 / 닫기
- Editor 저장 요청
- Content Browser 더블클릭 이벤트 처리

예시 흐름은 다음과 같다.

```text
SkeletalMesh.uasset 더블클릭
→ FAssetEditorManager::OpenAsset()
→ Asset Type 확인
→ FSkeletalMeshEditor 선택
→ ActiveEditor 설정
→ FSkeletalMeshEditor::OpenAsset()
```

### 5.2 `IAssetEditor`

`IAssetEditor`는 Asset Editor들이 공통으로 따라야 하는 인터페이스다.

```cpp
class IAssetEditor
{
public:
    virtual ~IAssetEditor() = default;

    virtual bool OpenAsset(UObject* Asset) = 0;
    virtual void Tick(float DeltaTime) = 0;
    virtual void Render(float DeltaTime) = 0;
    virtual bool Save() = 0;
    virtual void Close() = 0;
};
```

현재 기준으로 `IAssetEditor`를 구현할 Editor는 다음과 같다.

- `FCameraModifierStackEditor`
- `FSkeletalMeshEditor`

`FAssetEditorManager`는 구체적인 Asset Editor의 내부 UI를 직접 알 필요가 없다.  
공통 인터페이스인 `IAssetEditor`를 통해 열기, 저장, 렌더링, 닫기만 호출한다.

## 6. Skeletal Mesh Editor

### 6.1 역할

`FSkeletalMeshEditor`는 SkeletalMesh 전용 Asset Editor다.

```text
FSkeletalMeshEditor
├─ Logic / State
│  ├─ EditingMesh
│  ├─ EditingSkeleton
│  ├─ SelectedBoneIndex
│  ├─ CurrentLODIndex
│  ├─ bShowBones
│  └─ PreviewMode
│
└─ UI
   ├─ FSkeletalMeshEditorToolbar
   ├─ FSkeletalMeshPreviewViewport
   ├─ FSkeletonTreePanel
   └─ FSkeletalMeshDetailsPanel
```

주요 역할은 다음과 같다.

- SkeletalMesh 열기
- Preview Viewport에 Mesh 표시
- Skeleton Tree 표시
- Bone 선택 상태 관리
- LOD / Section / Material 정보 표시
- Reference Pose / Skinned Pose 보기
- Bone 표시 여부 토글
- 현재 Preview Mode 관리

### 6.2 상태 데이터

`FSkeletalMeshEditor`의 Logic / State는 다음 정보를 가진다.

| 상태 | 의미 |
|---|---|
| `EditingMesh` | 현재 편집 중인 SkeletalMesh |
| `EditingSkeleton` | 현재 Mesh가 참조하는 Skeleton |
| `SelectedBoneIndex` | Skeleton Tree 또는 Viewport에서 선택된 Bone Index |
| `CurrentLODIndex` | 현재 표시 중인 LOD Index |
| `bShowBones` | Preview Viewport에서 Bone을 표시할지 여부 |
| `PreviewMode` | Reference Pose / Skinned Pose 등 현재 미리보기 방식 |

이 상태들은 UI Panel이 각각 따로 들고 있지 않고,  
`FSkeletalMeshEditor`가 중심이 되어 관리한다.

### 6.3 UI 구성

`FSkeletalMeshEditor`의 UI는 다음 패널로 구성된다.

| UI | 역할 |
|---|---|
| `FSkeletalMeshEditorToolbar` | Save, PreviewMode, ShowBones, LOD 선택 등 상단 명령 제공 |
| `FSkeletalMeshPreviewViewport` | SkeletalMesh를 미리보기로 렌더링 |
| `FSkeletonTreePanel` | Skeleton 계층 구조 표시 및 Bone 선택 |
| `FSkeletalMeshDetailsPanel` | Mesh, LOD, Section, Material, Bone 정보 표시 |

### 6.4 열기 흐름

SkeletalMesh Asset을 여는 흐름은 다음과 같다.

```text
Content Browser에서 SkeletalMesh.uasset 더블클릭
→ FAssetEditorManager::OpenAsset()
→ Asset Type이 SkeletalMesh인지 확인
→ FSkeletalMeshEditor 생성 또는 재사용
→ FSkeletalMeshEditor::OpenAsset()
→ EditingMesh 설정
→ EditingSkeleton 설정
→ PreviewViewport에 Mesh 전달
→ SkeletonTreePanel 갱신
→ DetailsPanel 갱신
```

이 흐름에서 `FAssetEditorManager`는 Editor 선택까지만 담당한다.  
실제 SkeletalMesh 내부 표시와 편집 상태 관리는 `FSkeletalMeshEditor`가 담당한다.

### 6.5 Bone 선택 흐름

Bone 선택 흐름은 다음과 같다.

```text
SkeletonTreePanel에서 Bone 클릭
→ FSkeletalMeshEditor::SetSelectedBone()
→ SelectedBoneIndex 갱신
→ DetailsPanel에 Bone 정보 표시
→ PreviewViewport에서 선택 Bone Highlight
```

Bone 선택 상태는 `FSkeletonTreePanel`에만 존재하지 않는다.  
공통 상태인 `SelectedBoneIndex`를 `FSkeletalMeshEditor`에 두고,  
Tree Panel과 Preview Viewport가 같은 선택 상태를 바라보도록 한다.

## 7. 디렉토리 구조

에디터 모듈의 기준 디렉토리 구조는 다음과 같다.

```text
Editor/
├─ Core/
│  ├─ EditorEngine.h
│  ├─ EditorEngine.cpp
│  ├─ EditorMainFrame.h
│  └─ EditorMainFrame.cpp
│
├─ LevelEditor/
│  ├─ LevelEditor.h
│  ├─ LevelEditor.cpp
│  │
│  ├─ Selection/
│  │  ├─ SelectionManager.h
│  │  └─ SelectionManager.cpp
│  │
│  ├─ Viewport/
│  │  ├─ LevelViewportLayout.h
│  │  ├─ LevelViewportLayout.cpp
│  │  ├─ LevelEditorViewportClient.h
│  │  └─ LevelEditorViewportClient.cpp
│  │
│  ├─ Overlay/
│  │  ├─ OverlayStatSystem.h
│  │  └─ OverlayStatSystem.cpp
│  │
│  └─ UI/
│     ├─ LevelEditorToolbar.h
│     ├─ LevelViewportArea.h
│     ├─ EditorOutlinerPanel.h
│     ├─ EditorDetailsPanel.h
│     ├─ EditorContentBrowser.h
│     ├─ EditorPlaceActorsPanel.h
│     ├─ EditorConsolePanel.h
│     ├─ EditorStatPanel.h
│     └─ EditorBottomToolbar.h
│
├─ AssetEditor/
│  ├─ AssetEditorManager.h
│  ├─ AssetEditorManager.cpp
│  │
│  ├─ Interface/
│  │  └─ IAssetEditor.h
│  │
│  ├─ CameraModifierStackEditor/
│  │  ├─ CameraModifierStackEditor.h
│  │  ├─ CameraModifierStackEditor.cpp
│  │  └─ UI/
│  │     ├─ CameraModifierStackToolbar.h
│  │     ├─ CameraModifierStackContentsPanel.h
│  │     └─ CameraModifierStackDetailsPanel.h
│  │
│  └─ SkeletalMeshEditor/
│     ├─ SkeletalMeshEditor.h
│     ├─ SkeletalMeshEditor.cpp
│     ├─ SkeletalMeshEditorTypes.h
│     │
│     ├─ Viewport/
│     │  ├─ SkeletalMeshPreviewViewport.h
│     │  └─ SkeletalMeshPreviewViewport.cpp
│     │
│     └─ UI/
│        ├─ SkeletalMeshEditorToolbar.h
│        ├─ SkeletonTreePanel.h
│        └─ SkeletalMeshDetailsPanel.h
│
└─ Common/
   ├─ EditorDelegates.h
   ├─ EditorPanel.h
   ├─ EditorToolbar.h
   └─ EditorUIUtils.h
```

## 8. 이름 규칙

### 8.1 `Editor`와 `Manager` 구분

| 이름 | 사용 기준 |
|---|---|
| `Editor` | 실제 편집 대상과 UI를 가지는 편집 단위 |
| `Manager` | 여러 객체를 선택, 생성, 조회, 전환하는 관리자 |
| `Panel` | 특정 영역에 표시되는 UI 조각 |
| `Toolbar` | 명령 버튼과 토글을 제공하는 UI 영역 |
| `ViewportClient` | Viewport 하나의 입력과 동작을 처리하는 객체 |
| `Layout` | 여러 Viewport나 Panel의 배치 관리 객체 |

`FLevelEditor`는 현재 구조에서 하나의 Level editing 환경을 의미하므로 `Manager`를 붙이지 않는다.

반면 `FAssetEditorManager`는 여러 Asset Editor 중 어떤 Editor를 사용할지 결정하고,  
현재 활성 Editor를 관리하므로 `Manager`를 붙인다.

### 8.2 UI와 Logic 이름 분리

UI 객체는 이름에 역할을 명확히 붙인다.

```text
FEditorOutlinerPanel
FEditorDetailsPanel
FEditorContentBrowser
FSkeletalMeshEditorToolbar
FSkeletonTreePanel
```

반면 상태와 실행 흐름을 담당하는 객체는 `Panel` 이름을 붙이지 않는다.

```text
FSelectionManager
FLevelViewportLayout
FLevelEditorViewportClient
FAssetEditorManager
FSkeletalMeshEditor
```

## 9. 설계 기준

에디터 구조는 다음 기준을 따른다.

1. `UEditorEngine`은 에디터 전체 생명주기를 관리한다.
2. `FEditorMainFrame`은 최상위 UI 컨테이너 역할만 담당한다.
3. `FLevelEditor`는 Level / World 편집 기능을 묶는다.
4. `FAssetEditorManager`는 Asset Type에 따라 적절한 Asset Editor를 선택한다.
5. Asset Editor들은 `IAssetEditor` 인터페이스를 따른다.
6. UI Panel은 상태를 직접 소유하지 않고, Editor Logic 계층의 상태를 표시한다.
7. Viewport 배치와 Viewport 동작은 `Layout`과 `ViewportClient`로 분리한다.
8. SkeletalMeshEditor의 선택 상태는 Tree Panel이 아니라 Editor 본체가 관리한다.

이 구조를 따르면 Level editing과 Asset editing이 서로 섞이지 않고,  
새로운 Asset Editor를 추가할 때도 `FAssetEditorManager`에 분기만 추가하여 확장할 수 있다.
