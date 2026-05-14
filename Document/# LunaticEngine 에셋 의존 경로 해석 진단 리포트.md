# LunaticEngine 에셋 의존 경로 해석 진단 리포트

- 작성일: 2026-05-14
- 대상 저장소: `C:\GitDirectory11` (브랜치: `feature/AssetFilePath`)
- 대상 시스템: `.uasset` 직렬화 / 의존 에셋 파일경로 해석
- 진단 성격: **읽기 전용** — 코드 수정 없음, 픽스 제안 없음, 의심 지점 목록만 산출

---

## 1. 개요 (Executive Summary)

`.uasset` 헤더에 `AssetGuid` 필드는 존재하나(`AssetFileSerializer.h:30`) **저장 시마다 새로 생성되고 로드 시 룩업에 사용되지 않으므로**(`AssetFileSerializer.cpp:33-37`), 실질적인 에셋 식별자는 **파일경로 단독**이다. 의존 에셋 경로는 일반 `FString`으로 `operator<<(FArchive&, std::string&)`(`Archive.h:38-47`)에 그대로 흘러가며 직렬화 라이브러리 차원의 정규화/검증이 없다. 정규화는 호출자가 의도적으로 `FPaths::NormalizePath`를 부를 때만 수행되며(`Paths.cpp:188-198`), 호출 누락 위험 지점이 다수 식별된다. 특히 `FStaticMaterial::operator<<`(`StaticMeshCommon.h:46-70`), `UMaterial::Serialize`의 텍스처 슬롯 저장(`Material.cpp:382-391`), `USkeletonPoseAsset::TargetSkeletonPath`(`AssetData.cpp:152-176`)에서 **저장 시 정규화 부재** 및 **로드 시 자동 해석 부재**가 관찰되었다. 마운트/가상경로 시스템은 없고 식별 체계는 디스크 파일경로 + `FPaths::RootDir()` 절대화에 전적으로 의존하므로, 머신/RootDir 변경에 취약하다.

---

## 2. 저장소 구조 (Repository Map)

### 최상위 레이아웃

```
C:\GitDirectory11
├── LunaticEngine\
│   ├── Source\
│   │   ├── Engine\        (런타임 엔진 본체)
│   │   ├── Editor\        (에디터 계층)
│   │   ├── ObjViewer\     (스탠드얼론 메시 뷰어)
│   │   ├── Game\
│   │   └── PCH\
│   ├── Asset\             (콘텐츠/캐시)
│   ├── Data\              (원본 .obj/.fbx 등)
│   └── Shaders\
├── Scripts\
├── Document\              (비어 있음)
├── AGENTS.md
└── CLAUDE.md
```

### 에셋 시스템 관련 파일 인벤토리

