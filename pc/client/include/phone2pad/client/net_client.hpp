// TCP connection runner (Windows/WinSock). Connects to the adb-forwarded phone
// server and pumps bytes into a FrameReceiver. Shared by the client and recorder.
#pragma once

#include <atomic>
#include <functional>

#include "phone2pad/client/frame_receiver.hpp"

namespace phone2pad::client {

// Connect to 127.0.0.1:port, feed recv() into `receiver`, and send a PING every
// pingIntervalMs. Blocks until the peer disconnects or `stop` is set. On exit a
// single all-lift frame is emitted (cursor stop). Returns false if connect failed.
//
// `receivedData` (optional out) is set true iff at least one byte was received.
// With adb forward, connect() succeeds even when the phone app isn't listening yet
// (the forward accepts, then closes with zero bytes); callers use this to tell a
// real session from that idle "waiting for the app" case.
//
// `onFirstData` (optional) is invoked once, on the receiving thread, the moment the
// first byte arrives — i.e. the phone app has actually started streaming. The tray
// uses it to flip to a "Connected" status mid-session (the function otherwise blocks
// for the whole session, so the data signal isn't observable until it returns).
bool runClientConnection(FrameReceiver& receiver, int port, std::atomic<bool>& stop,
                         int pingIntervalMs = 1000, bool* receivedData = nullptr,
                         std::function<void()> onFirstData = {});

}  // namespace phone2pad::client
