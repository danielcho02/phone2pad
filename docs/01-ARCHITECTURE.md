# 01 — ARCHITECTURE: 시스템 구조

## 1. 전체 데이터 흐름

```
┌─────────────── Android 폰 ───────────────┐
│  BlackPadActivity (검정 전체화면)          │
│    └─ MotionEvent → TouchFrame 직렬화      │
│  LocalSocketServer (TCP :38917)            │
└───────────────┬───────────────────────────┘
                │  USB (adb forward tcp:38917 tcp:38917)
┌───────────────▼─────────────── Windows PC ┐
│  pc/client  (user-mode 서비스, C++)        │
│    ├─ AdbManager: 장치 감지, forward 설정, │
│    │   폰 앱 자동 기동 (am start)          │
│    ├─ FrameReceiver: TCP 수신, 패킷 파싱   │
│    ├─ FrameRecorder: 트레이스 녹화 (.ppt)  │
│    └─ Sink (전략 패턴, Phase별 교체):       │
│        Phase A: MouseSink   (SendInput 상대좌표) │
│        Phase B: GestureSink (제스처 판별→단축키)  │
│        Phase C: PtpSink     (드라이버 IOCTL)      │
│  pc/driver  (KMDF 가상 HID, Phase C)       │
│    └─ VHF로 가상 PTP 장치 생성,            │
│       PtpSink가 보낸 보고서를 OS에 주입     │
└────────────────────────────────────────────┘
                │
        Windows 입력 스택 (WPT 제스처 엔진)
```

## 2. 컴포넌트 책임

### 2.1 Android 앱 (`android/`)
- **역할: 멍청한 센서.** 해석 없이 raw 멀티터치를 프레임 단위로 전송.
- `BlackPadActivity`
  - 전체화면, 검정 배경, 시스템 바 숨김 (immersive)
  - `window.attributes.screenBrightness = 0f` (백라이트 최소화, 터치는 살아있음)
  - `onTouchEvent`에서 모든 포인터의 (id, x, y, tip) 추출 → `TouchFrame`
  - `MotionEvent.ACTION_CANCEL` 처리 필수 (전화 수신 등) → 전체 contact lift 프레임 전송
- `PadSocketServer`
  - 폰이 **서버**, PC가 클라이언트 (adb forward 방향과 일치)
  - 단일 연결만 허용, 끊기면 재대기
- 폰에서 하지 않는 것: 제스처 판별, 좌표 스케일링, 스무딩

### 2.2 PC 클라이언트 (`pc/client/`)
- ADB 수명주기 관리: `adb devices` 폴링 → 연결 감지 시
  `adb forward tcp:38917 tcp:38917` + `adb shell am start -n <pkg>/.BlackPadActivity`
- `FrameReceiver`: 길이-프리픽스 패킷 스트림 파싱 (`02-PROTOCOL.md`)
- `Sink` 인터페이스: `void onFrame(const TouchFrame&)`
  - Phase 전환 = Sink 구현 교체. 수신/파싱 코드는 전 Phase 공유.
- **Phase B 제스처 합성** (`GestureRouter` → `MouseSink` / `GestureSink`):
  - 최상위 Sink는 `GestureRouter`. 한 터치 세션의 **peak 접촉 수**로 분기한다:
    1손가락은 `MouseSink`(Phase A 상대 마우스, 무수정), 2+손가락은 `GestureSink`.
  - 2+가 한 번이라도 관측되면 전손가락 lift까지 그 세션을 GestureSink로 고정한다
    (peak-latch: 세션 중 강등 없음). 진입 시 첫 멀티터치 프레임을 MouseSink에 1회
    전달해 이동 앵커를 리셋 → 2→1손가락 decay에서 커서 점프 방지.
  - **Multi-touch settle window**: 첫 2+ 접촉 후 일정 시간(기본 90ms, 또는 4접촉 관측 시
    즉시) 동안 손가락 수를 확정하지 않고 `maxActiveCount`만 추적한다. landing skew(3개 먼저
    닿고 4번째가 늦게 닿음)를 흡수해 4손가락이 3손가락으로 오인식되는 것을 막는다. window
    동안은 어떤 제스처도 commit하지 않고, 확정 시점에 centroid/거리 anchor를 리셋해 초기
    skew가 제스처 delta로 들어가지 않게 한다.
  - `GestureSink` 상태머신: `Idle → Settling → Tracking(2/3/4) → Committed/AltTabSelecting/Cancelled`.
    - 2손가락: 스크롤(`wheel`/`hwheel`), 탭=우클릭, 핀치=Ctrl+휠.
      스크롤 vs 핀치는 중심점 이동량 vs 손가락 간 거리 변화로 disambiguation 후 확정.
      (settle window 안에서 끝나는 빠른 2손가락 탭도 `peakCount`로 판정해 보존.)
    - 3손가락: ↑=Win+Tab, ↓=Win+D. **←/→는 인터랙티브 Alt+Tab**(`AltTabSelecting`):
      commit 시 Alt를 누른 채 진입 방향대로 Tab(오른쪽)/Shift+Tab(왼쪽) 1회로 전환 UI를
      띄우고, 손가락을 좌우로 움직이며 step마다 선택을 이동하다가, **전손가락 lift(=ACTION_CANCEL
      =disconnect emitLift) 시 Alt를 떼어 선택을 확정**한다. 이 상태에선 다른 제스처가 섞이지 않음.
    - 4손가락: ←/→=Ctrl+Win+←/→(가상 데스크탑, 방향키 extended-key flag), ↑/↓=3손가락 미러(옵션).
    - 모두 `SendInput` 주입. 폰은 여전히 raw 센서 — 제스처 판별은 전부 PC에서.
