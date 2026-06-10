#include "phantompad/client/net_client.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace phantompad::client {

namespace {

struct WsaScope {
    bool ok = false;
    WsaScope() {
        WSADATA d{};
        ok = (WSAStartup(MAKEWORD(2, 2), &d) == 0);
    }
    ~WsaScope() {
        if (ok) WSACleanup();
    }
};

bool sendAll(SOCKET s, const std::uint8_t* data, std::size_t n) {
    std::size_t sent = 0;
    while (sent < n) {
        const int r = send(s, reinterpret_cast<const char*>(data + sent),
                           static_cast<int>(n - sent), 0);
        if (r <= 0) return false;
        sent += static_cast<std::size_t>(r);
    }
    return true;
}

}  // namespace

bool runClientConnection(FrameReceiver& receiver, int port, std::atomic<bool>& stop,
                         int pingIntervalMs) {
    WsaScope wsa;
    if (!wsa.ok) return false;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return false;
    }

    BOOL nodelay = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay),
               sizeof(nodelay));
    // recv() wakes every 200ms so we can send PINGs and observe `stop`.
    DWORD timeoutMs = 200;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs),
               sizeof(timeoutMs));

    using clock = std::chrono::steady_clock;
    auto lastPing = clock::now();
    std::vector<std::uint8_t> buf(8192);
    bool connected = true;

    while (!stop.load()) {
        const int n = recv(sock, reinterpret_cast<char*>(buf.data()),
                           static_cast<int>(buf.size()), 0);
        if (n > 0) {
            receiver.processBytes(std::span<const std::uint8_t>(buf.data(),
                                                                static_cast<std::size_t>(n)));
        } else if (n == 0) {
            break;  // peer closed
        } else {
            const int err = WSAGetLastError();
            if (err != WSAETIMEDOUT && err != WSAEWOULDBLOCK) {
                connected = false;
                break;
            }
        }

        const auto now = clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPing).count() >=
            pingIntervalMs) {
            const proto::Bytes ping = receiver.makePing();
            if (!sendAll(sock, ping.data(), ping.size())) break;
            lastPing = now;
        }
    }

    receiver.emitLift();  // cursor stop on disconnect (docs/01 §5)
    closesocket(sock);
    return connected;
}

}  // namespace phantompad::client

#endif  // _WIN32
