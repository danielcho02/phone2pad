# phone2pad

> USB-C로 연결한 Android 폰을 Windows 데스크탑의 **네이티브 Precision Touchpad**로
> 만드는 프로젝트. 폰 화면은 완전 검정(UI 없음), 제스처 해석은 Windows OS가 직접 수행.

기존 "폰을 마우스로" 앱들과의 차이: 마우스 에뮬레이션이 아니라 Windows가 장치를
**트랙패드로 인식**하므로, 3손가락 Task View / 4손가락 가상 데스크탑 전환 등
노트북과 동일한 OS 제스처가 설정 > 터치패드 페이지에서 그대로 동작한다.

## 문서 인덱스

| 경로 | 내용 |
|---|---|
| `CLAUDE.md` | 코딩 에이전트 루트 지침 (작업 전 필독) |
| `docs/00-OVERVIEW.md` | 목표, 비목표, 성공 기준 |
| `docs/01-ARCHITECTURE.md` | 시스템 구조 |
| `docs/02-PROTOCOL.md` | 폰↔PC 와이어 프로토콜 (SSOT) |
| `docs/03-HID-PTP-SPEC.md` | Windows PTP HID 요구사항 |
| `docs/04-ROADMAP.md` | Phase 계획 + 수락 기준 |
| `docs/05-TESTING.md` | 테스트 하네스 (폰 없는 개발 루프) |
| `docs/06-WORKFLOW.md` | 빌드, Git, 에이전트 운용 규칙 |
| `prompts/` | Phase별 Claude Code 시작 프롬프트 |

## 빠른 시작 (사용자)

릴리스로 사용하는 방법은 **[QUICKSTART.md](QUICKSTART.md)**를 보세요. 요약(PC 먼저, 폰 나중):

1. adb(Android Platform Tools)를 설치하고 PATH에 등록한다. (zip에 포함되지 않음)
2. GitHub Release에서 `phone2pad-windows-x64-vX.Y.Z.zip`을 받아 풀고 `phone2pad_client.exe` 실행.
   - 정적 링크 빌드라 별도 런타임(VC++ 재배포) 설치가 필요 없다.
   - client는 adb forward만 설정하고 연결을 대기한다. (폰 앱을 자동 실행하지 않음)
3. 폰에 `phone2pad-android-vX.Y.Z.apk`를 설치하고 USB 디버깅을 허용한다.
4. 폰에서 phone2pad 앱을 열고 **[트랙패드 모드 시작]**을 누른다 → 화면이 검게 변하면 트랙패드로 동작.

## 빌드 (개발)

```powershell
# PC client + 테스트
./scripts/test-all.ps1
# 배포 패키징 (dist/ 에 zip / apk·aab / SHA256SUMS.txt 생성)
./scripts/package-release.ps1 -Version 0.2.0
```

릴리스/서명/배포 절차는 **[RELEASE.md](RELEASE.md)**, 변경 이력은
**[CHANGELOG.md](CHANGELOG.md)**를 참고하세요.

## 사전 준비 (사용자 수동)

- 폰: 개발자 옵션 → USB 디버깅 ON, PC RSA 키 승인
- PC: adb (platform-tools) PATH 등록
- Phase C부터: WDK 설치, `bcdedit /set testsigning on` 후 재부팅

## 상태

- Phase 0 / A / B 완료 (USB/ADB 기반 user-mode MVP). `main`에 `v0.2-phase-b` 태그.
- **현재: Phase D — distribution.** GitHub Release(Windows zip + release-signed APK)와
  Google Play Internal testing(AAB) 배포 준비. 배포 태그는 SemVer `v0.2.0`.
- Phase C(네이티브 PTP HID 드라이버), Wi-Fi/Bluetooth는 이후 별도 작업.

## 라이선스

MIT License — [LICENSE](LICENSE) 참고. 개인정보 처리방침은 [PRIVACY.md](PRIVACY.md).
