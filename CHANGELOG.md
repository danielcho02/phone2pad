# Changelog

이 프로젝트의 주요 변경 사항을 기록합니다.
형식은 [Keep a Changelog](https://keepachangelog.com/ko/1.1.0/)를 따르며,
버전은 [Semantic Versioning](https://semver.org/lang/ko/)을 따릅니다.

## [Unreleased]

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

[Unreleased]: https://github.com/danielcho02/phone2pad/compare/v0.2.2...HEAD
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
