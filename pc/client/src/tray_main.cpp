// phone2pad_tray.exe (v0.3.0): a lightweight Win32 tray companion. It drives the
// shared ClientService (the same adb/forward/stream/inject pipeline the CLI uses) so
// the user can keep phone2pad ready in the background instead of running a console
// app each time, with optional autostart-on-login.
//
// Deliberately minimal: pure Shell_NotifyIcon + a context menu, no GUI framework.
// All heavy lifting lives in ClientService; this file is just UI + lifecycle glue.
// The service runs on its own thread and reports state via a callback that we marshal
// onto the UI thread with PostMessage (Shell_NotifyIcon must be driven from the
// thread that owns the icon's window).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <string>

#include "phone2pad/client/autostart.hpp"
#include "phone2pad/client/client_service.hpp"
#include "tray_resource.h"

using namespace phone2pad::client;

namespace {

// Tray icon callback + state-change marshalling messages.
constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT WM_STATE_CHANGED = WM_APP + 2;  // wParam = ServiceState
constexpr UINT kTrayUid = 1;

// Context-menu command IDs.
constexpr UINT ID_STATUS = 1001;
constexpr UINT ID_TOGGLE_SERVICE = 1002;
constexpr UINT ID_OPEN_GUIDE = 1003;
constexpr UINT ID_TOGGLE_AUTOSTART = 1004;
constexpr UINT ID_EXIT = 1005;

constexpr wchar_t kWindowClass[] = L"phone2pad_tray_wnd";
constexpr wchar_t kSingletonMutex[] = L"phone2pad_tray_singleton_b6f1";
constexpr wchar_t kGuideUrl[] =
    L"https://github.com/danielcho02/phone2pad/blob/main/QUICKSTART.md";

HWND g_wnd = nullptr;
ClientService* g_service = nullptr;
ServiceState g_state = ServiceState::Stopped;
NOTIFYICONDATAW g_nid{};
HICON g_icon = nullptr;  // the phone2pad logo icon (tray + balloon)

// (UTF-8 source via /utf-8; user-facing strings are Korean per CLAUDE.md.)
//
// User-facing Korean status label for the tray (tooltip + menu). Grouped like
// the engine's shortLabel buckets; kept tray-local so the engine API / CLI stay
// English. (CLAUDE.md: user-facing text is Korean.)
std::wstring trayStatusLabel(ServiceState s) {
    switch (s) {
        case ServiceState::Stopped:
        case ServiceState::NoDevice:
            return L"대기 중";
        case ServiceState::AdbMissing:
            return L"ADB를 찾을 수 없음";
        case ServiceState::Unauthorized:
        case ServiceState::DeviceOffline:
        case ServiceState::MultipleDevices:
        case ServiceState::ForwardFailed:
            return L"폰 연결 대기";
        case ServiceState::WaitingForApp:
            return L"앱 시작 대기";
        case ServiceState::Connected:
            return L"연결됨 - 트랙패드 사용 중";
    }
    return L"대기 중";
}

// Korean balloon body, or empty = no balloon. Empty for routine idle ticks
// (Stopped/NoDevice/DeviceOffline) so the corner isn't spammed.
std::wstring trayBalloonText(ServiceState s) {
    switch (s) {
        case ServiceState::Connected:
            return L"phone2pad 연결됨. 이제 폰을 트랙패드로 사용할 수 있습니다.";
        case ServiceState::WaitingForApp:
            return L"폰 앱에서 트랙패드 모드 시작을 눌러주세요.";
        case ServiceState::Unauthorized:
            return L"폰에서 USB 디버깅 허용을 눌러주세요.";
        case ServiceState::AdbMissing:
            return L"ADB를 찾을 수 없습니다. 설정 안내를 확인하세요.";
        case ServiceState::MultipleDevices:
            return L"폰이 여러 대 연결되어 있습니다. 하나만 남겨주세요.";
        case ServiceState::ForwardFailed:
            return L"USB 연결 설정에 실패했습니다. 케이블을 다시 연결해주세요.";
        default:
            return std::wstring();
    }
}

void setTooltip(const std::wstring& text) {
    g_nid.uFlags = NIF_TIP | NIF_SHOWTIP;
    wcsncpy_s(g_nid.szTip, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void showBalloon(const std::wstring& title, const std::wstring& msg) {
    g_nid.uFlags = NIF_INFO;
    // Show the phone2pad logo in the notification when available.
    g_nid.dwInfoFlags = (g_icon != nullptr) ? NIIF_USER : NIIF_INFO;
    g_nid.hBalloonIcon = g_icon;
    wcsncpy_s(g_nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(g_nid.szInfo, msg.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void onStateChanged(ServiceState s) {
    g_state = s;
    setTooltip(L"phone2pad - " + trayStatusLabel(s));
    const std::wstring balloon = trayBalloonText(s);
    if (!balloon.empty()) showBalloon(L"phone2pad", balloon);
}

// Open the bundled QUICKSTART.md next to the exe; fall back to the GitHub copy.
void openSetupGuide() {
    const std::wstring exe = autostart::currentExePath();
    const std::size_t pos = exe.find_last_of(L"\\/");
    const std::wstring dir = (pos == std::wstring::npos) ? std::wstring()
                                                         : exe.substr(0, pos + 1);
    const std::wstring guide = dir + L"QUICKSTART.md";
    const DWORD attrs = GetFileAttributesW(guide.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        ShellExecuteW(nullptr, L"open", guide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else {
        ShellExecuteW(nullptr, L"open", kGuideUrl, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void showContextMenu() {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) return;

    const std::wstring status = L"Status: " + trayStatusLabel(g_state);
    AppendMenuW(menu, MF_STRING | MF_GRAYED, ID_STATUS, status.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    const bool running = g_service != nullptr && g_service->running();
    AppendMenuW(menu, MF_STRING, ID_TOGGLE_SERVICE,
                running ? L"서비스 정지" : L"서비스 시작");
    AppendMenuW(menu, MF_STRING, ID_OPEN_GUIDE, L"사용 방법 열기");

    const UINT autoFlags = MF_STRING | (autostart::isEnabled() ? MF_CHECKED : MF_UNCHECKED);
    AppendMenuW(menu, autoFlags, ID_TOGGLE_AUTOSTART, L"Windows 시작 시 자동 실행");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_EXIT, L"종료");

    // Required so the menu dismisses when the user clicks elsewhere (Win32 quirk).
    SetForegroundWindow(g_wnd);
    POINT pt;
    GetCursorPos(&pt);
    const UINT cmd = static_cast<UINT>(TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, g_wnd, nullptr));
    DestroyMenu(menu);

    switch (cmd) {
        case ID_TOGGLE_SERVICE:
            if (g_service != nullptr) {
                if (g_service->running()) {
                    g_service->stop();
                } else {
                    g_service->start();
                }
            }
            break;
        case ID_OPEN_GUIDE:
            openSetupGuide();
            break;
        case ID_TOGGLE_AUTOSTART:
            autostart::setEnabled(!autostart::isEnabled());
            break;
        case ID_EXIT:
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            if (g_service != nullptr) g_service->stop();
            PostQuitMessage(0);
            break;
        default:
            break;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAYICON:
            // With NOTIFYICON_VERSION_4 the event is the low word of lParam.
            switch (LOWORD(lParam)) {
                case WM_CONTEXTMENU:
                case WM_LBUTTONUP:
                case NIN_SELECT:
                    showContextMenu();
                    break;
                default:
                    break;
            }
            return 0;
        case WM_STATE_CHANGED:
            onStateChanged(static_cast<ServiceState>(wParam));
            return 0;
        case WM_DESTROY:
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // Single instance: autostart + a manual launch must not run two copies.
    HANDLE singleton = CreateMutexW(nullptr, TRUE, kSingletonMutex);
    if (singleton == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kWindowClass;
    if (RegisterClassExW(&wc) == 0) return 1;

    // Message-only window: no taskbar/visible window, just a sink for tray callbacks.
    g_wnd = CreateWindowExW(0, kWindowClass, L"phone2pad", 0, 0, 0, 0, 0, HWND_MESSAGE,
                            nullptr, hInstance, nullptr);
    if (g_wnd == nullptr) return 1;

    // Load the phone2pad logo (embedded via tray.rc). A small icon sized for the tray;
    // falls back to the stock app icon only if the resource is somehow missing.
    g_icon = static_cast<HICON>(LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_PHONE2PAD),
                                           IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                           GetSystemMetrics(SM_CYSMICON), 0));
    if (g_icon == nullptr) g_icon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));

    // Add the tray icon with the phone2pad logo so users can recognize it in the
    // system tray / hidden-icons (^) area.
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_wnd;
    g_nid.uID = kTrayUid;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_icon;
    wcsncpy_s(g_nid.szTip, L"phone2pad - 대기 중", _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);

    // Build the service and marshal its (worker-thread) state callbacks onto this UI
    // thread via PostMessage, then start it — "stay ready" is the whole point.
    ClientService service;
    g_service = &service;
    service.setStateCallback([](ServiceState s) {
        PostMessageW(g_wnd, WM_STATE_CHANGED, static_cast<WPARAM>(s), 0);
    });
    if (!service.adbReady()) {
        // No adb: surface it immediately; the service will sit idle until Exit.
        onStateChanged(ServiceState::AdbMissing);
    }
    service.start();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    service.stop();
    g_service = nullptr;
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    ReleaseMutex(singleton);
    CloseHandle(singleton);
    return 0;
}
