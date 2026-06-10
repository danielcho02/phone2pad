# 03 — HID-PTP-SPEC: Windows Precision Touchpad 인식 요구사항

> Phase C의 핵심 문서. 여기 내용은 Microsoft Learn의
> "Windows Precision Touchpad device integration / required HID descriptors" 문서 계열을
> 근거로 작성되었다. **구현 전 에이전트는 반드시 최신 MS 공식 문서를 재확인하고
> 코드 주석에 출처 URL을 남길 것.** 하나라도 틀리면 Windows는 장치를
> 일반 마우스/디지타이저로 fallback시키며, 어떤 에러도 보여주지 않는다.

## 1. 장치가 노출해야 하는 Top-Level Collection

PTP 장치는 보통 다음 컬렉션을 노출한다:

1. **Touch Pad collection** — `Usage Page 0x0D (Digitizer)`, `Usage 0x05 (Touch Pad)`
   - Input report: 멀티터치 contact 데이터
   - Feature report: Device Capabilities, Certification Status(PTPHQA), Input Mode 등
2. **Mouse collection** — `Usage Page 0x01`, `Usage 0x02 (Mouse)`
   - PTP 미지원 OS/부팅 환경에서의 fallback 경로. (가상 장치라도 포함 권장)
3. (선택) Configuration collection — Input Mode/Selective Reporting을 분리 수용

## 2. Touch Pad Input Report 필수 필드

contact당 (Finger collection, `Usage 0x22`):

| Usage | 이름 | 비고 |
|---|---|---|
| 0x47 (Digitizer) | **Confidence** | 팜 리젝션. 손가락=1 |
| 0x42 (Digitizer) | **Tip Switch** | 접촉 여부 |
| 0x51 (Digitizer) | **Contact Identifier** | 세션 내 불변 ID |
| 0x30 (Generic Desktop) | **X** | logical + **physical extent와 unit(cm) 필수** |
| 0x31 (Generic Desktop) | **Y** | 동일 |

보고서 레벨:

| Usage | 이름 | 비고 |
|---|---|---|
| 0x56 (Digitizer) | **Scan Time** | **100µs 단위**, u16, wrap. 제스처 속도 계산에 사용되므로 정확해야 함 |
| 0x54 (Digitizer) | **Contact Count** | 이 보고서에 담긴 contact 수 |
| 0x09/Button 1 | **Button** | 물리 클릭 버튼. 우리는 탭으로 대체하지만 필드는 선언 |

Feature report:

| Usage | 이름 | 비고 |
|---|---|---|
| 0x55 (Digitizer) | **Contact Count Maximum** | 우리: 5 |
| 0xC5 (Digitizer, vendor 영역) | **Certification Status (PTPHQA blob)** | 256-byte feature. MS가 공개한 테스트 서명 블롭 존재 — `mac-precision-touchpad` 레포가 사용하는 값과 동일 계열. 없으면 PTP로 인식 안 됨 |
| 0x52 (Digitizer) | **Input Mode** | 호스트가 0(mouse)/3(touchpad) 설정. **부팅 직후 기본은 mouse mode이며 호스트가 3을 쓰기 전까지 PTP 보고 금지** |
| 0x57+0x58 | **Selective Reporting** (Surface/Button switch) | 호스트 제어 feature |
| (Latency) 0x60 | Latency Mode | wake/normal. 수신만 하면 됨 |

## 3. 보고 규칙 (런타임 동작)

1. **Parallel 모드 권장**: 한 input report에 현재 모든 contact 포함, `Contact Count` = 실제 수.
   (Hybrid 모드보다 구현 단순. 우리 프로토콜의 TOUCH_FRAME과 1:1 대응)
2. **lift 보고**: 손가락이 떨어질 때 해당 contact를 `Tip Switch=0`으로 마지막 1회 보고.
3. **보고 주기**: 접촉 중 최소 ~100Hz 권장. 이벤트 없으면 보고하지 않아도 됨
   (단, 접촉 유지 중 정지 상태에서도 주기 보고가 안정적 — 폰 이벤트가 멈추면
   드라이버가 마지막 상태를 10ms 간격으로 재보고하는 keep-alive 옵션 구현).
4. **Scan Time은 폰 timestampUs/100**으로 변환해 단조 증가 보장.
5. Input Mode가 touchpad(3)가 되기 전까지는 Mouse collection으로만 동작하거나 침묵.

## 4. 물리 크기 (Physical Extent) 계산

WPT 제스처 엔진은 cm 단위 물리 좌표로 동작한다. 디스크립터에서:

```
Logical Max X = 폰 px 너비 (HELLO 기반, 또는 고정 4096 후 스케일)
Physical Max X = 폰 너비(px) / xdpi * 2.54  → cm
Unit = cm (0x11), Unit Exponent = -2 (예: 값이 1/100 cm 단위가 되도록)
```

> 주의: 디스크립터는 장치 생성 시점에 고정되므로, HELLO를 받은 **후**
> VhfCreate를 호출하는 시퀀스로 설계한다. 회전(가로/세로) 시에는 장치 재생성 대신
> PC 쪽에서 좌표를 세로 기준으로 정규화해 보고한다 (디스크립터 불변 유지).

## 5. 드라이버 구현 경로

**1순위: KMDF + Virtual HID Framework (VHF)**
- `VhfCreate`(HID 디스크립터/속성 제공) → `VhfStart` → `VhfReadReportSubmit`으로 보고 주입
- GetFeature/SetFeature 콜백에서 PTPHQA blob, Input Mode 처리
- user-mode 클라이언트와는 드라이버가 만든 device interface + IOCTL로 통신

**2순위 (VHF 차단 시): kernel HID minidriver (mshidkmdf 경유, vmulti 방식)**

설치/개발 환경:
- WDK + Visual Studio, `bcdedit /set testsigning on` (사용자 수동, README에 문서화)
- `devcon`/`pnputil`로 root-enumerated 디바이스 설치
- 디버깅: WinDbg + `!hidkd` 계열, 또는 트레이스 로깅

## 6. 검증 체크리스트 (Phase C 수락 기준과 연동)

- [ ] 장치 관리자: "HID 규격 터치 패드"류 노드 생성, 에러 코드 없음
- [ ] **설정 > 터치패드 페이지에 제스처 설정 UI 표시** ← PTP 인식의 결정적 증거
- [ ] Input Mode SetFeature(3) 수신 로그 확인
- [ ] 1손가락 이동 시 커서 이동 (가속 곡선이 노트북과 동일하게 적용됨)
- [ ] 2손가락 스크롤 / 핀치 줌
- [ ] 3손가락 위 = Task View, 4손가락 좌우 = 가상 데스크탑 전환
- [ ] replay 시뮬레이터의 표준 트레이스 셋으로 위 전부 재현 가능
