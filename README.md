# phone2pad

> USB로 연결한 Android 폰을 Windows 트랙패드로 사용하는 앱.

**⚠️ v0.2.0 — 알파 (사전 배포본).** 초기 공개 버전으로, 기능과 안정성이 계속
바뀔 수 있습니다. 실사용 피드백을 위해 공개합니다.

phone2pad는 USB/ADB로 연결한 Android 폰을 Windows의 멀티터치 트랙패드로 바꿔 줍니다.
앱에서 트랙패드 모드를 시작하면 폰 화면이 검게 변하고, 그때부터 폰 표면의 터치가
PC의 커서·스크롤·제스처 입력으로 전달됩니다.

## 주요 기능

- **한 손가락** — 커서 이동, 탭(좌클릭), 길게 눌러 드래그
- **두 손가락** — 스크롤, 탭(우클릭)
- **세 손가락** — 앱 전환 제스처 (작업 보기 / 앱 전환 / 바탕화면 보기)
- **네 손가락** — 가상 데스크탑 전환 제스처
- **수동 트랙패드 모드 시작** — 앱에서 직접 모드를 켤 때만 터치가 전송됨

## 빠른 시작

자세한 안내는 **[QUICKSTART.md](QUICKSTART.md)** 를 참고하세요. 요약:

1. GitHub Release에서 Windows zip(`phone2pad-windows-x64-v0.2.0.zip`)을 다운로드합니다.
2. GitHub Release에서 Android APK zip(`phone2pad-android-v0.2.0-apk.zip`)을 다운로드합니다.
3. Android APK zip의 압축을 풀고, 안에 든 APK를 폰에 설치합니다.
4. 폰에서 **개발자 옵션**과 **USB 디버깅**을 활성화합니다.
5. Windows에 **Android Platform Tools(adb)** 를 설치합니다.
6. Windows zip의 압축을 풀고 **`phone2pad_client.exe`** 를 실행합니다.
   (client는 연결을 대기합니다.)
7. 폰에서 phone2pad 앱을 열고 **[트랙패드 모드 시작]** 을 누릅니다.
   화면이 검게 변하면 트랙패드로 동작합니다.

> **참고:** GitHub 웹 업로드는 `.apk` 파일을 거부할 수 있어, APK는
> `phone2pad-android-v0.2.0-apk.zip` 형태로 배포됩니다. zip을 풀어 안의 APK를 설치하세요.

## 제한 사항

- **USB/ADB 연결만 지원**합니다.
- **Wi-Fi / Bluetooth는 아직 지원하지 않습니다.**
- **네이티브 Windows Precision Touchpad 드라이버는 아직 포함되어 있지 않습니다.**
  현재는 user-mode 입력 방식으로 동작합니다.
- 폰에서 **USB 디버깅 활성화가 필요합니다.**

## 보안 및 개인정보

- **계정이나 로그인이 필요 없습니다.**
- **클라우드 서버를 사용하지 않습니다.**
- 터치 입력은 **USB/ADB 포워딩을 통해 로컬(같은 기기 ↔ 연결된 PC)로만 전송**되며,
  외부로 전송되거나 저장되지 않습니다.
- **비공식 출처의 APK를 내려받지 마세요.** 반드시 공식 GitHub Release만 사용하세요.

자세한 내용은 **[PRIVACY.md](PRIVACY.md)** 를 참고하세요.

## 문서

| 문서 | 내용 |
|---|---|
| [QUICKSTART.md](QUICKSTART.md) | 설치 및 사용 빠른 시작 |
| [RELEASE.md](RELEASE.md) | 빌드 · 배포 절차 |
| [PRIVACY.md](PRIVACY.md) | 개인정보 처리방침 |
| [CHANGELOG.md](CHANGELOG.md) | 버전별 변경 이력 |

## 라이선스

MIT License — [LICENSE](LICENSE) 참고.
