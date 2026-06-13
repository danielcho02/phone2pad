# Changelog

이 프로젝트의 주요 변경 사항을 기록합니다.
형식은 [Keep a Changelog](https://keepachangelog.com/ko/1.1.0/)를 따르며,
버전은 [Semantic Versioning](https://semver.org/lang/ko/)을 따릅니다.

## [Unreleased]

### Added
- **PC**: 트레이 메뉴에 **platform-tools 폴더 선택** 추가(adb 미탐색 시에만 표시) — Windows
  폴더 선택 창에서 압축을 푼 `platform-tools` 폴더를 고르면, 폴더 안 `adb.exe`를 검증해
  경로를 `%LOCALAPPDATA%\phone2pad\config.json`에 저장하고 자동으로 다시 확인합니다.
  파일을 옮길 필요·PATH 변경·관리자 권한이 없습니다. 잘못된 폴더는 한국어 오류
  ("선택한 폴더에서 adb.exe를 찾을 수 없습니다. platform-tools 폴더를 선택해 주세요.")를
  띄웁니다. 선택한 경로는 **adb 탐색 순서의 맨 앞**(PATH보다 우선)에서 사용됩니다.
- **PC(배포)**: **일반 사용자용 Windows 설치 프로그램**(`phone2pad-setup-vX.Y.Z.exe`,
  Inno Setup) 추가 — **관리자 권한 불필요(사용자별 설치)**, **시작 메뉴 `phone2pad`
  바로 가기**와 Windows **"앱 및 기능" 제거 항목** 생성, 설치 직후 실행 옵션 제공.
  포터블 zip과 **동일한 실행 파일/문서**를 담으며(zip 패키징은 그대로 유지),
  **adb는 번들하지 않고 자동 다운로드/PATH 변경도 하지 않음** — adb는 기존 트레이
  first-run 설정 게이트가 그대로 담당. `scripts/installer/phone2pad.iss` +
  `scripts/package-release.ps1`에 빌드 단계와 **무인 설치→파일/바로 가기 검증→무인 제거**
  스모크 테스트 추가(ISCC 부재 시 SKIPPED, 단 v0.3.0 릴리스는 PASS 필요).
- **PC**: **ADB 설치 first-run 설정 게이트** — adb가 없으면 트레이가 처음 그 상태가 될 때
  한국어 안내 창(MessageBox)을 한 번 띄워 비개발자도 따라 할 수 있는 가장 쉬운 방법
  (platform-tools 폴더를 `phone2pad_tray.exe` 옆에 두기)을 안내합니다. GitHub·PATH 지식이
  필요 없습니다.
- **PC**: 트레이 메뉴에 adb 미탐색 시에만 나타나는 항목 추가 — **ADB 설치 페이지 열기**
  (공식 Android SDK Platform-Tools 페이지만 엽니다. 자동 다운로드·설치·PATH 변경·관리자
  권한 없음), **ADB 다시 확인**(트레이를 다시 실행하지 않고 adb 재탐색). 기존
  **ADB 설치 안내 열기**(동봉 `ADB-SETUP.md`)는 보조 항목으로 유지.
- **PC(engine)**: `AdbManager::recheck()` / `ClientService::restart()` 추가 — 워커를 멈추고
  adb를 재탐색한 뒤 다시 시작(동기, 경쟁 없음, 고아 프로세스 없음). 트레이 **ADB 다시 확인**이
  이를 사용하며, 찾으면 **폰 연결 대기**로 전환하고 못 찾으면 폴더 위치 확인 알림을 띄웁니다.

### Changed
- **PC**: 트레이 상태 문구를 상황별로 분리해 "할 일"이 보이도록 개선 — `ADB 설치 필요` /
  `폰 연결 대기` / `폰에서 USB 디버깅 허용 필요` / `폰 인식 중…` / `폰이 여러 대 연결됨` /
  `USB 연결 실패 - 케이블 재연결` / `폰 앱에서 트랙패드 모드 시작 필요` /
  `연결됨 - 트랙패드 사용 중`. (이전에는 여러 상태가 `폰 연결 대기` 하나로 합쳐졌습니다.)
- **PC**: adb 미탐색 풍선 문구를 "phone2pad를 사용하려면 Android Platform Tools의 adb가
  필요합니다. 설치 후 다시 확인하세요."로 명확화. adb first-run 안내 창과
  QUICKSTART/README/windows-README/ADB-SETUP을 실제 첫 실행 흐름(트레이 안내 → 설치 페이지
  → **platform-tools 폴더 선택** → 다시 확인)에 맞춰 정리하고, exe 옆 두기·PATH 등록은
  고급 방법으로 강등. **최신 공식 릴리스는 v0.3.0 게시 전까지 v0.2.2**이며 adb는 **이
  버전에서 번들하지 않음**을 명시.
- **PC**: adb 앱 로컬 탐색 위치를 `tools\platform-tools` → `platform-tools`(실행 파일 옆)로
  단순화. 즉 platform-tools zip을 `phone2pad_tray.exe` 옆에 그대로 풀면 됩니다
  (`…\platform-tools\adb.exe`). PATH·Android SDK 탐색 순서는 그대로입니다.

## [0.3.0] - 2026-06-12

실행 마찰을 줄이는 UX 릴리스. 매번 콘솔 `phone2pad_client.exe`를 띄우는 대신, 백그라운드에
상주하는 가벼운 Windows 트레이 companion(`phone2pad_tray.exe`)을 추가했습니다. 프로토콜,
Android 터치 인코딩·제스처 동작은 변경되지 않았으며 USB/ADB 전용 경로를 그대로 유지합니다.
(Wi-Fi/Bluetooth/Precision Touchpad 드라이버/스타일러스는 범위 외)

### Added
- **PC**: `phone2pad_tray.exe` — 네이티브 Win32 트레이 companion(GUI 서브시스템, 콘솔 창 없음).
  - 트레이 메뉴(한국어): 상태 헤더, 서비스 시작/정지, 사용 방법 열기, Windows 시작 시 자동 실행
    (토글·체크 표시), 종료. 단일 인스턴스(named mutex)로 중복 실행 방지.
  - 전용 아이콘 — phone2pad 로고를 exe/트레이/풍선 아이콘으로 임베드(`tray.rc` +
    `assets/phone2pad.ico`, 16/32/48/256). **투명 배경 + 은은한 밝은 외곽선**으로 다크
    작업표시줄에서도 또렷하게 보이며(흰 사각형으로 보이던 문제 해결), 숨겨진 아이콘(^)
    영역에서 식별이 쉬움. (`scripts/make-icon.ps1`로 `logo.png`에서 생성)
- **Android**: 런처 아이콘 추가 — phone2pad 로고로 적응형(adaptive) 아이콘(API 26+,
  `mipmap-anydpi-v26` + 흰색 배경 레이어)과 레거시 밀도별 PNG(mdpi~xxxhdpi) 생성, 매니페스트
  `icon`/`roundIcon` 연결(기존 시스템 스톡 아이콘 대체). (`scripts/make-android-icons.ps1`)
  - 상태 가시화(한국어) — 툴팁은 항상 `phone2pad - …`로 시작하며 **대기 중 / 폰 연결 대기 /
    앱 시작 대기 / 연결됨 - 트랙패드 사용 중**(및 ADB 미탐색)을 표시. 풍선 알림 문구도 한국어.
  - 로그인 시 자동 시작 — 사용자별 `HKCU\...\Run` 레지스트리 값(관리자 권한 불필요)으로 토글.
- **PC**: `ClientService` — CLI와 트레이가 공유하는 헤드리스 엔진. 기존 파이프라인
  (AdbManager → FrameReceiver → OrientationNormalizingSink → GestureRouter →
  MouseSink/GestureSink → Win32InputInjector)을 그대로 재사용.
- **PC**: `ClientService` 상태 매핑·`shortLabel`/`describe`·start/stop 수명주기,
  자동 시작 명령 포맷팅에 대한 단위 테스트.

### Fixed
- **PC**: 트레이(GUI) 모드에서 adb 폴링이 콘솔 창을 띄워 **창이 깜빡이고 키보드 포커스를
  빼앗던** 문제 수정. 모든 adb 호출을 `CreateProcessW` + `CREATE_NO_WINDOW` +
  숨김 `STARTUPINFO`(STARTF_USESHOWWINDOW/SW_HIDE) + 파이프 캡처로 실행하는 숨김 프로세스
  러너로 교체(`_popen`/`std::system`·`cmd.exe` 경유 제거). 멈춘 adb 자식 프로세스가
  서비스를 무한정 붙잡지 않도록 보수적 타임아웃(기본 10초, 초과 시 종료)을 추가.

### Changed
- **PC**: 유휴 시 저전력 동작 — 디바이스/앱 세션이 없을 때는 전체 입력 파이프라인을 돌리지
  않고 보수적 주기로만 폴링(타이트 루프·공격적 재연결 없음). 전체 프레임/입력 파이프라인은
  **Connected** 상태에서만 활성화됩니다.
- **PC**: `phone2pad_client.exe`(콘솔)는 동일 동작을 유지하는 대체 경로로 남고, 배포 zip은
  트레이를 권장 진입점으로 안내(README/QUICKSTART). `package-release.ps1`이 트레이 exe와
  `QUICKSTART.md`를 함께 동봉.
- **Android**: 동작 변경 없이 릴리스 메타데이터를 `versionCode 5` / `versionName "0.3.0"`로 올림.

[0.3.0]: https://github.com/danielcho02/phone2pad/compare/v0.2.2...v0.3.0

## [0.2.2] - 2026-06-12

adb 탐색·진단·안내를 개선하고, 자연 스크롤 방향·가로/역가로 방향 정규화·패드 모드
시스템 UI 동작을 다듬은 안정화/핫픽스 릴리스. 제스처 판별 로직·임계값·프로토콜은
변경되지 않았습니다. (USB/ADB 전용 유지)

### Added
- **PC**: adb 자동 탐색 — PATH, `%LOCALAPPDATA%\Android\Sdk\platform-tools`,
  실행 폴더/exe 옆의 `tools\platform-tools`(앱 로컬 도구 디렉터리)를 순서대로 탐색.
- **PC**: adb를 못 찾으면 공식 다운로드 주소와 두 가지 설치 방법(PATH 설치 / `tools\platform-tools`에 풀기)을
  안내하고 종료.
- **PC**: 디바이스 상태 진단 메시지 — 미연결 / 미인증(unauthorized) / 오프라인 /
  다중 디바이스 / adb forward 실패 / 앱 미실행을 사람이 읽기 쉬운 문장으로 안내.
- **PC**: 시작 시 client 동작·adb 위치·대기 상태를 알려주는 콘솔 출력.
- **PC**: adb 경로 탐색 순서, `adb devices` 파싱, 디바이스 분류에 대한 단위 테스트.
- **Android**: 가로(landscape) / 역가로(reverse-landscape) 방향 정규화 — 폰을 어느 쪽으로
  눕혀도 일관된 패드 좌표계를 유지.
- **Android**: 패드 모드 진입 시 짧은 종료 안내(how-to-exit) 힌트와, 시스템 UI가 보일 때
  사용자가 트랙패드를 벗어났음을 알 수 있도록 시스템 UI 밝기를 살짝 올림.

### Fixed
- **PC**: 2손가락 자연 스크롤 방향이 사용자 기대와 반대로 동작하던 문제 수정.

### Changed
- **Android**: BlackPadActivity의 전체화면 / 시스템 UI 처리 동작을 개선
  (몰입형 표시·힌트·밝기 전환 정리).
- **Android**: 릴리스 메타데이터를 `versionCode 4` / `versionName "0.2.2"`로 올림.

[Unreleased]: https://github.com/danielcho02/phone2pad/compare/v0.3.0...HEAD
[0.2.2]: https://github.com/danielcho02/phone2pad/compare/v0.2.1...v0.2.2

## [0.2.1] - 2026-06-11

Windows client 제스처 매핑 핫픽스. 제스처 판별 로직·임계값·프로토콜·Android UI는
변경되지 않았습니다.

### Fixed
- **PC**: 네 손가락 좌우 스와이프의 가상 데스크탑 전환 방향이 반대로 동작하던 문제 수정.
  이제 스와이프 방향과 데스크탑 이동 방향이 사용자 기대와 일치합니다
  (Windows client gesture mapping hotfix).

## [0.2.0] - 2026-06-11

USB/ADB 기반 user-mode MVP의 첫 배포 버전. Windows가 폰을 네이티브 트랙패드로
인식하는 Phase C(PTP HID 드라이버)는 아직 포함되지 않습니다.

### Added
- **Android**: 앱 직접 실행 시 USB 사용법 안내 + [트랙패드 모드 시작] 버튼을 보여주는 `MainActivity`.
- **Android**: release 서명 설정(`keystore.properties` 기반), `versionName 0.2.0`.
- **PC**: 배포 패키징 스크립트 `scripts/package-release.ps1`
  (Release 빌드 → self-contained zip → SHA256SUMS).
- 문서: `QUICKSTART.md`, `RELEASE.md`, `PRIVACY.md`, `CHANGELOG.md`, `LICENSE`(MIT).

### Changed
- **PC**: MSVC C 런타임을 정적 링크(`/MT`)하여 Windows zip을 self-contained로 배포
  (VC++ 재배포 패키지 불필요).
- **PC**: client가 폰 패드 모드를 **자동 실행하지 않음**. 공식 흐름은 수동 시작:
  client는 adb forward만 설정하고 연결을 대기하며, 사용자가 폰 앱에서
  [트랙패드 모드 시작]을 눌러야 연결됩니다. (auto-launch는 추후 polish 단계에서 재검토)
- **Android**: 런처가 `BlackPadActivity`(검정 패드)에서 `MainActivity`로 변경.
  `BlackPadActivity`는 landscape 패드 모드 전용으로 유지되며 `exported=true`이므로,
  개발자는 `adb am start -n com.phone2pad/.BlackPadActivity`로 수동 기동할 수 있습니다.

### 기존 기능 (Phase A/B, 0.2.0에 포함)
- 1손가락 이동/탭/길게 눌러 드래그 (MouseSink).
- 2손가락 스크롤/우클릭/핀치, 3손가락 Task View·바탕화면·Alt+Tab,
  4손가락 가상 데스크탑 전환 (GestureSink, user-mode `SendInput`).
- 폰↔PC 바이너리 프로토콜, 트레이스 녹화/재생 도구(recorder/replay).

[0.2.1]: https://github.com/danielcho02/phone2pad/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/danielcho02/phone2pad/releases/tag/v0.2.0
