; phone2pad Windows installer (Inno Setup 6).
;
; Per-user install, NO admin required. It packages the SAME staged payload as the
; portable zip (produced by scripts/package-release.ps1, Stage 2). The packager
; passes the staged path and version as preprocessor defines:
;
;   ISCC.exe /DMyVersion=0.3.0 ^
;            /DStageDir=<repo>\dist\phone2pad-windows-x64-v0.3.0 ^
;            /DRepoRoot=<repo> ^
;            scripts\installer\phone2pad.iss
;
; Deliberately does NOT:
;   * modify PATH,
;   * bundle or auto-download adb / platform-tools (the tray's first-run ADB setup
;     gate handles that: download page -> place platform-tools beside the exe ->
;     "ADB 다시 확인"),
;   * create a login-Startup entry (the tray's own "Windows 시작 시 자동 실행"
;     menu owns HKCU\...\Run, to avoid two competing autostart mechanisms).
;
; Wizard/shortcut strings are ASCII (English) on purpose: Inno 6 renders Korean
; reliably only from a UTF-8 *with BOM* source, which the build toolchain here
; can't guarantee. The user-facing Korean docs (QUICKSTART.md / README.txt /
; ADB-SETUP.md) ship alongside and are opened from the tray menu.

#ifndef MyVersion
  #define MyVersion "0.3.0"
#endif
#ifndef RepoRoot
  #define RepoRoot "..\.."
#endif
#ifndef StageDir
  #define StageDir RepoRoot + "\dist\phone2pad-windows-x64-v" + MyVersion
#endif

[Setup]
; Stable AppId so upgrades/uninstall track correctly across versions. Do not change.
AppId={{8F2A6C13-7E4B-4D9A-AC15-2B6E9D3F1A07}
AppName=phone2pad
AppVersion={#MyVersion}
AppVerName=phone2pad {#MyVersion}
AppPublisher=phone2pad
AppPublisherURL=https://github.com/danielcho02/phone2pad
AppSupportURL=https://github.com/danielcho02/phone2pad
DefaultDirName={autopf}\phone2pad
DefaultGroupName=phone2pad
DisableProgramGroupPage=yes
; Per-user by default, no admin. The user may still opt into a machine-wide
; install via the elevation dialog if they have rights.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
; x64compatible = native x64 and ARM64 running x64 under emulation (Inno 6.3+
; preferred identifier; replaces the deprecated bare "x64").
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#RepoRoot}\dist
OutputBaseFilename=phone2pad-setup-v{#MyVersion}
SetupIconFile={#RepoRoot}\pc\client\assets\phone2pad.ico
UninstallDisplayIcon={app}\phone2pad_tray.exe
UninstallDisplayName=phone2pad {#MyVersion}
LicenseFile={#StageDir}\LICENSE
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes

[Files]
Source: "{#StageDir}\phone2pad_tray.exe";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\phone2pad_client.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\recorder.exe";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\replay.exe";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\README.txt";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\QUICKSTART.md";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\ADB-SETUP.md";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\LICENSE";              DestDir: "{app}"; Flags: ignoreversion

[Icons]
; Primary Start Menu shortcut (requirement): launches the tray companion.
Name: "{group}\phone2pad";  Filename: "{app}\phone2pad_tray.exe"; Comment: "phone2pad"
Name: "{group}\Quick Start (QUICKSTART)"; Filename: "{app}\QUICKSTART.md"
Name: "{group}\{cm:UninstallProgram,phone2pad}"; Filename: "{uninstallexe}"

[Run]
; Optional "Start phone2pad after install" checkbox on the final page.
; 'skipifsilent' guarantees the tray is NOT launched during silent verification.
Filename: "{app}\phone2pad_tray.exe"; Description: "Start phone2pad"; Flags: nowait postinstall skipifsilent
