# QUICKSTART — phone2pad 빠른 시작 (사용자용)

USB-C로 연결한 Android 폰을 Windows 트랙패드처럼 사용하는 방법입니다.
대상: Windows 10/11 (x64), Android 9(API 28) 이상.

> 현재 버전은 USB 전용입니다. Wi-Fi / Bluetooth는 지원하지 않습니다.

## 1. 준비물

- USB 데이터 전송이 가능한 USB-C 케이블
- **adb (Android Platform Tools)** — phone2pad에는 포함되어 있지 않습니다.
  - [Android SDK Platform Tools](https://developer.android.com/tools/releases/platform-tools)를
    내려받아 `adb.exe`를 PATH에 등록하거나, Android Studio 설치본의 platform-tools를 사용하세요.
  - 확인: PowerShell에서 `adb version` 이 동작해야 합니다.

## 2. 폰 설정 (최초 1회)

1. 설정 > 휴대전화 정보 > 빌드 번호를 7번 눌러 **개발자 옵션**을 켭니다.
2. 개발자 옵션 > **USB 디버깅**을 켭니다.
3. phone2pad 앱(APK)을 설치합니다.
   - GitHub Release의 `phone2pad-android-vX.Y.Z.apk`를 받아 설치하세요.
   - "출처를 알 수 없는 앱 설치"를 허용해야 할 수 있습니다.

## 3. PC client 먼저 실행

1. GitHub Release의 `phone2pad-windows-x64-vX.Y.Z.zip`을 받아 압축을 풉니다.
   (별도 런타임 설치 불필요 — 정적 링크된 실행 파일입니다.)
2. 압축을 푼 폴더에서 **`phone2pad_client.exe`**를 실행합니다.
   - client는 adb forward를 설정하고 연결을 **대기**합니다.
     (폰 앱을 자동으로 켜지 않습니다.)
   - 콘솔에 `Open phone2pad on your Android phone and tap Trackpad Mode Start.`
     안내가 보이면 정상입니다. 종료는 Ctrl+C.

## 4. 폰에서 트랙패드 모드 시작

1. 폰을 USB 케이블로 PC에 연결합니다.
2. 폰에 "USB 디버깅을 허용하시겠습니까?" 창이 뜨면 **허용**합니다.
3. 폰의 phone2pad 앱을 열고 **[트랙패드 모드 시작]**을 누릅니다.
   - 화면이 검게 변하면 트랙패드 모드이며, 이때 PC client가 연결됩니다.

## 5. 제스처

| 손가락 | 동작 |
|---|---|
| 1손가락 | 커서 이동 / 탭 = 좌클릭 / 길게 눌러 드래그 |
| 2손가락 | 스크롤 / 탭 = 우클릭 / 핀치 = 확대·축소 |
| 3손가락 | 위 = Task View, 아래 = 바탕화면 보기, 좌우 = Alt+Tab |
| 4손가락 | 좌우 = 가상 데스크탑 전환 |

## 6. 문제 해결

- 커서가 안 움직임: `adb devices` 에 폰이 보이는지, USB 디버깅 허용을 눌렀는지 확인하세요.
- 연결이 자꾸 끊김: 케이블이 데이터 전송용인지, 허브 대신 PC 직결인지 확인하세요.
- 케이블을 뽑으면 폰은 평소 상태로 돌아옵니다.

## 라이선스

MIT License ([LICENSE](LICENSE) 참고).