- 설정: Phase A/B 모두 CLI 플래그 + 기본값 구조체(`MouseSinkConfig`/`GestureConfig`).
  (`config.toml` 파서는 미도입 — 추후 과제.)

### 2.3 가상 HID 드라이버 (`pc/driver/`, Phase C)
- KMDF + **Virtual HID Framework (VHF)** — MS 권장 가상 HID 생성 경로
  (`VhfCreate` / `VhfReadReportSubmit`)
- 클라이언트와의 통신: 드라이버가 노출하는 디바이스 인터페이스에 IOCTL로
  PTP input report 전달
- HID Report Descriptor: `03-HID-PTP-SPEC.md` 참조
- 대안 경로(차단 시): kernel HID minidriver (vmulti 방식). 단 VHF 우선.

### 2.4 도구 (`pc/tools/`)
- `recorder`: FrameReceiver에 끼어들어 수신 프레임을 타임스탬프와 함께
  `.pptrace` 파일(JSON Lines)로 저장
- `replay`: `.pptrace`를 원래 타이밍으로 재생하여 Sink에 주입
  → **폰 없이 PC 쪽 전체 개발/회귀 테스트 가능** (테스트 하네스의 핵심)

## 3. 스레드/지연 설계

- 폰: UI 스레드에서 MotionEvent → lock-free queue → 송신 전용 스레드
- PC: 수신 스레드 → Sink 호출 (Sink는 블로킹 금지)
- 배치 금지: 프레임은 도착 즉시 처리. Nagle 비활성화 (`TCP_NODELAY`) 양쪽 필수.
- 목표 예산: 폰 캡처 1ms + USB/ADB ~3ms + 파싱 <1ms + 주입 <1ms ≈ p95 20ms 이내

## 4. 좌표계 규약

- **Phase A 기본은 landscape pad mode**(폰을 책상 위에 가로로 눕혀 노트북 터치패드처럼 사용).
- 폰: 픽셀 단위 절대좌표, **landscape Activity 좌표계 그대로 전송**
  (원점 = landscape 상태의 좌상단, X축 = 폰의 긴 가로축, Y축 = 폰의 짧은 세로축).
  폰은 회전/스케일/스무딩을 하지 않으며, 좌표 범위·방향 해석은 PC가 HELLO의
  `screenWidthPx`/`screenHeightPx`/`rotation`(실제 Activity 좌표계 기준)으로 수행한다.
- PTP 보고서: logical range는 디스크립터에 선언된 값으로 PC가 스케일링
- 물리 크기: HELLO에 폰의 xdpi/ydpi 포함 → 디스크립터의 physical extent 계산에 사용
  (WPT 제스처 엔진은 물리 크기 기반으로 이동 거리를 해석하므로 중요)
- Phase C에서 필요 시 디스크립터/물리 크기와 일관되게 좌표·방향 규약을 재정리한다.

## 5. 실패 모드 처리

| 상황 | 동작 |
|---|---|
| USB 분리 | PC: 가상 장치 제거(또는 idle), 폰: Activity 종료 |
| 소켓 끊김 | 전체 contact lift 보고서 1회 주입 (커서 멈춤 방지), 재연결 루프 |
| 프레임 손상 (magic 불일치) | 스트림 재동기화: 다음 magic byte까지 스킵, 카운터 로깅 |
| 폰 화면 회전 | HELLO 재전송으로 방향 갱신 |
