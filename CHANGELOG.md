# Changelog

이 프로젝트의 주요 변경 사항을 기록합니다.
형식은 [Keep a Changelog](https://keepachangelog.com/ko/1.1.0/)를 따르며,
버전은 [Semantic Versioning](https://semver.org/lang/ko/)을 따릅니다.

## [Unreleased]

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

[Unreleased]: https://github.com/danielcho02/phone2pad/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/danielcho02/phone2pad/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/danielcho02/phone2pad/releases/tag/v0.2.0
