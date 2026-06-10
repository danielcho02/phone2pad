# PhantomPad

> USB-C로 연결한 Android 폰을 Windows 데스크탑의 **네이티브 Precision Touchpad**로
> 만드는 프로젝트. 폰 화면은 완전 검정(UI 없음), 제스처 해석은 Windows OS가 직접 수행.

기존 "폰을 마우스로" 앱들과의 차이: 마우스 에뮬레이션이 아니라 Windows가 장치를
**트랙패드로 인식**하므로, 3손가락 Task View / 4손가락 가상 데스크탑 전환 등
노트북과 동일한 OS 제스처가 설정 > 터치패드 페이지에서 그대로 동작한다.

## 문서 인덱스

| 경로 | 내용 |
|---|---|
| `CLAUDE.md` | 코딩 에이전트 루트 지침 (작업 전 필독) |
| `docs/00-OVERVIEW.md` | 목표, 비목표, 성공 기준 |
| `docs/01-ARCHITECTURE.md` | 시스템 구조 |
| `docs/02-PROTOCOL.md` | 폰↔PC 와이어 프로토콜 (SSOT) |
| `docs/03-HID-PTP-SPEC.md` | Windows PTP HID 요구사항 |
| `docs/04-ROADMAP.md` | Phase 계획 + 수락 기준 |
| `docs/05-TESTING.md` | 테스트 하네스 (폰 없는 개발 루프) |
| `docs/06-WORKFLOW.md` | 빌드, Git, 에이전트 운용 규칙 |
| `prompts/` | Phase별 Claude Code 시작 프롬프트 |

## 빠른 시작 (개발)

1. 이 폴더를 Git 레포로 초기화: `git init && git add . && git commit -m "docs: bootstrap"`
2. Claude Code 실행 후 `prompts/phase-0A-bootstrap.md` 내용을 붙여넣기
3. Phase A 완료 후 실기기 수락 테스트 → `prompts/phase-B-gestures.md` → `phase-C-driver.md`

## 사전 준비 (사용자 수동)

- 폰: 개발자 옵션 → USB 디버깅 ON, PC RSA 키 승인
- PC: adb (platform-tools) PATH 등록
- Phase C부터: WDK 설치, `bcdedit /set testsigning on` 후 재부팅

## 상태

Phase 0 시작 전 — 설계 문서 완료 단계.
