# 06 — WORKFLOW: 개발 환경, 빌드, 에이전트 운용 규칙

## 1. 개발 환경

| 영역 | 도구 |
|---|---|
| Android | Android Studio 또는 CLI Gradle, Kotlin, minSdk 28 (Android 9) |
| PC client | Visual Studio 2022 / clang-cl, CMake ≥ 3.25, C++20 |
| PC driver | WDK (VS 연동), KMDF, testsigning 머신 |
| 공통 | Git, PowerShell 스크립트 (`scripts/`) |

빌드 명령 (루트에서):

```powershell
# PC
cmake -B pc/build -S pc && cmake --build pc/build --config Debug
# 테스트
pc/build/Debug/proto_tests.exe
pc/build/Debug/gesture_tests.exe
# Android
cd android && ./gradlew assembleDebug test
# 전체
./scripts/test-all.ps1
```

## 2. Git 규칙

- 브랜치: `phase-a/<topic>`, `phase-c/<topic>`
- 커밋: Conventional Commits (`feat(android): ...`, `fix(driver): ...`, `test(proto): ...`)
- Phase 완료 = 태그 (`v0.1-phase-a`)
- 드라이버 바이너리/서명 파일은 커밋 금지 (`.gitignore`)

## 3. 코딩 에이전트 운용 (Claude Code)

### 세션 시작 절차
1. `CLAUDE.md` 읽기 (자동)
2. 현재 Phase 확인 (`CLAUDE.md` §6 체크리스트)
3. 해당 Phase 프롬프트(`prompts/phase-*.md`)의 컨텍스트로 작업

### 작업 단위 규칙
- 한 세션 = 한 마일스톤 (예: "C-2 PTPHQA feature 응답"). 여러 Phase를 한 번에 진행 금지.
- 구현 → 테스트 작성 → `scripts/test-all.ps1` 통과 → 수락 기준 체크 → 커밋 순서.
- OS API(WDF/VHF/SendInput/HID)는 추측 금지. Microsoft Learn에서 시그니처 확인 후
  출처 URL을 주석으로 남긴다.
- 실기기 필요한 검증(L4)은 에이전트가 하지 않는다. "사용자 확인 필요" 목록으로
  세션 말미에 정리해 보고한다.

### 에이전트가 직접 실행하면 안 되는 것
- `bcdedit`, 드라이버 설치/제거 (`pnputil`) — 절차만 문서화
- 폰 관련 adb 명령 중 설정 변경류 — 사용자 승인 후

## 4. 디렉토리별 소유권

```
proto/      프로토콜 변경은 docs/02 수정과 반드시 동일 커밋
android/    Kotlin. 폰 = 센서 원칙 위반 코드(제스처 로직) 금지
pc/client/  Sink 전략 패턴 유지. 수신 경로에 Phase 분기 if문 금지
pc/driver/  Phase C 전까지 비움
pc/tools/   replay/recorder는 client의 파싱 코드를 라이브러리로 재사용
```

## 5. 문서 갱신 규칙

- 설계 변경 → 해당 `docs/*.md` 먼저 수정 → 구현 (docs-first)
- 디버깅 중 알게 된 OS 동작 특이사항은 `docs/notes/` 에 짧은 메모로 축적
  (예: `notes/2026-06-vhf-feature-report-quirk.md`) — 다음 세션의 에이전트가 읽는다
