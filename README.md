<div align="center">

# phone2pad

### Use your Android phone as a Windows touchpad.

**USB-first · Local-only · No account · No cloud**

<br />

[Download](https://github.com/danielcho02/phone2pad/releases) ·
[Quick Start](QUICKSTART.md) ·
[Privacy](PRIVACY.md) ·
[Changelog](CHANGELOG.md)

</div>

---

## Overview

**phone2pad** turns an Android phone into a Windows touchpad through a local USB/ADB connection.

Run the Windows client, open the Android app, tap **Trackpad Mode Start**, and your phone becomes a black touch surface for your PC.

> **Coming in v0.3.0:** The Windows side can run as a lightweight **tray companion** (`phone2pad_tray.exe`) that stays in the background with optional autostart-on-login. Find the **phone2pad** icon in the **hidden icons (^) area** at the bottom-right of the Windows taskbar (near the clock) and click it for status and controls. (Built and in testing — not yet in the latest released download below.)
> (한국어: Windows 작업표시줄 오른쪽 아래의 숨겨진 아이콘(^) 영역에서 phone2pad 아이콘을 찾고 클릭하세요.)

> **v0.2.2 alpha / pre-release**
> This is an early public release. It is usable for testing and feedback, but features, UX, and implementation details may change.

---

## Why phone2pad?

Most phone-as-mouse apps depend on Wi-Fi pairing, background services, accounts, or cloud features.

phone2pad starts with a simpler workflow:

1. Connect your phone with USB.
2. Run a small Windows client.
3. Start trackpad mode manually from the Android app.
4. Send touch input locally over USB/ADB.

No account. No cloud server. No remote backend.

---

## Features

| Gesture               | Action                                 |
| --------------------- | -------------------------------------- |
| One-finger move       | Move cursor                            |
| One-finger tap        | Left click                             |
| One-finger long press | Drag                                   |
| Two-finger scroll     | Scroll vertically / horizontally       |
| Two-finger tap        | Right click                            |
| Two-finger pinch      | Zoom                                   |
| Three-finger gestures | Task View, app switching, show desktop |
| Four-finger gestures  | Virtual desktop switching              |

---

## Download

Download the latest alpha release from:

**[GitHub Releases](https://github.com/danielcho02/phone2pad/releases)**

For **v0.2.2 alpha**, use these files:

| File                               | Description                   |
| ---------------------------------- | ----------------------------- |
| `phone2pad-windows-x64-v0.2.2.zip` | Windows x64 client            |
| `phone2pad-android-v0.2.2-apk.zip` | Android APK packaged as a zip |
| `SHA256SUMS.txt`                   | SHA256 checksums              |

> GitHub web uploads may reject raw `.apk` files.
> Download `phone2pad-android-v0.2.2-apk.zip`, unzip it, then install the APK inside.

---

## Quick Start

For the full setup guide, see **[QUICKSTART.md](QUICKSTART.md)**.

### 1. Prepare

You need:

* Windows 10/11 x64
* Android 9 or later
* USB data cable
* Android USB debugging enabled
* Android Platform Tools / `adb`

phone2pad does not ship Google's Platform Tools. The client finds `adb`
automatically — pick whichever is easier:

* **Install normally:** install Android Platform Tools (or Android Studio) so `adb`
  is on your `PATH`. Verify with `adb version`.
* **Drop it next to the client:** download Platform Tools and unzip them into a
  `tools\platform-tools` folder beside `phone2pad_client.exe` (so that
  `…\tools\platform-tools\adb.exe` exists). No `PATH` setup needed.

If `adb` is missing, the client prints the download link and setup steps and exits.
`adb` is used only for the local USB link between phone and PC — nothing goes online.

### 2. Install the Android app

1. Download `phone2pad-android-v0.2.2-apk.zip`.
2. Unzip it.
3. Install the APK on your Android phone.
4. Allow installation from unknown sources if Android asks.

### 3. Run the Windows client

1. Download `phone2pad-windows-x64-v0.2.2.zip`.
2. Unzip it.
3. Run:

```powershell
phone2pad_client.exe
```

The client reports where it found `adb`, then waits for your phone. When the phone
is connected and authorized you should see:

```text
Open phone2pad on your Android phone and tap Trackpad Mode Start.
```

If something is off, the client says what to fix (no phone detected, device not
authorized, more than one phone connected, etc.).

### 4. Start trackpad mode

1. Connect your Android phone to the PC with USB.
2. Accept the USB debugging prompt on the phone.
3. Open the phone2pad app.
4. Tap **Trackpad Mode Start**.
5. When the phone screen turns black, use it as a touchpad.

---

## How It Works

phone2pad has two main parts:

| Part           | Role                                                    |
| -------------- | ------------------------------------------------------- |
| Android app    | Captures touch input from the phone screen              |
| Windows client | Receives touch frames and injects mouse / gesture input |

The current release uses this flow:

```text
Android touch input
→ phone2pad Android app
→ USB / ADB local forwarding
→ phone2pad Windows client
→ Windows user-mode input
```

Native Windows Precision Touchpad driver support is planned for a later phase.

---

## Tech Stack

| Area           | Technology                         |
| -------------- | ---------------------------------- |
| Android app    | Kotlin, Android SDK                |
| Windows client | C++, Win32                         |
| Protocol       | Custom binary touch-frame protocol |
| Transport      | USB / ADB forwarding               |
| Build          | Gradle, CMake, MSBuild             |
| Packaging      | PowerShell                         |
| License        | MIT                                |

---

## Current Limitations

phone2pad is still an alpha release.

* USB/ADB only
* Wi-Fi and Bluetooth are not supported yet
* Native Windows Precision Touchpad driver is not included yet
* Android USB debugging is required
* Gesture behavior may differ from a real laptop touchpad
* The Android APK is distributed as a zip file in GitHub Releases

---

## Security and Privacy

phone2pad is designed to stay local.

* No account required
* No cloud server
* No analytics
* No remote backend
* Touch input is sent over local USB/ADB forwarding
* Touch data is not uploaded or stored by phone2pad

Only download APKs from the official GitHub Releases page.

For more details, see **[PRIVACY.md](PRIVACY.md)**.

---

## Roadmap

* [x] USB/ADB touch streaming
* [x] Windows user-mode cursor movement
* [x] Basic multi-touch gestures
* [x] Release packaging for Windows and Android
* [x] Better diagnostics for ADB/device issues
* [ ] **v0.3.0** — Reduced launch friction: background / tray / autostart UX (no manual client launch each time) — *implemented; pending the v0.3.0 Release*
* [ ] **v0.4.0+** — Wi-Fi / Bluetooth transport, native Windows Precision Touchpad driver, major input-mode research

---

## Documentation

| Document                       | Description                 |
| ------------------------------ | --------------------------- |
| [QUICKSTART.md](QUICKSTART.md) | End-user setup guide        |
| [RELEASE.md](RELEASE.md)       | Release and packaging notes |
| [PRIVACY.md](PRIVACY.md)       | Privacy policy              |
| [CHANGELOG.md](CHANGELOG.md)   | Version history             |
| [LICENSE](LICENSE)             | MIT License                 |

---

## Build From Source

High-level requirements:

* Windows 10/11
* Android Studio or Android SDK
* Android Platform Tools
* JDK compatible with the Android Gradle Plugin
* CMake
* MSBuild / Visual Studio Build Tools

For release packaging details, see **[RELEASE.md](RELEASE.md)**.

---

## Contributing

Issues, bug reports, and testing feedback are welcome.

When reporting a problem, please include:

* Windows version
* Android device model
* Android version
* USB cable / hub setup
* What worked or failed
* Windows client console output, if available

---

## License

phone2pad is released under the **MIT License**.

See **[LICENSE](LICENSE)** for details.

<div align="center">

<br />

**Made for people who have a phone, a cable, and no touchpad.**

</div>
