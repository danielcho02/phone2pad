phone2pad — Windows client (x64)
================================

USB-C로 연결한 Android 폰을 Windows 트랙패드처럼 사용하는 클라이언트입니다.

포함 파일
---------
  phone2pad_tray.exe     트레이 companion (권장) — 백그라운드 상주, 상태 표시, 자동 시작
  phone2pad_client.exe   콘솔 클라이언트 (대체용) — 동일한 동작을 콘솔 창에서 실행
  recorder.exe           터치 트레이스 녹화 (고급/진단용)
  replay.exe             녹화 트레이스 재생 (고급/진단용, 폰 없이 동작 점검)
  QUICKSTART.md          빠른 시작 안내 (트레이 메뉴의 "Open setup guide"가 이 파일을 엽니다)
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

사용 방법 — 권장: 트레이 companion (phone2pad_tray.exe)
---------
1. 이 폴더에서 phone2pad_tray.exe 를 더블클릭합니다.
   - 콘솔 창 없이 작업 표시줄 알림 영역(트레이)에 phone2pad 아이콘으로 상주합니다.
   - Windows 작업표시줄 오른쪽 아래의 숨겨진 아이콘(^) 영역에서 phone2pad 아이콘을 찾고 클릭하세요.
   - 트레이 아이콘에 마우스를 올리면 현재 상태가 보입니다:
     대기 중 / 폰 연결 대기 / 앱 시작 대기 / 연결됨 - 트랙패드 사용 중.
   - 사용하지 않을 때는 거의 0에 가까운 CPU만 사용하며, 폰 앱이 트랙패드 모드에
     들어올 때만 입력 파이프라인이 동작합니다.
2. 트레이 아이콘을 클릭하면 메뉴가 열립니다:
   - 서비스 시작 / 서비스 정지 : 서비스 시작·정지
   - 사용 방법 열기            : 이 폴더의 QUICKSTART.md 열기
   - Windows 시작 시 자동 실행  : 로그인 시 자동 시작 켜기/끄기 (체크 표시)
   - 종료                      : 트레이 종료
3. 폰을 USB로 연결 → USB 디버깅 허용 → 폰 앱에서 "트랙패드 모드 시작"을 누르면
   화면이 검게 변하고 상태가 "연결됨 - 트랙패드 사용 중"으로 바뀌며 트랙패드로 동작합니다.

사용 방법 — 대체: 콘솔 클라이언트 (phone2pad_client.exe)
---------
1. 이 폴더에서 phone2pad_client.exe 를 실행합니다.
   - client는 adb forward만 설정하고 연결을 대기합니다 (폰 앱을 자동 실행하지 않음).
   - "Open phone2pad on your Android phone and tap Trackpad Mode Start." 안내가
     보이면 정상입니다. 종료하려면 Ctrl+C.
2. 폰을 USB 케이블로 PC에 연결합니다.
3. 폰에서 USB 디버깅을 허용합니다.
4. 폰의 phone2pad 앱을 열고 "트랙패드 모드 시작"을 누르면 화면이 검게 변하고,
   PC client가 연결되어 트랙패드로 동작합니다.

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
