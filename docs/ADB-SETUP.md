# ADB 설치 안내 — phone2pad

phone2pad는 폰과 PC 사이를 **USB로 연결**하기 위해 `adb`(Android Platform Tools)를
사용합니다. **adb는 필수**이며, phone2pad에는 **포함되어 있지 않습니다**
(구글 공식 도구라 함께 배포하지 않습니다).

adb는 폰↔PC 사이의 **로컬 USB 통신**에만 쓰입니다. 인터넷으로 전송되는 것은 없습니다.

> phone2pad는 adb를 **자동으로 내려받거나 설치하지 않습니다.** 아래 안내에 따라
> 직접 준비해 주세요. **관리자 권한은 필요 없습니다.**

> **트레이가 직접 안내합니다.** `phone2pad_tray.exe`를 실행했을 때 adb가 없으면,
> 트레이가 설치 안내 창을 띄우고 메뉴에 **[ADB 설치 페이지 열기]** 와 **[ADB 다시 확인]**
> 항목을 보여줍니다. 아래 내용은 그 보조 설명입니다.

## 이미 가지고 있나요? (Android Studio 사용자)

Android Studio를 설치했다면 보통 adb가 이미 아래 위치에 있어, **별도 설정 없이
자동으로 인식**됩니다.

```
%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe
```

트레이/콘솔이 곧바로 폰을 기다리면 준비가 끝난 것입니다.

## 새로 설치하기 — 가장 쉬운 방법 (exe 옆에 두기)

별도의 PATH 설정 없이 **폴더만 옮기면** 됩니다.

1. 트레이 메뉴의 **[ADB 설치 페이지 열기]**(또는 아래 링크)로 공식
   [Android SDK Platform-Tools](https://developer.android.com/tools/releases/platform-tools)
   페이지를 열고, Windows용 zip을 내려받습니다.
2. zip의 압축을 풉니다. 안에 `platform-tools` 폴더와 그 안의 `adb.exe`가 들어 있습니다.
3. 그 `platform-tools` 폴더째로 **`phone2pad_tray.exe`가 있는 폴더 안에** 넣습니다.
   아래처럼 되도록 두면 됩니다.

   ```
   phone2pad\
     phone2pad_tray.exe
     platform-tools\adb.exe
   ```

4. 트레이 메뉴에서 **[ADB 다시 확인]**을 누릅니다. 상태가 **폰 연결 대기**로 바뀌면
   준비 완료입니다. (트레이를 다시 실행할 필요는 없습니다.)

## (고급) PATH에 등록하기

여러 도구에서 adb를 쓰고 싶다면 `platform-tools` 폴더를 시스템 `PATH`에 등록해도 됩니다.
PowerShell에서 아래를 복사·붙여넣기 하세요(경로는 압축을 푼 실제 위치로 바꾸세요).

```powershell
# 현재 사용자 PATH에 platform-tools 추가 (관리자 불필요)
setx PATH "$($env:Path);C:\Tools\platform-tools"
```

새 PowerShell 창을 열고 `adb version`이 동작하면 성공입니다.

## phone2pad가 adb를 찾는 순서

1. 시스템 `PATH`
2. `%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe`
3. `%ANDROID_HOME%\platform-tools\adb.exe`
4. `%ANDROID_SDK_ROOT%\platform-tools\adb.exe`
5. 실행 파일 옆의 `platform-tools\adb.exe`

이 중 **하나만** 충족하면 됩니다. 준비가 끝났는데도 인식되지 않으면, 트레이 메뉴의
**[ADB 다시 확인]**을 누르고, 폰 화면의 "USB 디버깅 허용" 창을 확인한 뒤 케이블을 다시
연결해 보세요.

자세한 전체 사용법은 [QUICKSTART.md](../QUICKSTART.md)를 참고하세요.
