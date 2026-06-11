<div align="center">

# phone2pad

### Turn your Android phone into a Windows touchpad.

<p>
  <a href="https://github.com/danielcho02/phone2pad/releases">
    <img alt="GitHub release" src="https://img.shields.io/github/v/release/danielcho02/phone2pad?include_prereleases&style=for-the-badge&label=release">
  </a>
  <a href="https://github.com/danielcho02/phone2pad/blob/main/LICENSE">
    <img alt="License" src="https://img.shields.io/github/license/danielcho02/phone2pad?style=for-the-badge">
  </a>
  <img alt="Status" src="https://img.shields.io/badge/status-alpha-orange?style=for-the-badge">
  <img alt="Platform" src="https://img.shields.io/badge/Windows%20%2B%20Android-USB%2FADB-blue?style=for-the-badge">
</p>

<p>
  <strong>phone2pad</strong> lets you use an Android phone as a touchpad for Windows through a local USB/ADB connection.
</p>

<p>
  <a href="#download">Download</a>
  ·
  <a href="#quick-start">Quick Start</a>
  ·
  <a href="#features">Features</a>
  ·
  <a href="#architecture">Architecture</a>
  ·
  <a href="#roadmap">Roadmap</a>
</p>

</div>

---

> [!WARNING]
> **v0.2.0 is an alpha / pre-release.**  
> It works as a USB/ADB-based touchpad MVP, but the UX, protocol, and implementation may change.

## Preview

```text
Android phone              USB / ADB              Windows PC
┌────────────────┐      local forwarding       ┌─────────────────────┐
│ phone2pad app  │ ─────────────────────────▶ │ phone2pad_client.exe │
│ black touchpad │                            │ user-mode input      │
└────────────────┘                            └─────────────────────┘
```

Open the Android app, tap **Trackpad Mode Start**, and the phone becomes a black touch surface for your PC.

---

## Why phone2pad?

Most “phone as mouse” apps rely on Wi-Fi, pairing flows, accounts, or background services.

phone2pad starts from a simpler idea:

- plug in your phone
- run a tiny Windows client
- start trackpad mode manually
- send touch input locally over USB/ADB

No account. No cloud server. No remote backend.

---

## Features

| Input | Action |
|---|---|
| One-finger move | Move cursor |
| One-finger tap | Left click |
| One-finger long press | Drag |
| Two-finger scroll | Scroll vertically / horizontally |
| Two-finger tap | Right click |
| Two-finger pinch | Zoom |
| Three-finger gestures | Task View, app switching, show desktop |
| Four-finger gestures | Virtual desktop switching |

---

## Download

Get the latest release here:

<div align="center">

### [Download from GitHub Releases](https://github.com/danielcho02/phone2pad/releases)

</div>

For **v0.2.0 alpha**, use these assets:

| File | Use |
|---|---|
| `phone2pad-windows-x64-v0.2.0.zip` | Windows client |
| `phone2pad-android-v0.2.0-apk.zip` | Android APK packaged as a zip |
| `SHA256SUMS.txt` | Checksums for release assets |

> [!NOTE]
> GitHub web uploads may reject raw `.apk` files.  
> Download `phone2pad-android-v0.2.0-apk.zip`, unzip it, then install the APK inside.

---

## Quick Start

For a more detailed guide, see [`QUICKSTART.md`](QUICKSTART.md).

### 1. Install prerequisites

You need:

- Windows 10/11 x64
- Android 9 or later
- USB data cable
- Android USB debugging enabled
- Android Platform Tools / `adb`

Check ADB:

```powershell
adb version
```

### 2. Install the Android app

1. Download `phone2pad-android-v0.2.0-apk.zip`.
2. Unzip it.
3. Install the APK on your Android phone.
4. Allow “Install unknown apps” if Android asks.

### 3. Run the Windows client

1. Download `phone2pad-windows-x64-v0.2.0.zip`.
2. Unzip it.
3. Run:

