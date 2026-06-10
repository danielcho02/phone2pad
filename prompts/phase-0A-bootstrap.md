# Claude Code 프롬프트 — Phase 0 + A (부트스트랩 → E2E 마우스)

아래를 그대로 Claude Code에 붙여넣어 시작한다. (레포 루트에서 실행)

---

CLAUDE.md와 docs/ 전체를 읽고 시작해. 이번 세션 목표는 Phase 0 완료 후 Phase A 진입이야.

## 작업 1 — Phase 0: 프로토콜 기반
1. docs/06-WORKFLOW.md의 레포 구조와 빌드 시스템을 생성해 (android Gradle 스켈레톤, pc CMake).
2. docs/02-PROTOCOL.md 명세대로 구현해:
   - C++: `pc/client/proto/` 에 인코더+디코더 (스트림 재동기화 포함)
   - Kotlin: `android/.../proto/` 에 인코더 (+검증용 디코더)
3. C++ reference 인코더로 proto/test-vectors/*.json 을 생성하는 도구를 만들고,
   docs/02 §7의 최소 벡터 셋을 생성해.
4. 양쪽 단위 테스트가 동일 벡터로 round-trip 통과해야 함. ./scripts/test-all.ps1 작성.

## 작업 2 — Phase A: E2E 배관
1. Android: BlackPadActivity (전체화면 검정, immersive, screenBrightness=0f,
   FLAG_KEEP_SCREEN_ON) + onTouchEvent → TouchFrame 전송. PadSocketServer (:38917, TCP_NODELAY).
   ACTION_CANCEL 처리 필수 (docs/01 §2.1).
2. PC client:
   - AdbManager: adb devices 폴링 → forward 설정 → am start로 폰 앱 기동
   - FrameReceiver → Sink 인터페이스 (docs/01 §2.2 전략 패턴 그대로)
   - MouseSink: 1손가락 delta → SendInput 상대 이동, 탭(150ms/10px 이내) = 좌클릭
3. pc/tools/recorder 와 pc/tools/replay 구현 (docs/05 §4-5). replay가 MouseSink에
   주입해서 폰 없이 커서가 움직이는 것까지 확인 가능해야 함.

## 제약
- 폰 = 센서. 폰 쪽에 제스처/스케일링 로직 넣지 마.
- Windows API 시그니처는 Microsoft Learn에서 확인하고 출처 주석 남겨.
- 실기기 테스트는 못 하니까, 세션 끝에 "사용자 확인 필요" 체크리스트를
  docs/04-ROADMAP.md Phase A 수락 기준 기반으로 정리해줘.
- 완료 시 CLAUDE.md §6 체크리스트 갱신 + Conventional Commits로 커밋.

작업 1부터 시작하고, 작업 1의 테스트가 전부 통과한 뒤에 작업 2로 넘어가.
