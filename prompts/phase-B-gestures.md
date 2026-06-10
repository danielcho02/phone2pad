# Claude Code 프롬프트 — Phase B (User-mode 제스처 MVP)

전제: Phase A 완료, 표준 트레이스 셋(docs/05 §5)이 traces/ 에 녹화되어 있음.
(트레이스가 아직 없으면 .pptrace 포맷으로 합성 트레이스를 먼저 만들어 진행)

---

CLAUDE.md와 docs/01, 04, 05를 읽고 시작해. 이번 세션 목표는 Phase B: GestureSink 구현이야.

## 작업
1. `pc/client/gesture/` 에 제스처 상태머신 구현:
   - 상태: IDLE → TRACKING(n손가락) → GESTURE_COMMITTED → IDLE
   - 판별 파라미터(임계 이동거리, 탭 시간, 방향 각도)는 config.toml로 외부화
   - 액션 출력은 Injector 인터페이스로 추상화 (실구현: SendInput / 테스트: mock)
2. 제스처 매핑 (docs/04 Phase B 표 그대로):
   - 1손가락 이동/탭/탭드래그
   - 2손가락 스크롤(자연 스크롤 옵션), 2손가락 탭=우클릭, 핀치=Ctrl+휠
   - 3손가락 위=Win+Tab, 아래=Win+D
   - 4손가락 좌우=Win+Ctrl+←/→
3. 포인터 가속: 저속 1:1, 고속 2.5x의 2구간 곡선. config로 조절.
4. L2 테스트 하네스 (docs/05 §3):
   - 트레이스+기대액션 JSON 러너 구현
   - 모든 트레이스에 대해 positive + negative(오발동 0) 검증
5. replay --sink gesture 지원 추가.

## 주의
- 제스처 커밋 전 이동량이 커서로 새어나가면 안 됨 (n≥2 손가락 감지 즉시 커서 잠금).
- 손가락이 비동시에 닿는 현실 패턴 처리: 첫 접촉 후 80ms 윈도우 안에 추가 접촉이
  오면 같은 제스처로 묶기. 이 윈도우도 config 파라미터.
- 수락 기준: docs/04 Phase B. 오발동 0이 최우선.
- 완료 시 체크리스트 갱신 + 커밋. 실기기 확인 항목은 따로 보고.
