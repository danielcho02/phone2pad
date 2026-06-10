# 04 — ROADMAP: Phase별 계획과 수락 기준

각 Phase는 독립적으로 동작 가능한 결과물을 낸다. Phase B까지만 가도 일상 사용이
가능하고, Phase C가 본래 목표(네이티브 PTP)다.

---

## Phase 0 — 부트스트랩 + 프로토콜 (예상: 1~2일)

**산출물**
- 레포 구조 생성, 빌드 시스템 (android: Gradle, pc: CMake)
- `proto` 직렬화 라이브러리: Kotlin 인코더 + C++ 디코더/인코더
- reference 인코더로 `proto/test-vectors/*.json` 생성
- CI 수준의 로컬 테스트 스크립트 (`scripts/test-all.ps1` / `.sh`)

**수락 기준**
- [x] Kotlin/C++ 양쪽이 동일 테스트 벡터 round-trip 통과
- [x] 손상 스트림 재동기화 단위 테스트 통과

---

## Phase A — End-to-End 배관 + 상대좌표 마우스 (예상: 2~3일)

**산출물**
- Android: BlackPadActivity + PadSocketServer (검정 화면, 터치 전송)
- PC: AdbManager (장치 감지 → forward → 앱 자동 기동), FrameReceiver, **MouseSink**
  - MouseSink: 1손가락 delta → `SendInput` 상대 마우스 이동, 탭 = 좌클릭
- `recorder` 도구: 실제 폰 터치 트레이스를 `.pptrace`로 저장
- `replay` 도구: 트레이스 재생 → Sink 주입

**수락 기준**
- [ ] 폰 연결 → 자동으로 검정 화면 → 커서가 움직인다
- [ ] 탭 = 클릭, 더블탭 동작
- [ ] PING RTT 로그 p95 < 5ms (ADB-USB 기준)
- [ ] replay로 폰 없이 커서 이동 재현
- [ ] 표준 트레이스 5종 녹화 완료 (`05-TESTING.md` 목록)

---

## Phase B — User-mode 제스처 MVP (예상: 3~5일)

**산출물**
- **GestureSink**: 손가락 개수 + 방향 판별 상태머신
  - 1손가락: 이동/탭/드래그(탭 후 즉시 이동)
  - 2손가락: 스크롤(`MOUSEEVENTF_WHEEL`/`HWHEEL`), 2손가락 탭 = 우클릭, 핀치 = Ctrl+휠
  - 3손가락 위 = `Win+Tab`, 3손가락 좌우 = `Alt+Tab` 계열, 아래 = `Win+D`
  - 4손가락 좌우 = `Win+Ctrl+←/→` (가상 데스크탑)
  - 모두 `SendInput` 주입
- 포인터 가속 곡선 (간단한 2구간), 설정 파일로 토글/속도 조절
- 제스처 단위 테스트: 트레이스 입력 → 기대 액션 시퀀스 검증

**수락 기준**
- [ ] 표준 트레이스 셋에 대한 제스처 판별 정확도 100% (오발동 0)
- [ ] 실사용: 데스크탑 전환·Task View·스크롤이 자연스러움
- [ ] **이 시점부터 일상 사용 가능 (MVP)**

---

## Phase C — PTP 가상 HID 드라이버 (예상: 1~2주, 최고 난이도)

**산출물**
- `pc/driver`: KMDF + VHF 가상 PTP 장치 (`03-HID-PTP-SPEC.md` 준수)
- **PtpSink**: TouchFrame → PTP input report 변환 → 드라이버 IOCTL
- 설치 스크립트 + testsigning 수동 절차 README
- 진단 도구: 드라이버 로그 뷰어, GetFeature/SetFeature 트레이스

**마일스톤 분해 (순서 중요)**
1. C-1: 빈 VHF 장치 생성, 장치 관리자에 정상 표시
2. C-2: PTPHQA blob + 필수 feature 응답 → **설정 > 터치패드 페이지 등장**
3. C-3: 단일 contact 보고 → 커서 이동 (OS 가속 적용 확인)
4. C-4: 멀티 contact → 2손가락 스크롤/핀치
5. C-5: 3/4손가락 OS 제스처 전체 동작
6. C-6: replay 회귀 테스트 전 시나리오 통과

**수락 기준**: `03-HID-PTP-SPEC.md` §6 체크리스트 전부

---

## Phase D — 폴리시 (지속)

- 팜 리젝션 휴리스틱 (접촉 면적 `MotionEvent.getSize` → confidence=0)
- HAPTIC 패킷: 탭/클릭 시 폰 진동 피드백
- 폰 전력 최적화: 사용 안 할 때 send 스레드 sleep, 화면 OLED 완전 검정 확인
- 연결 UX: 토스트 없는 자동 시작/종료, PC 트레이 아이콘
- 지연 측정 리포트 자동화 (p50/p95/p99)

---

## 리스크와 대응

| 리스크 | 가능성 | 대응 |
|---|---|---|
| PTPHQA blob으로도 PTP 인식 실패 | 중 | mac-precision-touchpad의 동작 사례 분석, 디스크립터 diff 비교. 최악 시 Phase B가 fallback |
| VHF 문서/샘플 부족 | 중 | MS 공식 샘플(`VhfSample` 계열) + vmulti 경로 병행 검토 |
| ADB 지연 스파이크 | 저 | PING 히스토그램으로 측정, 필요 시 AOA(USB Accessory) 전환 검토 |
| 폰 화면 꺼짐(슬립)으로 터치 중단 | 중 | FLAG_KEEP_SCREEN_ON + 밝기 0 전략. wakelock은 최후 수단 |
