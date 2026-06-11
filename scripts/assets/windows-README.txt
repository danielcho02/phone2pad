phone2pad — Windows client (x64)
================================

USB-C로 연결한 Android 폰을 Windows 트랙패드처럼 사용하는 클라이언트입니다.

포함 파일
---------
  phone2pad_client.exe   메인 클라이언트 (폰 연결 → 마우스/제스처 입력)
  recorder.exe           터치 트레이스 녹화 (고급/진단용)
  replay.exe             녹화 트레이스 재생 (고급/진단용, 폰 없이 동작 점검)
  LICENSE                MIT License

이 exe들은 정적 CRT로 빌드되어 별도 런타임(VC++ 재배포 패키지) 설치가 필요 없습니다.

사전 준비
---------
1. adb(Android Platform Tools)가 필요합니다. 이 zip에는 포함되어 있지 않습니다.
   - Android Studio를 설치했다면 platform-tools에 adb가 포함되어 있습니다.
   - 또는 "Android SDK Platform Tools"를 내려받아 adb.exe를 PATH에 등록하세요.
     https://developer.android.com/tools/releases/platform-tools
2. 폰에서 개발자 옵션 > USB 디버깅을 켜고, 연결 시 PC의 RSA 키를 허용하세요.
3. 폰에 phone2pad 앱(APK)을 설치하세요.

사용 방법
---------
1. 이 폴더에서 phone2pad_client.exe 를 실행합니다.
2. 폰을 USB 케이블로 PC에 연결합니다.
3. 폰에서 USB 디버깅을 허용합니다.
4. 폰의 phone2pad 앱에서 "패드 시작"을 누르면 화면이 검게 변하고 트랙패드로 동작합니다.

제스처
------
  1손가락: 커서 이동 / 탭 = 좌클릭 / 길게 눌러 드래그
  2손가락: 스크롤 / 탭 = 우클릭 / 핀치 = 확대·축소
  3손가락: 위로 = Task View, 아래로 = 바탕화면, 좌우 = Alt+Tab
  4손가락: 좌우 = 가상 데스크탑 전환

현재 한계
---------
  - USB 전용입니다 (Wi-Fi / Bluetooth 미지원).
  - Windows 10/11 x64 전용입니다.
  - 아직 네이티브 Precision Touchpad가 아니라 user-mode 입력 주입 방식입니다.

라이선스: MIT (LICENSE 파일 참고)