| 역할 | 경로 | 주요 심볼 |
| --- | --- | --- |
| 에셋 직렬화 진입점 | `LunaticEngine/Source/Engine/Asset/AssetFileSerializer.h`, `.cpp` | `SaveObjectToAssetFile`, `LoadObjectFromAssetFile`, `FAssetFileHeader` (magic `0x5453414A` 'JAST', `AssetVersion=4`) |
| 에셋 데이터 베이스 | `LunaticEngine/Source/Engine/Asset/AssetData.h`, `.cpp` | `UAssetData`, `USkeletonPoseAsset` |
| 경로 유틸 | `LunaticEngine/Source/Engine/Platform/Paths.h`, `.cpp` | `NormalizePath`, `ConvertRelativePathToFull`, `ResolvePathToDisk`, `ResolveAssetPath`, `RootDir`, `RemapLegacyAssetPathString` (익명 ns) |
| 스크립트 경로 유틸 | `LunaticEngine/Source/Engine/Platform/ScriptPaths.h`, `.cpp` | `NormalizeScriptPath`, `ResolveScriptPath`, `ReadScriptFile` |
| Archive 베이스 | `LunaticEngine/Source/Engine/Serialization/Archive.h` | `FArchive`, `operator<<(FArchive&, std::string&)`, `operator<<(FArchive&, FName&)` |
| Windows Archive | `LunaticEngine/Source/Engine/Serialization/WindowsArchive.h` | `FWindowsBinWriter`, `FWindowsBinReader` |
| StaticMesh 데이터 + 머티리얼 참조 | `LunaticEngine/Source/Engine/Mesh/StaticMeshCommon.h` | `FStaticMaterial`, `FStaticMesh::Serialize` |
| StaticMesh UObject | `LunaticEngine/Source/Engine/Mesh/StaticMesh.cpp` | `UStaticMesh::Serialize` |
| SkeletalMesh | `LunaticEngine/Source/Engine/Mesh/SkeletalMesh.cpp`, `SkeletalMeshCommon.h` | `USkeletalMesh::Serialize` |
| Mesh 매니저 (캐시/경로 매핑) | `LunaticEngine/Source/Engine/Mesh/MeshAssetManager.cpp`, `.h` | `MakeMeshAssetPath`, `NormalizeMeshAssetCacheKey`, `LoadStaticMeshAssetFile` |
| Material | `LunaticEngine/Source/Engine/Materials/Material.h`, `.cpp` | `UMaterial::Serialize`, `GetAssetPathFileName`, `SetAssetPathFileName` |
| Material 매니저 (캐시/로드) | `LunaticEngine/Source/Engine/Materials/MaterialManager.cpp` | `ResolveMaterialAssetDiskPath`, `GetOrCreateMaterial`, `MaterialCache` |
| Texture | `LunaticEngine/Source/Engine/Texture/Texture2D.cpp` | `UTexture2D::Serialize`, `LoadFromFile`, `FindTextureSourceOnDisk` |
| Import 진입점 | `LunaticEngine/Source/Editor/AssetTools/AssetImportManager.cpp`, `.h` | `ImportAssetFromPath`, `SaveImportedObject` |
| FBX 임포터 (텍스처 경로 해석 포함) | `LunaticEngine/Source/Engine/Mesh/FbxCommon.cpp` | `ResolveFbxTexturePath`, 머티리얼 JSON 생성 |
| OBJ 임포터 | `LunaticEngine/Source/Engine/Mesh/ObjImporter.cpp` | `FObjImporter::Import` |

> 비고: 본 엔진에는 별도의 prebuilt `.bin` MeshCache 인덱스가 존재하지 않는다. `.uasset` 자체가 cooked 결과물이며, 소스 파일 modtime과 비교해 재임포트 여부를 결정한다 (`MeshAssetManager.cpp:117-130`, `:728-738`).

---

## 3. 로직 흐름 (Logic Flow)

다음은 "사용자가 메시 에셋을 임포트해 `.uasset`을 저장 → 다른 머시에서 해당 `.uasset`을 로드 → 머티리얼/텍스처 의존 해석" 시 호출 체인이다.

### 3.1 임포트/저장 경로

1. 사용자가 에디터에서 파일 다이얼로그로 `C:\Foo\Bar.fbx` 선택 → `FAssetImportManager::ImportAssetFromPath` 진입 (`AssetImportManager.h:23` 선언).
2. 메시 분기 → `ImportMeshSource` → `FFbxStaticMeshImporter::Import(SourcePath, …)` 또는 `FObjImporter::Import`.
3. FBX의 텍스처 경로는 `ResolveFbxTexturePath` (`FbxCommon.cpp:506-585`)에서 후보 디렉토리 탐색 후 `FPaths::NormalizePath` 적용 (`FbxCommon.cpp:563, 572, 577` 부근).
4. 머티리얼 정보가 JSON으로 구성됨 (`FbxCommon.cpp:399-451`).
5. `SaveImportedObject` 호출 → `FAssetFileSerializer::SaveObjectToAssetFile(FilePath, RootObject)` (`AssetFileSerializer.cpp:132-167`).
6. `ToAssetPathString(FilePath)` (`AssetFileSerializer.cpp:28-31`)로 `.uasset` 자체 경로를 절대경로로 변환 후 `FWindowsBinWriter`로 오픈.
7. 헤더 작성 (`AssetFileSerializer.cpp:151-161`) — `Magic`, `FileVersion=4`, `ClassId`, `ClassName`, `AssetName`, `AssetGuid` (매번 새로 생성), `DependencyCount=0`, `BodyOffset=0`.
8. `RootObject->Serialize(Writer)` (`AssetFileSerializer.cpp:163`)로 본체 직렬화. 이때:
   - `UStaticMesh::Serialize` (`StaticMesh.cpp:53-72`) → `StaticMeshAsset->Serialize(Ar)` (`StaticMeshCommon.h:111-117`)로 vertex/index/section 기록, 이어서 `Ar << StaticMaterials`.
   - `TArray<FStaticMaterial>` 직렬화는 요소별로 `FStaticMaterial::operator<<` (`StaticMeshCommon.h:46-70`)를 호출. 저장 시 `JsonPath = Mat.MaterialInterface->GetAssetPathFileName()`을 그대로 `Ar << JsonPath` 기록 (정규화 호출 없음).
