# AGENTS.md — phone2pad 프로젝트 에이전트 지침

이 파일은 Codex 및 모든 코딩 에이전트가 작업 전 반드시 읽어야 하는 루트 지침이다.
> 프로젝트 공식 이름은 `phone2pad`다.

## 1. 프로젝트 한 줄 정의

**USB-C로 연결된 Android 폰(검정 화면, UI 없음)을 Windows 데스크탑의
"노트북급 Precision Touchpad"로 동작시키는 시스템.**

목표는 마우스 에뮬레이션이 아니다. **Windows가 폰을 네이티브 트랙패드로 인식**하여
3손가락 스와이프(Task View), 4손가락 스와이프(가상 데스크탑 전환), 2손가락 스크롤,
핀치 줌 등 모든 OS 제스처가 **Windows 설정 > 터치패드 페이지에서 그대로 동작**해야 한다.

## 2. 필독 문서 (작업 순서대로)

| 문서 | 내용 | 언제 읽나 |
|---|---|---|
| `docs/00-OVERVIEW.md` | 목표, 비목표, 성공 기준 | 항상 |
| `docs/01-ARCHITECTURE.md` | 전체 시스템 구조, 컴포넌트 책임 | 항상 |
| `docs/02-PROTOCOL.md` | 폰↔PC 와이어 프로토콜 (binary) | 폰/PC 통신 코드 작업 시 |
| `docs/03-HID-PTP-SPEC.md` | Windows Precision Touchpad HID 요구사항 | PC 드라이버/HID 작업 시 |
| `docs/04-ROADMAP.md` | Phase별 마일스톤 + 수락 기준 | 작업 시작 시 현재 Phase 확인 |
| `docs/05-TESTING.md` | 테스트 하네스, 리플레이 시뮬레이터 | 모든 구현 작업 시 |
| `docs/06-WORKFLOW.md` | 레포 구조, 빌드, 커밋 규칙 | 항상 |

## 3. 핵심 설계 결정 (변경 금지 — 변경 필요 시 사용자에게 먼저 질문)

1. **연결은 USB 유선 (ADB port forward) 단일 경로.** Wi-Fi/BT는 비목표.
2. **제스처 해석은 OS에 위임한다.** PC 쪽에서 손가락 개수 세서 단축키 주입하는 방식(Phase B)은
   임시 fallback일 뿐, 최종 형태는 PTP HID 보고서를 통한 네이티브 인식(Phase C)이다.
3. **폰 앱은 입력 센서다.** 폰에서 제스처 판별 금지. raw 멀티터치 좌표만 전송한다.
4. **절대좌표 전송.** 폰 → PC로 폰 좌표계의 절대좌표를 보내고, 매핑/스케일링은 PC가 담당.
5. **프로토콜은 `docs/02-PROTOCOL.md`가 단일 진실 공급원(SSOT).**
   Kotlin/C++ 양쪽 구현은 `proto/test-vectors/`의 공유 테스트 벡터를 모두 통과해야 한다.

## 4. 에이전트 행동 규칙

- **물리 디바이스 없이 개발 가능해야 한다.** 모든 PC 쪽 기능은 `pc/tools/replay`
  시뮬레이터(녹화된 터치 트레이스 재생)로 테스트할 수 있어야 한다.
- 구현 전 해당 Phase의 수락 기준(`docs/04-ROADMAP.md`)을 확인하고,
  완료 시 수락 기준 체크리스트를 결과 보고에 포함하라.
- HID 디스크립터, WDF API 등 **OS 레벨 세부사항은 추측하지 말 것.**
  불확실하면 Microsoft Learn 공식 문서를 검색·확인하고 출처 URL을 코드 주석에 남겨라.
- 드라이버 코드는 테스트 서명(testsigning) 환경 전제. 사용자 머신 설정을 바꾸는
  명령(bcdedit 등)은 직접 실행하지 말고 README에 수동 절차로 문서화하라.
- 커밋 메시지·코드 주석은 영어, 사용자 대면 문서는 한국어.

## 5. 레포 구조

```
phone2pad/
├── CLAUDE.md                 ← 이 파일
├── docs/                     ← 설계 문서 (SSOT)
├── prompts/                  ← Phase별 Claude Code 시작 프롬프트
├── proto/
│   └── test-vectors/         ← 프로토콜 공유 테스트 벡터 (JSON)
├── android/                  ← Kotlin 폰 앱
├── pc/
│   ├── client/               ← C++ user-mode 서비스 (수신, 매핑, 주입)
│   ├── driver/               ← KMDF 가상 HID 드라이버 (Phase C)
│   └── tools/
│       ├── replay/           ← 터치 트레이스 리플레이 시뮬레이터
│       └── recorder/         ← 트레이스 녹화 도구
└── README.md
```

## 6. 현재 상태

- [x] Phase 0: 레포 부트스트랩, 프로토콜 라이브러리 + 테스트 벡터
- [x] Phase A: 폰 → PC 좌표 전송, 상대좌표 마우스 (SendInput) — 실기기 L4 검증만 잔여
- [~] Phase B: user-mode 제스처 (GestureRouter + GestureSink) — 스크롤/우클릭/핀치/
      3·4손가락 스와이프 구현 + L2/L3 자동 테스트 통과, 실기기 L4 검증 대기
- [ ] Phase C: PTP 가상 HID 드라이버 (네이티브 트랙패드 인식)
- [ ] Phase D: 폴리시 — 팜 리젝션, 햅틱, 전력 최적화

작업 시작 전 이 체크리스트를 갱신하고, Phase 완료 시 체크 표시 후 커밋하라.
