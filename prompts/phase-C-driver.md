# Claude Code 프롬프트 — Phase C (PTP 가상 HID 드라이버)

전제: Phase A/B 완료. WDK 설치, testsigning 켜진 개발 머신.
**마일스톤 C-1 ~ C-6을 세션을 나눠 하나씩 진행한다. 한 세션에 하나만.**

---

## 세션 C-1/C-2 프롬프트

CLAUDE.md, docs/01, docs/03(가장 중요), docs/04 Phase C를 읽고 시작해.
이번 목표: C-1(빈 VHF 장치) + C-2(PTP 인식).

1. `pc/driver/` 에 KMDF 드라이버 프로젝트 생성. Virtual HID Framework(VHF) 사용:
   VhfCreate → VhfStart. INF, 설치 절차 README 포함 (설치 명령은 문서화만, 실행은 사용자).
   - 구현 전 Microsoft Learn에서 VHF API(VhfCreate, VHF_CONFIG, EvtVhfAsyncOperationGetFeature 등)
     최신 시그니처를 검색·확인하고 출처 URL 주석 필수.
2. HID Report Descriptor를 docs/03 §1-2 대로 작성:
   Touch Pad collection(5 contacts, parallel) + Mouse collection +
   feature reports(Contact Count Max, PTPHQA Certification Status, Input Mode,
   Selective Reporting, Latency Mode).
   - PTPHQA blob은 imbushuo/mac-precision-touchpad가 사용하는 공개 테스트 블롭을
     레퍼런스로 확인하되, 라이선스 확인 후 출처 명시.
3. GetFeature/SetFeature 콜백 구현. Input Mode 상태를 device interface로
   user-mode에 노출 (이벤트 또는 polling).
4. user-mode 진단 도구 `pc/tools/ptpdiag`: 장치 존재, feature 교환 로그 확인.

수락: 장치 관리자 정상 + **설정 > 터치패드 페이지 등장** (사용자 확인 항목).
디스크립터의 모든 필드에 docs/03 표의 어떤 행에 해당하는지 주석을 달아.

---

## 세션 C-3~C-5 프롬프트

이전 세션 결과와 docs/notes/ 를 읽고 시작해. 이번 목표: 입력 보고 경로.

1. 드라이버에 IOCTL_PHONE2PAD_SUBMIT_FRAME 추가: user-mode가 보낸 PTP input report를
   VhfReadReportSubmit으로 주입.
2. PtpSink 구현: TouchFrame → PTP report 변환.
   - Scan Time = timestampUs / 100, u16 wrap, 단조 증가 보장
   - lift 규칙: tip=0 1회 보고 (docs/03 §3)
   - Input Mode != 3 이면 보고 억제
   - keep-alive: 접촉 유지 + 이벤트 정지 시 10ms 간격 재보고 (config 토글)
3. replay --sink ptp 지원.
4. 회귀: 표준 트레이스 셋 재생 → 사용자 확인 체크리스트(docs/03 §6) 출력.

C-3(1손가락)부터 확인받고 C-4(멀티), C-5(3/4손가락) 순서로 진행해.
막히면 추측으로 디스크립터를 바꾸지 말고, 현재 디스크립터 hex dump와
SetFeature 로그를 정리해서 사용자에게 보고해.