9. 디스크에 .uasset 파일이 생성됨.

### 3.2 로드/해석 경로

1. 어딘가에서 `FMeshAssetManager::LoadStaticMeshAssetFile(AssetPath, Device)` 호출 (`MeshAssetManager.cpp` 내).
2. `NormalizeMeshAssetCacheKey(AssetPath)` = `FPaths::NormalizePath(AssetPath)` (`MeshAssetManager.cpp:133-141`).
3. `StaticMeshCache` 조회 (TMap<FString,UStaticMesh*>), 미스 시 `FAssetFileSerializer::LoadObjectFromAssetFile`로 디스크 로드.
4. `LoadObjectFromAssetFile` (`AssetFileSerializer.cpp:169-233`): `ToAssetPathString`으로 절대경로화 후 `FWindowsBinReader` 오픈, 헤더 검증.
5. `FObjectFactory::Get().Create(ClassName)`로 빈 UObject 생성 (`AssetFileSerializer.cpp:215`).
6. `Object->Serialize(Reader)` (`AssetFileSerializer.cpp:229`).
7. `UStaticMesh::Serialize` 로드 분기 → `FStaticMaterial::operator<<` 로드 분기:
   ```cpp
   if (Ar.IsLoading()) {
       if (!JsonPath.empty()) {
           Mat.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(JsonPath);
       }
   }
   ```
   (`StaticMeshCommon.h:57-66`)
8. `FMaterialManager::GetOrCreateMaterial(MaterialPath)` (`MaterialManager.cpp:150-208`):
   - L152: `GenericPath = FPaths::NormalizePath(MaterialPath)` — 여기서 비로소 정규화.
   - L159-163: `MaterialCache.find(GenericPath)` 조회.
   - L165 이후 .uasset 확장자면 `ResolveMaterialAssetDiskPath(GenericPath)` (`MaterialManager.cpp:20-29`) → `FPaths::ResolvePathToDisk`로 디스크 경로 결정, `LoadObjectFromAssetFile` 재귀 로드.
   - L173: `LoadedMaterial->SetAssetPathFileName(GenericPath)` — Material 자신의 식별자를 GenericPath로 설정.
   - L192-207: 로드 실패 시 폴백 마젠타 머티리얼을 생성하여 `MaterialCache`에 저장.
9. Material 로드 본체 `UMaterial::Serialize` (`Material.cpp:309-416`)는 다시 `Ar << PathFileName` (L311), 셰이더, 텍스처 슬롯을 차례로 읽는다. 텍스처 슬롯은 `Ar << TexturePath`를 그대로 읽고 `UTexture2D::LoadFromFile(TexturePath, Device)` 호출 (`Material.cpp:393-415`).

### 3.3 흐름 요약 다이어그램

```
[FBX/OBJ 임포트] ──▶ FbxCommon::ResolveFbxTexturePath (정규화 O)
        │
        ▼
[FAssetFileSerializer::SaveObjectToAssetFile]
        ├─ ToAssetPathString  : .uasset 파일 자체 경로 = ABSOLUTE
        ├─ Header(Magic/Ver/Class/Guid)  : Guid는 every-save 신규
        └─ Object->Serialize
            └─ FStaticMaterial::operator<<
                └─ Ar << GetAssetPathFileName()  ← 정규화 호출 없음
                    (= Material의 PathFileName 현재 상태에 의존)

[로드 시점]
[FMeshAssetManager::LoadStaticMeshAssetFile]
        └─ FAssetFileSerializer::LoadObjectFromAssetFile
            └─ Object->Serialize
                └─ FStaticMaterial::operator<< (Ar.IsLoading)
                    └─ Ar >> JsonPath  ← 저장 당시 바이트 그대로
                        └─ FMaterialManager::GetOrCreateMaterial(JsonPath)
                            ├─ NormalizePath(JsonPath)   ← 여기서 비로소 정규화
                            ├─ MaterialCache lookup
                            └─ ResolvePathToDisk → LoadObjectFromAssetFile
```

> 의존 경로의 정규화는 **저장 측이 아니라 로드 측 매니저**에 비대칭적으로 위치한다. 저장 측이 비정규형(예: backslash, 절대경로, `Asset/Game/Content/…` 레거시 형태)을 기록하면, 로드 측 정규화는 단방향이라 동등성을 회복하지 못할 수 있다.