```powershell
phone2pad_client.exe
```

The client waits for the Android app:

```text
Open phone2pad on your Android phone and tap Trackpad Mode Start.
```

### 4. Start trackpad mode

1. Connect the phone to the PC with USB.
2. Accept the USB debugging prompt.
3. Open the phone2pad app.
4. Tap **Trackpad Mode Start**.
5. When the phone screen turns black, use it as a touchpad.

---

## Architecture

```text
┌─────────────────────────────────────────────────────────────────┐
│ Android                                                         │
│                                                                 │
│  MainActivity                                                   │
│      └─ user opens app and taps "Trackpad Mode Start"            │
│                                                                 │
│  BlackPadActivity                                               │
│      └─ captures touch input from the full-screen black surface  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ Touch frames
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ USB / ADB forwarding                                             │
│                                                                 │
│  local transport between the connected phone and Windows PC       │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Windows                                                         │
│                                                                 │
│  phone2pad_client.exe                                            │
│      ├─ receives touch frames                                    │
│      ├─ maps gestures                                            │
│      └─ injects user-mode mouse / gesture input                  │
└─────────────────────────────────────────────────────────────────┘
```

Current release uses **user-mode input**.  
Native Windows Precision Touchpad driver support is planned for a later phase.

---

## Tech Stack

| Layer | Stack |
|---|---|
| Android app | Kotlin, Android SDK |
| Windows client | C++, Win32 |
| Protocol | Custom binary touch-frame protocol |
| Transport | USB / ADB forwarding |
| Build | Gradle, CMake, MSBuild |
| Release packaging | PowerShell |
| License | MIT |

---

## Current Limitations

phone2pad is still early.

- USB/ADB only
- Wi-Fi and Bluetooth are not supported yet
- Native Windows Precision Touchpad driver is not included yet
- Android USB debugging is required
- Gesture behavior may differ from a real laptop touchpad
- The Android APK is distributed as a zip file in GitHub Releases

---

## Security and Privacy

phone2pad is designed to stay local.

- No account required
- No cloud server
- No analytics
- No remote backend
- Touch input is sent over local USB/ADB forwarding
- Touch data is not uploaded or stored by phone2pad

Only download APKs from the official GitHub Releases page.

See [`PRIVACY.md`](PRIVACY.md) for details.

---

## Roadmap

- [x] USB/ADB touch streaming
- [x] Windows user-mode cursor movement
- [x] Basic multi-touch gestures
- [x] Release packaging for Windows and Android
- [ ] Better diagnostics for ADB/device issues
- [ ] Gesture tuning and UX polish
- [ ] Wi-Fi / Bluetooth transport exploration
- [ ] Native Windows Precision Touchpad driver research
- [ ] Installer / setup experience improvements

---

## Documentation

| Document | Description |
|---|---|
| [`QUICKSTART.md`](QUICKSTART.md) | End-user setup guide |
| [`RELEASE.md`](RELEASE.md) | Release and packaging notes |
| [`PRIVACY.md`](PRIVACY.md) | Privacy policy |
| [`CHANGELOG.md`](CHANGELOG.md) | Version history |
| [`LICENSE`](LICENSE) | MIT License |

---

## Build From Source

This project currently targets Windows + Android development environments.

High-level requirements:

- Windows 10/11
- Android Studio or Android SDK
- Android Platform Tools
- JDK compatible with the Android Gradle Plugin
- CMake
- MSBuild / Visual Studio Build Tools

For release packaging details, see [`RELEASE.md`](RELEASE.md).

---

## Contributing

Issues, bug reports, and testing feedback are welcome.

When reporting a problem, please include:

- Windows version
- Android device model
- Android version
- USB cable / hub setup
- What you expected to happen
- What actually happened
- Windows client console output, if available

---

## License

phone2pad is released under the [MIT License](LICENSE).

<div align="center">

Made for people who have a phone, a cable, and no touchpad.

</div>
