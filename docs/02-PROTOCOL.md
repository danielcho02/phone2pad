# 02 — PROTOCOL: 폰 ↔ PC 와이어 프로토콜 v1

> 이 문서가 단일 진실 공급원(SSOT)이다. Kotlin 인코더와 C++ 디코더는
> `proto/test-vectors/`의 동일한 테스트 벡터를 통과해야 한다.
> 프로토콜 변경 시: 이 문서 수정 → 테스트 벡터 갱신 → 양쪽 구현 수정 순서를 지킨다.

## 1. 전송 계층

- TCP, 폰이 listen (`38917`), PC가 connect (adb forward 경유)
- `TCP_NODELAY` 양쪽 필수
- 바이트 오더: **little-endian** 고정

## 2. 패킷 공통 헤더

```
offset  size  field
0       1     magic     = 0xA7
1       1     type      (아래 표)
2       2     length    payload 길이 (u16)
4       len   payload
```

| type | 이름 | 방향 | 빈도 |
|---|---|---|---|
| 0x01 | HELLO | 폰→PC | 연결 직후 1회 + 회전 시 |
| 0x02 | TOUCH_FRAME | 폰→PC | 터치 이벤트마다 (~120Hz) |
| 0x03 | PING | PC→폰 | 1초 주기 |
| 0x04 | PONG | 폰→PC | PING 응답 (echo) |
| 0x05 | HAPTIC | PC→폰 | 클릭 피드백 요청 시 (Phase D) |

## 3. HELLO (0x01)

```
offset  size  field
0       1     protocolVersion = 1
1       2     screenWidthPx   (u16)  세로 기준
3       2     screenHeightPx  (u16)
5       2     xdpi_x10        (u16)  xdpi * 10 (예: 421.7 → 4217)
7       2     ydpi_x10        (u16)
9       1     rotation        0=0°, 1=90°, 2=180°, 3=270°
10      1     maxContacts     폰이 지원하는 동시 터치 수 (보통 10)
```

## 4. TOUCH_FRAME (0x02)

하나의 `MotionEvent`(또는 batched move의 마지막 샘플) = 하나의 프레임.
**프레임에는 현재 화면에 닿아 있는 모든 contact의 상태가 들어간다 (parallel 보고).**

```
offset  size  field
0       4     timestampUs   (u32) 폰 단조 시계, µs, wrap 허용
4       1     contactCount  (u8)  0 = 모든 손가락 떨어짐
5       N*6   contacts[contactCount]

contact (6 bytes):
0       1     contactId   MotionEvent pointerId (안정적 ID)
1       1     flags       bit0: tip (1=닿음, 0=lift 중)
                          bit1: confidence (1=정상, 0=팜 의심)
2       2     x           (u16) 픽셀, 세로 기준 절대좌표
4       2     y           (u16)
```

규칙:
- 손가락이 떨어지는 순간(`ACTION_POINTER_UP`/`ACTION_UP`)에는 해당 contact를
  `tip=0`으로 **한 번 포함**한 프레임을 보낸 뒤, 이후 프레임에서 제외한다.
  (PTP 보고서 규약과 1:1 대응시키기 위함)
- `ACTION_CANCEL` 수신 시: 모든 contact `tip=0` 프레임 1개 전송.
- contactId는 터치 세션 동안 불변이어야 한다 (Android pointerId 그대로 사용).

## 5. PING/PONG (0x03/0x04)

payload: `u32 seq` + `u32 senderTimestampUs`. PONG은 그대로 echo.
PC는 RTT를 기록한다 (지연 측정 하네스에서 사용). 3초 무응답 시 연결 재수립.

## 6. HAPTIC (0x05) — Phase D

payload: `u8 effect` (0=click, 1=light tick). 폰은 `VibrationEffect`로 재생.

## 7. 테스트 벡터

`proto/test-vectors/*.json` 형식:

```json
{
  "name": "two_finger_move",
  "description": "두 손가락 이동 중 프레임",
  "packet_hex": "a702130078563412020100e8031f04010244055e06",
  "decoded": {
    "type": "TOUCH_FRAME",
    "timestampUs": 305419896,
    "contacts": [
      {"id": 1, "tip": true,  "confidence": false, "x": 1000, "y": 1055},
      {"id": 2, "tip": false, "confidence": true,  "x": 1348, "y": 1630}
    ]
  }
}
```

요구사항:
- Kotlin: `packet_hex` 디코드 == `decoded`, `decoded` 인코드 == `packet_hex`
- C++: 동일 (round-trip 양방향)
- 최소 벡터 셋: HELLO, 빈 프레임(count=0), 1/2/5 contact, lift 프레임,
  손상 프레임(재동기화 테스트), PING/PONG
- 위 예시의 hex는 **샘플 형식 예시일 뿐 검증된 값이 아니다.**
  Phase 0에서 구현 후 reference 인코더로 실제 벡터를 생성하라.
