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
- **GestureRouter**: peak 접촉 수로 분기 (1손가락→MouseSink, 2+→GestureSink),
  2→1 decay 시 커서 점프 방지 (앵커 리셋). — 구현 완료
- **GestureSink**: 손가락 개수 + 방향 판별 상태머신 (Idle/Settling/Tracking/Committed/
  AltTabSelecting/Cancelled). **Multi-touch settle window**(기본 90ms)로 손가락 수를
  확정해 landing skew로 인한 4↔3 오인식 방지 — L4 fix 반영
  - 2손가락: 스크롤(`MOUSEEVENTF_WHEEL`/`HWHEEL`, 기본 natural 방향), 탭 = 우클릭
    (settle 안 빠른 탭 보존), 핀치 = Ctrl+휠 (disambiguation) — 구현 완료 (핀치 B2, config 토글)
  - 3손가락 위 = `Win+Tab`, 아래 = `Win+D` — 구현 완료
  - **3손가락 좌우 = 인터랙티브 Alt+Tab** (Alt hold → 진입 방향대로 Tab/Shift+Tab으로 UI 표시
    → 좌우 step 선택 → lift/cancel/disconnect 시 Alt 떼어 확정) — L4 fix 반영
  - 4손가락 좌우 = `Ctrl+Win+←/→` (가상 데스크탑, 방향키 extended-key), 위/아래 = 3손가락 미러(옵션) — 구현 완료
  - 모두 `SendInput` 주입
- 설정: CLI 플래그 + `GestureConfig`(스크롤 방향/감도, 핀치 토글 등). `config.toml` 미도입.
- 제스처 단위 테스트: 트레이스/합성 프레임 입력 → 기대 액션 시퀀스 + 오발동 negative 검증

**수락 기준**
- [x] 표준 트레이스 셋에 대한 제스처 판별 정확도 100% (오발동 0) — L2/L3 자동 테스트 통과
- [ ] 실사용: 데스크탑 전환·Task View·스크롤이 자연스러움 — 실기기 L4 대기
- [~] **이 시점부터 일상 사용 가능 (MVP)** — user-mode 경로 완성, 실기기 검증 대기

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

## Phase D — distribution + 폴리시

### D-1 distribution (배포 패키징) — 진행 중

현재 USB/ADB user-mode MVP(Phase A/B)를 배포 가능한 형태로 정리. Phase C(드라이버),
Wi-Fi/Bluetooth는 범위 외.

**공식 사용 흐름 (v0.2.0, 수동 시작만 지원)**
PC client는 폰 앱을 자동 실행하지 않는다. ① PC에서 `phone2pad_client.exe` 실행
(adb forward만 설정하고 대기) → ② 폰에서 phone2pad 앱 실행 → ③ `MainActivity`에서
[트랙패드 모드 시작] → ④ `BlackPadActivity` 검정 화면 + 서버 시작 → ⑤ client 연결.

**산출물**
- Android: `MainActivity` 런처(USB 사용법 안내 + [트랙패드 모드 시작]), `versionName 0.2.0`,
  release 서명 설정(`keystore.properties` 기반, 키는 커밋 금지)
- PC: 정적 CRT(`/MT`) 빌드로 self-contained Windows zip, **auto-launch(am start) 기본 제거**
- `scripts/package-release.ps1`: Release 빌드 → zip → (키 있으면) APK/AAB → `SHA256SUMS.txt`
- 문서: `QUICKSTART.md`, `RELEASE.md`, `CHANGELOG.md`, `PRIVACY.md`, `LICENSE`(MIT)

**수락 기준**
- [x] `scripts/test-all.ps1` 회귀 유지 (proto L1 + client L2/L3 + `:app:assembleDebug`)
- [x] `package-release.ps1`이 self-contained zip 생성 (client는 시스템 DLL만 의존)
- [x] keystore 없으면 Android release asset SKIP + 경고 (debug APK는 release asset 아님)
- [x] client가 폰 앱을 자동 실행하지 않고 연결 대기 (수동 [트랙패드 모드 시작] 필요)
- [x] `BlackPadActivity` 직접 기동 경로(`adb am start`)는 개발자용으로 유지(`exported=true`)
- [ ] GitHub Release `v0.2.0` 게시 (사용자: 태그 push + `gh release create`)
- [ ] Google Play Internal testing 업로드 (사용자: Play Console, AAB + Data safety + Privacy URL)

### D-2 폴리시 (지속)

- 팜 리젝션 휴리스틱 (접촉 면적 `MotionEvent.getSize` → confidence=0)
- HAPTIC 패킷: 탭/클릭 시 폰 진동 피드백
- 폰 전력 최적화: 사용 안 할 때 send 스레드 sleep, 화면 OLED 완전 검정 확인
- 연결 UX: 토스트 없는 자동 시작/종료, PC 트레이 아이콘
  - **auto-launch 재검토**: PC client가 `adb am start`로 폰 패드 모드를 자동 진입시키는
    옵션(`--auto-launch`)을 wireless/polish 단계에서 다시 검토 (v0.2.0에서는 수동 시작만 지원)
- 지연 측정 리포트 자동화 (p50/p95/p99)

---

## 리스크와 대응

| 리스크 | 가능성 | 대응 |
|---|---|---|
| PTPHQA blob으로도 PTP 인식 실패 | 중 | mac-precision-touchpad의 동작 사례 분석, 디스크립터 diff 비교. 최악 시 Phase B가 fallback |
| VHF 문서/샘플 부족 | 중 | MS 공식 샘플(`VhfSample` 계열) + vmulti 경로 병행 검토 |
| ADB 지연 스파이크 | 저 | PING 히스토그램으로 측정, 필요 시 AOA(USB Accessory) 전환 검토 |
| 폰 화면 꺼짐(슬립)으로 터치 중단 | 중 | FLAG_KEEP_SCREEN_ON + 밝기 0 전략. wakelock은 최후 수단 |