---

## 4. 체크포인트 분석 (Chokepoint Analysis)

### 4.A 변환 시 직렬화 (Source → .uasset)

- **의존 경로 표현**: `FString` (≡ `std::string`, UTF-8). GUID/해시 기반 ID는 사용하지 않는다.
- **직렬화 메커니즘**: 모든 의존 경로는 일반 문자열 `operator<<`를 통과한다.
  ```cpp
  // LunaticEngine/Source/Engine/Serialization/Archive.h:38-47
  inline FArchive& operator<<(FArchive& Ar, std::string& Str)
  {
      uint32 Length = static_cast<uint32>(Str.size());
      Ar << Length;
      if (Ar.IsLoading()) Str.resize(Length);
      if (Length > 0) Ar.Serialize(Str.data(), Length * sizeof(char));
      return Ar;
  }
  ```
  → 경로 전용 특수 오버로딩은 없다. 정규화/검증 훅이 없으므로 저장 측이 책임진다.
- **저장 측 정규화 적용 사례**:
  - `.uasset` 파일 자체 경로: `AssetFileSerializer.cpp:28-31`에서 `ConvertRelativePathToFull(NormalizePath(...))` 적용.
  - `FbxCommon`의 텍스처 경로: 임포트 단계에서 `FPaths::NormalizePath` 호출됨(`FbxCommon.cpp:563, 572, 577` 부근).
  - `MeshAssetManager::MakeMeshAssetPath`: 결과를 `NormalizePath`로 마감(`MeshAssetManager.cpp:109, 114`).
- **저장 측 정규화 누락 사례**:
  - `FStaticMaterial::operator<<` 저장 분기에서 `JsonPath = Mat.MaterialInterface->GetAssetPathFileName(); Ar << JsonPath;` — `NormalizePath` 호출 없음 (`StaticMeshCommon.h:50-55`).
  - `UMaterial::Serialize` 텍스처 슬롯 저장: `FString TexturePath = Pair.second ? Pair.second->GetSourcePath() : FString(); Ar << TexturePath;` — `NormalizePath` 호출 없음 (`Material.cpp:386-391`).
  - `UMaterial::Serialize` 본인 식별자 `Ar << PathFileName` 저장 (`Material.cpp:311`) — `NormalizePath` 호출 없음.
  - `USkeletonPoseAsset::Serialize`의 `Ar << TargetSkeletonPath` (`AssetData.cpp:156`) — `NormalizePath` 호출 없음.
- **결론**: 저장된 바이트가 정규화 형식인지 여부는 “직전에 누군가 정규화된 값을 setter로 넣었느냐”라는 **암묵적 불변식**에 전적으로 의존한다. 시스템 차원의 보장은 없다.

### 4.B 로드 시 역직렬화 (.uasset → 런타임 객체)

- 동일 `operator<<`로 길이 프리픽스 후 바이트 그대로 읽힘 (`Archive.h:43-44`).
- 매니저 계층에서 정규화가 재시도된다:
  - `FMaterialManager::GetOrCreateMaterial` L152: `GenericPath = FPaths::NormalizePath(MaterialPath);` (`MaterialManager.cpp:150-208`).
  - `FMeshAssetManager::NormalizeMeshAssetCacheKey`도 `NormalizePath` 호출 (`MeshAssetManager.cpp:133-141`).
- 디스크 경로 결정:
  ```cpp
  // LunaticEngine/Source/Engine/Materials/MaterialManager.cpp:20-29
  std::filesystem::path ResolveMaterialAssetDiskPath(const FString& MaterialPath)
  {
      const std::filesystem::path Resolved(FPaths::ResolvePathToDisk(MaterialPath));
      if (!Resolved.empty()) return Resolved.lexically_normal();
      return std::filesystem::path(FPaths::ToWide(FPaths::NormalizePath(MaterialPath))).lexically_normal();
  }
  ```
- 해석 컨텍스트: `FPaths::RootDir()` (`Paths.cpp:117-139`)이 기준 절대경로. exe 경로 → 부모 디렉토리 walk-up으로 `Shaders/`, `Settings/`, `Asset/`를 모두 가진 디렉토리를 찾고, 실패 시 `current_path()`, 그것도 실패 시 exe 부모 디렉토리. `static` 캐시로 1회 결정.
- **비대칭의 핵심**: 저장 측이 비정규/절대/backslash로 기록했을 때, 로드 측이 적용하는 `NormalizePath`는 단방향 리매핑(`Paths.cpp:25-65`)만을 알고 있어 동일 키 회복이 보장되지 않는다.

