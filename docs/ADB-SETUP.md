# ADB 설치 안내 — phone2pad

phone2pad는 폰과 PC 사이를 **USB로 연결**하기 위해 `adb`(Android Platform Tools)를
사용합니다. **adb는 필수**이며, phone2pad에는 **포함되어 있지 않습니다**
(구글 공식 도구라 함께 배포하지 않습니다).

adb는 폰↔PC 사이의 **로컬 USB 통신**에만 쓰입니다. 인터넷으로 전송되는 것은 없습니다.

> phone2pad는 adb를 **자동으로 내려받거나 설치하지 않습니다.** 아래 안내에 따라
> 직접 준비해 주세요. 관리자 권한은 필요 없습니다.

## 이미 가지고 있나요? (Android Studio 사용자)

Android Studio를 설치했다면 보통 adb가 이미 아래 위치에 있습니다.

```
%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe
```

이 경우 별도 설정 없이 phone2pad가 자동으로 찾습니다. 트레이/콘솔이 곧바로 폰을
기다리면 준비가 끝난 것입니다.

## 새로 설치하기 (단독 설치)

1. 공식 [Android SDK Platform-Tools](https://developer.android.com/tools/releases/platform-tools)
   페이지에서 Windows용 zip을 내려받습니다.
2. zip의 압축을 풉니다. 안에 `platform-tools` 폴더와 그 안의 `adb.exe`가 들어 있습니다.
3. 아래 **둘 중 하나**만 하면 됩니다.

   - **방법 A — PATH에 추가:** `platform-tools` 폴더를 시스템 `PATH`에 등록합니다.
     명령 프롬프트에서 `adb version`이 동작하면 성공입니다.
   - **방법 B — exe 옆에 두기 (가장 쉬움):** `platform-tools` 폴더째로
     `phone2pad_tray.exe`(또는 `phone2pad_client.exe`)가 있는 폴더 안에 넣습니다.
     즉 아래처럼 되도록 둡니다. PATH 설정이 필요 없습니다.

     ```
     phone2pad_tray.exe
     platform-tools\adb.exe
     ```

## phone2pad가 adb를 찾는 순서

1. 시스템 `PATH`
2. `%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe`
3. `%ANDROID_HOME%\platform-tools\adb.exe`
4. `%ANDROID_SDK_ROOT%\platform-tools\adb.exe`
5. 실행 파일 옆의 `platform-tools\adb.exe`

이 중 **하나만** 충족하면 됩니다. 준비가 끝났는데도 인식되지 않으면, 다른 폰 화면의
"USB 디버깅 허용" 창을 확인하고 케이블을 다시 연결해 보세요.

자세한 전체 사용법은 [QUICKSTART.md](../QUICKSTART.md)를 참고하세요.
