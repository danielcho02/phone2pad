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
- `Sink` 인터페이스: `void consume(const TouchFrame&)`
  - Phase 전환 = Sink 구현 교체. 수신/파싱 코드는 전 Phase 공유.
- 설정 파일(`config.toml`): 포인터 속도, 스크롤 방향, 패키지명 등

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

- 폰: 픽셀 단위 절대좌표, 원점 좌상단, **세로 모드 기준 그대로 전송**
  (가로 회전 변환은 PC가 HELLO의 해상도 + 방향 필드로 수행)
- PTP 보고서: logical range는 디스크립터에 선언된 값으로 PC가 스케일링
- 물리 크기: HELLO에 폰의 xdpi/ydpi 포함 → 디스크립터의 physical extent 계산에 사용
  (WPT 제스처 엔진은 물리 크기 기반으로 이동 거리를 해석하므로 중요)

## 5. 실패 모드 처리

| 상황 | 동작 |
|---|---|
| USB 분리 | PC: 가상 장치 제거(또는 idle), 폰: Activity 종료 |
| 소켓 끊김 | 전체 contact lift 보고서 1회 주입 (커서 멈춤 방지), 재연결 루프 |
| 프레임 손상 (magic 불일치) | 스트림 재동기화: 다음 magic byte까지 스킵, 카운터 로깅 |
| 폰 화면 회전 | HELLO 재전송으로 방향 갱신 |
