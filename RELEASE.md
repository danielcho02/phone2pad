# RELEASE — phone2pad 배포 절차 (메인테이너용)

phone2pad의 GitHub Release(Windows zip + Android APK)와 Google Play Internal
testing(AAB)을 만드는 절차입니다.

라이선스: **MIT** ([LICENSE](LICENSE)).

---

## 0. 산출물 개요

| 산출물 | 용도 | 빌드 |
|---|---|---|
| `phone2pad-windows-x64-vX.Y.Z.zip` | Windows client (self-contained) | `cmake --build ... --config Release` (정적 CRT) |
| `phone2pad-android-vX.Y.Z.apk` | GitHub Release용 **release-signed** APK | `:app:assembleRelease` |
| `phone2pad-android-vX.Y.Z.aab` | Google Play 업로드 전용 | `:app:bundleRelease` |
| `SHA256SUMS.txt` | 무결성 체크섬 | `package-release.ps1` |

> **debug APK는 개발/테스트 전용**이며 공개 release asset으로 첨부하지 않습니다.

## 1. Android release 서명 준비 (최초 1회)

1. 업로드 키스토어를 한 번 생성합니다(생성된 `.jks`는 **절대 커밋 금지**):
   ```powershell
   keytool -genkeypair -v -keystore phone2pad-release.jks `
     -alias phone2pad -keyalg RSA -keysize 2048 -validity 10000
   ```
2. `android/keystore.properties.example`를 복사해 `android/keystore.properties`를 만들고 값을 채웁니다:
   ```properties
   storeFile=phone2pad-release.jks
   storePassword=...
   keyAlias=phone2pad
   keyPassword=...
   ```
   - `storeFile`은 `android/` 기준 상대경로입니다.
   - `keystore.properties`, `*.jks`, `*.keystore`는 `.gitignore`에 포함되어 있습니다.
     **키 파일과 비밀번호는 절대 커밋하지 마세요.**
3. keystore가 없으면 release 빌드는 unsigned로 떨어지고, `package-release.ps1`은
   Android asset 생성을 SKIP합니다(빌드는 깨지지 않습니다).

## 2. 빌드/패키징

```powershell
# (선택) 회귀 테스트
./scripts/test-all.ps1

# 배포 패키징 — dist/ 에 zip / (키 있으면) apk·aab / SHA256SUMS.txt 생성
./scripts/package-release.ps1 -Version 0.2.0
```

`package-release.ps1` 동작:
1. PC client를 Release(정적 CRT)로 빌드 → `phone2pad_client/recorder/replay`
2. Windows zip 구성(+ `README.txt`, `LICENSE`)
3. `keystore.properties`가 있으면 `:app:assembleRelease` + `:app:bundleRelease`,
   없으면 경고 후 SKIP
4. `SHA256SUMS.txt` 생성

자체 완결성 점검(권장): zip을 깨끗한 Windows에서 풀어 `phone2pad_client.exe` 실행
(client는 `WS2_32/USER32/KERNEL32`만 의존, VC++ 재배포 불필요).

## 3. GitHub Release

배포용 태그는 SemVer(`vX.Y.Z`)를 사용합니다. (기존 `vN-phase-*` 마일스톤 태그와 별개)

```powershell
git tag -a v0.2.0 -m "phone2pad v0.2.0 — USB/ADB user-mode MVP"
git push origin v0.2.0

gh release create v0.2.0 `
  --title "phone2pad v0.2.0" `
  --notes-file release-notes.md `
  dist/phone2pad-windows-x64-v0.2.0.zip `
  dist/phone2pad-android-v0.2.0.apk `
  dist/SHA256SUMS.txt
```

### Release notes 초안

```markdown
# phone2pad v0.2.0

USB-C로 연결한 Android 폰을 Windows 트랙패드로 사용하는 user-mode MVP입니다.

## 동작
- 1손가락: 이동 / 탭 = 좌클릭 / 길게 눌러 드래그
- 2손가락: 스크롤 / 탭 = 우클릭 / 핀치 = 확대·축소
- 3손가락: Task View / 바탕화면 / Alt+Tab
- 4손가락: 가상 데스크탑 전환

## 현재 한계
- USB 전용 (Wi-Fi / Bluetooth 미지원)
- Windows 10/11 x64 전용
- 아직 네이티브 Precision Touchpad가 아니라 user-mode 입력 주입 방식 (네이티브 PTP는 추후)

## 설치
QUICKSTART.md 참고. adb(Android Platform Tools)가 별도로 필요합니다.

## 체크섬
SHA256SUMS.txt 참고.
```

## 4. Google Play Internal testing

지금 단계는 public 출시가 아니라 **Internal testing**을 권장합니다.

준비물:
- **AAB**: `:app:bundleRelease` 산출물(`dist/phone2pad-android-vX.Y.Z.aab`)
- **Play App Signing**: 업로드 키(§1의 keystore)로 서명해 업로드, 배포 서명은 Google이 관리
- **Data safety**: 현재 앱은 네트워크가 localhost(adb forward) 전용, 외부 서버 전송 없음,
  개인정보 수집 없음 → 그대로(과장 없이) 기재 ([PRIVACY.md](PRIVACY.md) 근거)
- **Privacy policy URL**: `PRIVACY.md` 내용을 호스팅한 URL (예: GitHub Pages)
- **스토어 등록물**: 짧은/긴 설명, 스크린샷(MainActivity 화면), 512px 아이콘

순서:
1. Play Console에서 앱 생성 (패키지명 `com.phone2pad`)
2. App signing(업로드 키 등록) 설정
3. Internal testing 트랙에 AAB 업로드
4. Data safety / 콘텐츠 등급 / 타깃 연령 / Privacy policy URL 입력
5. 내부 테스터 이메일 목록 추가 → 공유 링크로 설치

> Play 정책상 신규/업데이트 앱은 비교적 최근 API 레벨을 target 해야 합니다.
> 현재 `targetSdk = 36`으로 충족합니다(정확한 요구 레벨은 Play Console 안내로 확인).

## 5. 보안/주의

- keystore 파일·비밀번호·`local.properties`·빌드 산출물·`dist/`는 커밋하지 않습니다.
- Phase C(드라이버/HID/WDK/VHF), Wi-Fi/Bluetooth는 이 릴리스 범위 밖입니다.