### 4.C 경로 정규화 계층

- **단일 정규화기**: `FPaths::NormalizePath` (`Paths.cpp:188-198`)
  ```cpp
  std::string FPaths::NormalizePath(const std::string& Path)
  {
      if (Path.empty()) return {};
      std::filesystem::path FsPath(ToWide(Path));
      FsPath = RemapLegacyAssetPath(FsPath.lexically_normal());
      return ToUtf8(FsPath.generic_wstring());
  }
  ```
- **리매핑 규칙(`Paths.cpp:25-65`)**:
  - 모든 `\` → `/`.
  - `Asset/Game/Content/` → `Asset/Content/`, `Asset/Engine/Content/` → `Asset/Content/`, `Asset/Game/Source/` → `Asset/Source/`, `Asset/Engine/Source/` → `Asset/Source/`.
  - `Asset/Content/Sound/` → `Asset/Content/Sounds/`.
  - 디렉토리 단독 비교 케이스도 동일하게 처리.
  - **역방향 규칙 없음**.
- **Separator 처리**: 라이터는 `.generic_wstring()`로 항상 forward-slash 출력(`Paths.cpp:197`). 그러나 정규화를 거치지 않고 저장된 경로는 backslash가 남을 수 있다(예: `GetAssetPathFileName()`이 setter로 backslash 형태를 받았던 경우).
- **대소문자**: `FPaths::NormalizePath`는 대소문자를 건드리지 않는다. 캐시 키(`MaterialCache`, `StaticMeshCache`, `TextureCache`)는 `FString` 기반 case-sensitive 비교다. `FName`은 case-insensitive 풀(`FName` 구현 인용 가능)이지만, 경로는 `FName`을 거치지 않고 평문 `FString`으로만 이동한다.

### 4.D 참조/ID 간접화 (Indirection)

- **GUID 필드**: `FAssetFileHeader::AssetGuid` (`AssetFileSerializer.h:30`).
- **GUID 생성**:
  ```cpp
  // LunaticEngine/Source/Engine/Asset/AssetFileSerializer.cpp:33-37
  FString MakeAssetGuidString()
  {
      static uint64 Counter = 1;
      const uint64 TimeSeed = static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
      return std::to_string(TimeSeed) + "-" + std::to_string(Counter++);
  }
  ```
  → **저장할 때마다 새 값**. 동일 자산의 ID 안정성 없음.
- **GUID 사용처**: `LoadObjectFromAssetFile`은 헤더에서 GUID를 읽어 `Header.AssetGuid`에 보관하지만, 룩업/매핑/검증에 사용하지 않는다. 코드상 GUID로 자산을 찾는 경로 없음.
- **Mount/VirtualPath**: 코드베이스 전반에 마운트 또는 `/Game/`, `/Engine/` 스타일 가상 경로 시스템이 없다. 식별 체계는 디스크 절대경로 + `RootDir()` 기반 상대화에 의존.
- **결론**: 실질적 자산 키는 **파일경로 단독**. 위치/이름이 바뀌면 모든 의존이 끊긴다.

---

## 5. 의심 지점 목록 (Suspicion List)

> 신뢰도 순으로 정렬. 본 패스에서는 픽스 제안을 하지 않는다.

### S1. AssetGuid는 정보용일 뿐 실제 식별자가 아님 — **신뢰도: 높음**

- **위치**: `AssetFileSerializer.cpp:33-37`, `:151-161`, `:181-212`.
- **관찰된 내용**:
  ```cpp
  FString MakeAssetGuidString()
  {
      static uint64 Counter = 1;
      const uint64 TimeSeed = static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
      return std::to_string(TimeSeed) + "-" + std::to_string(Counter++);
  }
  ```
  헤더 작성 시 `Header.AssetGuid = MakeAssetGuidString();` (`AssetFileSerializer.cpp:157`)으로 새로 부여, 로드 시 `Reader << Header.AssetGuid` 로 읽기만 함.
- **의심 사유**: 파일경로가 바뀌면 동일 콘텐츠라도 자산을 다시 연결할 수 있는 안정적 ID가 없다. UE의 GUID/SoftObjectPath와 같은 indirection이 부재해, 본 증상(의존 파일경로 해석 실패)이 파일 이동/리네임 시 그대로 재현될 가능성이 크다.
- **확정/배제 증거**: GUID를 키로 사용하는 룩업 코드가 존재하는지 전체 grep으로 추가 확인. 현재 패스에서는 발견되지 않음.

### S2. `FStaticMaterial::operator<<` 저장 측의 정규화 부재 — **신뢰도: 높음**

- **위치**: `StaticMeshCommon.h:46-70`.
- **관찰된 내용**:
  ```cpp
  friend FArchive& operator<<(FArchive& Ar, FStaticMaterial& Mat)
  {
      Ar << Mat.MaterialSlotName;
      FString JsonPath;
      if (Ar.IsSaving() && Mat.MaterialInterface)
      {
          JsonPath = Mat.MaterialInterface->GetAssetPathFileName();
      }
      Ar << JsonPath;
      if (Ar.IsLoading())
      {
          if (!JsonPath.empty())
              Mat.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(JsonPath);
          else
              Mat.MaterialInterface = nullptr;
      }
      return Ar;
  }
  ```
  `Mat.MaterialInterface->GetAssetPathFileName()` (`Material.h:160`)는 단순 getter (`return PathFileName;`)이며 `SetAssetPathFileName` (`Material.h:162`)도 정규화 없음.
- **의심 사유**: `Material::PathFileName`의 현재 상태(어디서 setter를 호출했는가)에 따라 `JsonPath`가 정규화/비정규화, 절대/상대, forward/backslash로 들쭉날쭉 저장될 수 있다. 로드 측 `GetOrCreateMaterial`이 정규화를 한 번 수행해도(`MaterialManager.cpp:152`), 단방향 리매핑이라 회복 못 하는 케이스가 존재한다. 본 증상의 강력한 후보.
- **확정/배제 증거**: 실제 디스크의 `.uasset` 바이트를 헥스/문자열 덤프하여 저장된 `JsonPath`의 형식을 직접 관찰. `Material::SetAssetPathFileName` 모든 호출자를 grep해서 입력 형식 일관성 확인.

### S3. `USkeletonPoseAsset::TargetSkeletonPath`가 로드 시 자동 해석되지 않음 — **신뢰도: 높음**

- **위치**: `AssetData.cpp:152-176`.
- **관찰된 내용**:
  ```cpp
  void USkeletonPoseAsset::Serialize(FArchive &Ar)
  {
      UAssetData::Serialize(Ar);
      Ar << TargetSkeletonPath;
      Ar << Space;
      // ... BoneTransforms 직렬화 ...
  }
  ```
  `TargetSkeletonPath`는 `FString`으로 읽고 끝. 본 함수 안에서 해당 경로로 스켈레톤 에셋을 로드하는 호출이 없다.
- **의심 사유**: PoseAsset은 자기 자신을 로드한 직후 의존하는 Skeleton을 함께 가져오지 않는다. 외부 호출자가 PoseAsset의 `TargetSkeletonPath`를 보고 별도로 로드해야 하는데, 그 책임 분담이 모든 호출 경로에서 일관되게 지켜지는지 코드만으로는 확인되지 않는다. 누락 시 “경로는 있는데 의존이 해석되지 않은” 증상이 그대로 나타난다.
- **확정/배제 증거**: `TargetSkeletonPath` 참조처 grep으로 모든 사용자 목록화. `USkeletonPoseAsset` 로드 후 Skeleton을 적용하는 함수가 존재하는지 확인.

### S4. `UMaterial::Serialize` 텍스처 슬롯 저장의 정규화 부재 — **신뢰도: 높음**

- **위치**: `Material.cpp:309-415`, 특히 `:382-415`.
- **관찰된 내용** (저장/로드 본체):
  ```cpp
  // 저장
  for (auto& Pair : TextureParameters)
  {
      FString SlotName = Pair.first;
      FString TexturePath = Pair.second ? Pair.second->GetSourcePath() : FString();
      Ar << SlotName;
      Ar << TexturePath;
  }
  // 로드
  for (uint32 i = 0; i < TextureCount; ++i)
  {
      FString SlotName;
      FString TexturePath;
      Ar << SlotName;
      Ar << TexturePath;
      if (!TexturePath.empty())
      {
          ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
          UTexture2D* Loaded = UTexture2D::LoadFromFile(TexturePath, Device);
          if (Loaded) TextureParameters[SlotName] = Loaded;
      }
  }
  ```
- **의심 사유**: `UTexture2D::GetSourcePath()`의 반환값(`SourceFilePath`) 형식이 임포트 시 어떻게 설정되었느냐에 따라 저장 바이트가 달라진다. 또한 로드 측에서 `LoadFromFile(TexturePath, Device)` 직전 정규화가 명시적으로 보이지 않는다(내부 정규화는 추가 검증 필요). 정규화 비대칭이 발생하면 동일 텍스처가 캐시 키 미스를 일으키거나 디스크 룩업 실패 가능.
- **확정/배제 증거**: `UTexture2D::LoadFromFile` 내부에서 `FPaths::NormalizePath` 호출 여부 확인. 임포트 단계에서 `SourceFilePath` 세팅 코드의 정규화 유무 확인.

### S5. `UTexture2D::Serialize`에서 `SourceFilePath` 직판 — **신뢰도: 중간**

- **위치**: `Texture2D.cpp:1193-1225`.
- **관찰된 내용**:
  ```cpp
  void UTexture2D::Serialize(FArchive& Ar)
  {
      Ar << SourceFilePath;
      Ar << Width;
      Ar << Height;
      // ... 픽셀 데이터 ...
      if (Ar.IsLoading())
      {
          AssetFilePath.clear();
          CacheKeyPath.clear();
          SourceFileWriteTime = {};
          bHasSourceFileWriteTime = TryGetTextureWriteTime(SourceFilePath, SourceFileWriteTime);
      }
  }
  ```
- **의심 사유**: 로드 시 `SourceFilePath`가 저장 당시 절대경로(다른 머신의 디스크 경로) 그대로면, `TryGetTextureWriteTime`는 파일을 찾지 못해 `bHasSourceFileWriteTime=false`로 떨어진다. 그 자체는 즉시 실패가 아니지만, 이후 `LoadInternal`이나 캐시 비교에서 “원본 변경 감지 불능” 상태가 되어 잘못된 디스크 경로를 계속 사용할 수 있다.
- **확정/배제 증거**: `UTexture2D::LoadInternal`, `FindTextureSourceOnDisk`, `ResolveTexturePathOnDisk`가 로드 직후 `SourceFilePath`를 재해석하는지 확인. 변경 감지 분기에서만 재해석한다면 본 의심은 유효.

### S6. `.uasset` 파일 자체 경로의 절대화로 인한 캐시/로그 키 혼동 — **신뢰도: 중간**

- **위치**: `AssetFileSerializer.cpp:28-31`, `:134`, `:171`.
- **관찰된 내용**:
  ```cpp
  FString ToAssetPathString(const std::filesystem::path& FilePath)
  {
      return FPaths::ConvertRelativePathToFull(
          FPaths::NormalizePath(
              FPaths::ToUtf8(FilePath.lexically_normal().generic_wstring())));
  }
  ```
  → 디스크 I/O는 절대경로 기준으로 안전.
- **의심 사유**: 로그/외부 캐시(`MaterialCache`, `StaticMeshCache`)는 정규화된 *프로젝트 상대* 키를 사용하는 경향이 있는 반면(`MaterialManager.cpp:152, 175`), `AssetFileSerializer` 측 로그는 절대경로를 출력한다. 두 표현 사이의 일관성/매핑이 어긋날 경우, 동일 자산의 두 표현이 캐시에 따로 등록될 수 있다.
- **확정/배제 증거**: 동일 자산을 두 번 로드해 캐시 항목 수를 dump. 또는 `MaterialCache` 내용을 로깅해 키 중복 여부 확인.

### S7. `RemapLegacyAssetPath`의 단방향성 — **신뢰도: 낮음~중간**

- **위치**: `Paths.cpp:25-65` (익명 네임스페이스).
- **관찰된 내용**:
  ```cpp
  ReplaceAll(Result, L"/Asset/Game/Content/", L"/Asset/Content/");
  ReplaceAll(Result, L"/Asset/Engine/Content/", L"/Asset/Content/");
  // ... 등등, 역방향은 없음 ...
  ```
- **의심 사유**: 정규화는 한쪽 방향으로만 정의. 만약 누군가 이미 정규화 후 형식(`Asset/Content/...`)으로 저장한 자산을 별도 도구로 레거시 형식으로 되돌렸거나, 그 반대로 레거시 형식이 저장된 후 RootDir이 바뀌어 별도 매핑이 필요해진다면 회복 불가.
- **확정/배제 증거**: 실제 디스크의 `.uasset` 표본에서 `Asset/Game/`, `Asset/Engine/` 문자열이 등장하는지 확인. 등장한다면 본 의심은 활성화.

---

## 6. 사용자 확인 필요 사항 (Open Questions)

다음은 코드만으로는 결정할 수 없는 항목이며, 사람의 판단/추가 정보가 필요하다.

1. **누가 `UMaterial::SetAssetPathFileName`을 호출하며, 항상 정규화된 값이 들어가는가?**
   - `UNKNOWN — 사용자 확인 필요`
   - 모든 호출자를 grep해 입력 형식을 확인하면 S2의 결론이 확정/배제된다.

2. **실제 디스크에 저장된 `.uasset` 표본의 의존 경로 바이트는 어떤 형식인가? (절대/상대, separator, 레거시 폴더 명?)**
   - `UNKNOWN — 사용자 확인 필요`
   - 헥스 덤프 또는 작성된 임시 디버그 로깅으로 1회 확인되면 S2·S4·S5·S7 의 우선순위가 정해진다.

3. **증상 재현 시나리오 — 어떤 자산이 어떤 의존을 못 찾는가? (StaticMesh→Material? Material→Texture? PoseAsset→Skeleton?)**
   - `UNKNOWN — 사용자 확인 필요`
   - 실패 사례 유형을 알려주면 본 리스트의 의심 우선순위를 좁힐 수 있다.

---

## 7. 재현 검증 포인트 (Reproduction Hooks)

> 아래는 “여기에 진단용 로그/assertion을 추가하면 의심을 확정/배제할 수 있다”는 서술적 가이드이다. **현재 패스에서는 실제 코드 수정을 하지 않는다.**

- **`FAssetFileSerializer::SaveObjectToAssetFile` (`AssetFileSerializer.cpp:132-167`)**
  진입 직후, `AssetPath`(절대) / `RootObject->GetClass()->GetName()` / `Header.AssetGuid`를 함께 로그. 동일 자산 재저장 시 GUID 변경 여부를 한눈에 확인할 수 있다.

- **`FStaticMaterial::operator<<` (`StaticMeshCommon.h:46-70`)**
  `Ar.IsSaving()` 분기에서 `JsonPath`의 raw 바이트 16진 덤프 + 길이 출력. `Ar.IsLoading()` 분기에서 동일 정보를 출력. 이 두 라인이 본 증상의 1차 진단 데이터다. 추가로 `Mat.MaterialInterface->GetAssetPathFileName()` 호출 전에 nullptr 여부와 `MaterialSlotName`을 같이 출력.

- **`FMaterialManager::GetOrCreateMaterial` (`MaterialManager.cpp:150-208`)**
  L152 직후 `MaterialPath` → `GenericPath` 변환 결과 출력. L167 `ResolveMaterialAssetDiskPath` 반환값, 그리고 `std::filesystem::exists(DiskPath)` 결과를 함께 로그. L184-188 의 Warning 로그가 이미 있으나, *어떤 호출자에서 들어왔는지* 콜 스택 정보를 추가하면 S2 확정에 결정적이다.

- **`FMeshAssetManager::LoadStaticMeshAssetFile` (`MeshAssetManager.cpp` 내, 진입부)**
  입력 `AssetPath`, 정규화 후 `CacheKey`, 캐시 미스 여부, 디스크 absolute 경로를 한 줄로 출력. 호출자 식별을 위해 콜 스택 1~2 프레임 출력이 유용.

- **`FPaths::ResolvePathToDisk` (`Paths.cpp:200-260`)**
  L244 직전에 누적된 `Candidates` 목록 dump. 어떤 후보들이 시도됐고 어떤 것이 실제로 존재했는지가 단방향 리매핑 가설(S7) 확정에 직결.

- **`UTexture2D::Serialize` (`Texture2D.cpp:1193-1225`)**
  L1218-1224의 `Ar.IsLoading()` 분기에서 `SourceFilePath` 값과 `bHasSourceFileWriteTime` 결과를 함께 출력. false가 잦다면 S5의 신뢰도가 상승.

- **`USkeletonPoseAsset::Serialize` (`AssetData.cpp:152-176`)**
  L156 직후 `TargetSkeletonPath` 출력 + (`Ar.IsLoading()`일 때) 호출자가 이후 `TargetSkeletonPath` 기반으로 Skeleton을 로드하는지 외부 추적. 외부 호출 부재가 확인되면 S3 확정.

---

점검 완료 — 의심 지점 7건, 확인 필요 3건
