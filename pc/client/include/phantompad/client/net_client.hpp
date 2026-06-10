// TCP connection runner (Windows/WinSock). Connects to the adb-forwarded phone
// server and pumps bytes into a FrameReceiver. Shared by the client and recorder.
#pragma once

#include <atomic>

#include "phantompad/client/frame_receiver.hpp"

namespace phantompad::client {

// Connect to 127.0.0.1:port, feed recv() into `receiver`, and send a PING every
// pingIntervalMs. Blocks until the peer disconnects or `stop` is set. On exit a
// single all-lift frame is emitted (cursor stop). Returns false if connect failed.
bool runClientConnection(FrameReceiver& receiver, int port, std::atomic<bool>& stop,
                         int pingIntervalMs = 1000);

}  // namespace phantompad::client
