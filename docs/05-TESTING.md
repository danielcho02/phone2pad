# 05 — TESTING: 테스트 하네스 설계

원칙: **물리 폰 없이 PC 쪽 전체를 개발·회귀 테스트할 수 있어야 한다.**
에이전트(Claude Code)는 폰을 만질 수 없으므로, 이 하네스가 에이전트 개발 루프의 핵심이다.

## 1. 계층별 테스트

```
L1  프로토콜 단위 테스트      (폰/PC 둘 다, 테스트 벡터 기반)
L2  제스처/변환 단위 테스트    (PC, 트레이스 → 기대 출력)
L3  리플레이 통합 테스트       (PC, 트레이스 → Sink → 실제 입력 주입)
L4  수동 E2E 체크리스트        (실기기, Phase 수락 시에만)
```

## 2. L1 — 프로토콜 테스트

- 입력: `proto/test-vectors/*.json`
- Kotlin: JUnit, `PacketCodecTest` — encode/decode round-trip
- C++: 단일 헤더 테스트 프레임워크(doctest 권장), 동일 벡터 로드
- 추가 케이스: 부분 수신(스트림 분할), magic 손상 후 재동기화, length 오버플로

## 3. L2 — 트레이스 기반 제스처 테스트 (Phase B 핵심)

`.pptrace` 포맷 (JSON Lines, 사람이 읽고 손으로 만들 수 있어야 함):

```jsonl
{"t": 0,     "contacts": [{"id":0,"tip":1,"x":500,"y":1200}]}
{"t": 8333,  "contacts": [{"id":0,"tip":1,"x":510,"y":1198}]}
{"t": 16666, "contacts": [{"id":0,"tip":0,"x":510,"y":1198}]}
```

테스트 형식: `trace + expected actions`

```json
{
  "trace": "traces/three_finger_swipe_up.pptrace",
  "expect": [{"action": "key_combo", "keys": ["LWIN", "TAB"]}]
}
```

GestureSink를 mock injector에 연결해 기대 액션 시퀀스를 검증한다.
**오발동 테스트가 더 중요**: 2손가락 스크롤 트레이스에서 3손가락 제스처가
발동하면 실패. 모든 트레이스는 모든 제스처에 대해 negative 검증도 수행.

구현 현황 (Phase B):
- `MockInjector`(`pc/client/tests/mock_injector.hpp`)가 wheel/hwheel/우클릭/키 콤보를
  이벤트 로그로 기록하고, `gesture_sink_test.cpp`가 직접 만든 프레임 시퀀스로
  positive/negative를 검증한다 (스크롤 ↔ 우클릭/스와이프 상호 배제, peak-latch,
  핀치 토글, 스와이프 1회 발동 등).
- `gesture_router_test.cpp`는 라우터 회귀를 검증한다: 1손가락 세션이 MouseSink 단독과
  동일, 2→1 decay tail이 커서로 새지 않음(점프 0), gesture 세션 중 MouseSink 비호출.
- 제스처 테스트는 별도 JSON expect 하네스 대신 C++ TEST_CASE에서 이벤트 로그를 단언한다
  (기존 Phase A 테스트 패턴과 일관).

L4 fix 반영 (인터랙티브 Alt+Tab / settle window):
- `gesture_sink_test.cpp`에 인터랙티브 Alt+Tab 검증 추가 — 오른쪽: Alt down → Tab step,
  왼쪽: Shift+Tab(첫 step 방향 반영), **lift 전 Alt up 없음** / lift 시 Alt up 1회,
  Shift 잔류 없음(down 수 == up 수). lift==ACTION_CANCEL==disconnect(`emitLift`) 경로 동치.
- settle window 검증 — 3→4 landing skew(50ms 늦은 4번째)가 4손가락 desktop switch로
  확정되고 3손가락(Alt+Tab) 오발동 0; 4→3 decay tail에서도 3손가락 미발동(negative).
- settle window(기본 90ms) 도입으로 멀티터치 fixture/단위테스트는 초기 hold를 두어 타임라인을
  ~300ms로 연장했다(빠른 2손가락 탭만 settle 안에서 종료). 신규 fixture
  `three_finger_alt_tab_right.pptrace`(Alt hold + Tab step), `four_finger_swipe_left`는
  landing-skew 형태로 갱신.

## 4. L3 — 리플레이 통합

- `pc/tools/replay --trace x.pptrace --sink mouse|gesture|ptp --speed 1.0`
- 원본 타임스탬프 간격을 보존해 재생 (지연 특성까지 재현)
- `--sink ptp`는 실제 드라이버에 주입 → 화면에서 OS 제스처가 실제 발동
  (Phase C 회귀 테스트: 트레이스 셋 재생하며 사람이 1회 확인, 이후 스크린 상태
  검증 자동화는 선택 과제)

## 5. 표준 트레이스 셋 (Phase A에서 실기기로 녹화)

| 파일 | 내용 |
|---|---|
| `one_finger_move.pptrace` | 단순 이동 + 정지 + 이동 |
| `one_finger_tap.pptrace` | 짧은 탭, 더블탭 |
| `two_finger_scroll_v.pptrace` | 세로 스크롤 (양방향) |
| `two_finger_tap.pptrace` | 우클릭 |
| `pinch_zoom.pptrace` | 줌 인/아웃 |
| `three_finger_swipe_up.pptrace` | Task View |
| `four_finger_swipe_left.pptrace` | 데스크탑 전환 |
| `palm_touch.pptrace` | 손바닥 오접촉 (Phase D) |
| `messy_real_usage.pptrace` | 90초 실사용 — 오발동 회귀용 |

녹화: `pc/tools/recorder --out traces/<name>.pptrace` 실행 후 폰에서 동작 수행.

> Phase B 단계에서는 위 멀티터치 트레이스를 **합성(손작성) `.pptrace` 픽스처**로
> 만들어 L2/L3에 사용한다 (`pc/tools/replay/fixtures/`:
> `two_finger_scroll_v|_h`, `two_finger_tap`, `three_finger_swipe_up`,
> `four_finger_swipe_left`, `pinch_zoom`). 폰 없이 결정론적으로 회귀할 수 있으며,
> 실기기 녹화(자연스러움·실사용 검증)는 L4로 이연한다.

## 6. 지연 측정

- PING/PONG RTT 히스토그램: client가 `--latency-report` 플래그로 60초 수집
  → p50/p95/p99 출력
- 터치→주입 파이프라인 내부 지연: 프레임 수신 시각 vs Sink 주입 완료 시각 로깅
- 목표: 내부 처리 <2ms, RTT p95 <5ms

## 7. 수동 E2E 체크리스트 (Phase 수락 시)

1. 콜드 스타트: 케이블 연결 → 10초 내 트랙패드 동작
2. 케이블 분리 → 재연결 복구
3. 폰 화면 회전 상태에서 연결
4. 전화 수신 중단 (ACTION_CANCEL 경로)
5. PC 절전 → 복귀
6. 제스처 전 종목 (`03-HID-PTP-SPEC.md` §6)
