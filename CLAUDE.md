# CLAUDE.md — phone2pad 프로젝트 에이전트 지침

> 이 파일은 Claude Code 및 모든 코딩 에이전트가 작업 전 반드시 읽어야 하는 루트 지침이다.
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
- [~] Phase A: 폰 → PC 좌표 전송, 상대좌표 마우스 (SendInput)
      - 구현 완료: Android :app(landscape BlackPadActivity + PadSocketServer),
        PC client(AdbManager/FrameReceiver/MouseSink), replay/recorder 도구
      - 자동 테스트 통과: proto(L1) + client/replay(L2/L3), :app:assembleDebug
      - 실기기 검증(L4) 대기: 콜드스타트·탭/더블탭·RTT p95<5ms·표준 트레이스 5종 녹화
- [~] Phase B: user-mode 제스처 판별 + 단축키 주입 (사용 가능한 MVP)
      - 구현 완료: GestureRouter(1손가락→MouseSink, 2+→GestureSink, peak-latch),
        GestureSink(2손가락 스크롤/우클릭/핀치, 3·4손가락 스와이프 → SendInput),
        InputInjector 확장(wheel/hwheel/right/key), replay `--sink gesture`
      - 클릭/드래그 개선: MouseSink long-press 드래그(Idle→OneFingerDown→
        Moving/DragArmed→Dragging, lift/멀티터치/disconnect 시 leftUp 보장),
        2손가락 우클릭 안정화(tap 한도 완화 + landing/lift skew 가드)
      - 자동 테스트 통과: gesture_sink/gesture_router L2 + 합성 fixture L3 (오발동 0),
        mouse_sink 드래그 케이스 추가, 기존 Phase A·proto 회귀 유지
      - 실기기 검증(L4) 대기: 실사용 자연스러움, 3손가락 Alt+Tab 신뢰성 튜닝,
        long-press 드래그 체감, 2손가락 우클릭 신뢰성
- [ ] Phase C: PTP 가상 HID 드라이버 (네이티브 트랙패드 인식) — 보류
- [~] Phase D: distribution + 폴리시
      - [~] D-1 distribution (배포 패키징): USB/ADB MVP를 배포 형태로 정리
        - 구현 완료: MainActivity 런처(USB 안내 + [트랙패드 모드 시작], BlackPadActivity는
          landscape 패드 전용 유지·exported=true로 adb am start 수동 경로 보존),
          공식 흐름은 수동 시작(client는 auto-launch 안 함, forward만 설정 후 연결 대기),
          versionName 0.2.0 + release 서명 설정(keystore.properties 기반, 키 커밋 금지),
          PC 정적 CRT(/MT) self-contained zip, scripts/package-release.ps1(zip/APK·AAB/SHA256SUMS),
          QUICKSTART/RELEASE/CHANGELOG/PRIVACY/LICENSE(MIT)
        - 자동 검증: test-all.ps1 회귀 유지, package-release dry-run으로 self-contained
          zip 생성(client는 시스템 DLL만 의존), keystore 없으면 Android asset SKIP
        - 대기(사용자): GitHub Release v0.2.0 게시, Google Play Internal testing 업로드
      - [~] v0.3.0 실행 마찰 감소 UX (트레이 companion) — D-2 "연결 UX(트레이 아이콘)" 충족
        - 구현 완료: `phone2pad_tray.exe`(네이티브 Win32, GUI 서브시스템·콘솔 없음) —
          Shell_NotifyIcon + 컨텍스트 메뉴(상태/Start·Stop/Open setup guide/Start with
          Windows/Exit), 단일 인스턴스(named mutex), 상태 풍선(Idle/Waiting for phone/
          Waiting for app/Connected). 자동 시작은 `HKCU\...\Run`(관리자 불필요).
        - 공유 엔진 `ClientService`(client_core): CLI/트레이가 동일 파이프라인 재사용.
          유휴 저전력 tiered 루프(monitor→arm→active) — 보수적 폴링, Connected에서만 입력
          파이프라인 활성. `net_client`에 onFirstData 콜백 추가(세션 시작 즉시 Connected 표시).
        - L4 핫픽스: 트레이(GUI) 모드에서 adb 폴링이 콘솔 창을 띄워 창 깜빡임·키보드 포커스
          탈취를 일으키던 문제 수정 — `process_runner`(CreateProcessW + CREATE_NO_WINDOW +
          SW_HIDE + 파이프 캡처 + ~10s 타임아웃)로 AdbManager의 `_popen`/`std::system` 교체.
        - L4 UX: phone2pad 로고를 exe/트레이/풍선 아이콘으로 임베드(`tray.rc` +
          `assets/phone2pad.ico`, `scripts/make-icon.ps1`로 생성), 트레이 문구를 한국어로
          개선(툴팁 항상 `phone2pad - …`, 상태/메뉴/풍선 한국어), 숨겨진 아이콘(^) 영역 안내를
          README/QUICKSTART/windows-README에 추가. 트레이 Korean 리터럴용 `/utf-8` 적용.
        - L4 아이콘 폴리시: 트레이/exe 아이콘을 투명 배경 + 밝은 외곽선으로 재생성(다크
          작업표시줄 가시성, 흰 사각형 해결). Android 런처 아이콘 추가(adaptive + 레거시 PNG,
          매니페스트 `icon`/`roundIcon` 연결) — `scripts/make-android-icons.ps1`.
        - 콘솔 `phone2pad_client.exe`는 동일 동작 대체 경로로 유지(ClientService 위로 재작성).
        - 자동 테스트 통과: client_service(상태/shortLabel/describe·start/stop) + autostart
          포맷팅 단위 테스트 추가, 기존 회귀 유지(76 cases, 0 failures).
        - 배포: package-release가 트레이 exe + QUICKSTART.md 동봉, 트레이를 권장 진입점으로
          안내(README/QUICKSTART). Android versionCode 5 / versionName 0.3.0(동작 무변경).
        - 실기기 검증(L4) 대기: 트레이 상태 전환 체감, 자동 시작 재부팅 검증, 유휴 CPU 확인.
      - [ ] D-2 폴리시 — 팜 리젝션, 햅틱, 전력 최적화 (이후)

작업 시작 전 이 체크리스트를 갱신하고, Phase 완료 시 체크 표시 후 커밋하라.
