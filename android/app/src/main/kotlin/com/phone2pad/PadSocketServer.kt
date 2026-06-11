// TCP server on the phone (docs/01 §2.1, docs/02 §1). The phone listens; the PC
// connects via `adb forward`. Single connection only; on disconnect it goes back
// to waiting. TCP_NODELAY on, no batching — frames are written as they arrive.
package com.phone2pad

import com.phone2pad.proto.DecodedPacket
import com.phone2pad.proto.Hello
import com.phone2pad.proto.PacketEncoder
import com.phone2pad.proto.StreamDecoder
import com.phone2pad.proto.TouchFrame
import java.io.BufferedOutputStream
import java.io.IOException
import java.io.InputStream
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.BlockingQueue
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit

class PadSocketServer(private val port: Int = DEFAULT_PORT) {

    @Volatile private var hello: Hello? = null
    @Volatile private var running = false
    // Outgoing queue for the currently-connected client; null when none.
    @Volatile private var outQueue: BlockingQueue<ByteArray>? = null
    private var acceptThread: Thread? = null
    @Volatile private var serverSocket: ServerSocket? = null

    /** Start the accept loop. HELLO is (re)sent at the start of each connection. */
    fun start(initialHello: Hello) {
        hello = initialHello
        if (running) return
        running = true
        acceptThread = Thread(::acceptLoop, "pad-accept").apply { isDaemon = true; start() }
    }

    /** Update the stored HELLO without transmitting (used as the seed for new connections). */
    fun updateHello(updated: Hello) {
        hello = updated
    }

    /**
     * Push a fresh HELLO to the connected client (e.g. on a landscape<->reverse-landscape
     * rotation; docs/02 §3). Stores it as the seed too, and enqueues it for the current
     * connection so the PC re-reads the new rotation mid-session. No-op send if nobody is
     * connected (the stored value still seeds the next connection).
     */
    fun sendHello(updated: Hello) {
        hello = updated
        outQueue?.offer(PacketEncoder.encode(updated))
    }

    /** Enqueue a touch frame for the connected client. No-op if nobody is connected. */
    fun sendFrame(frame: TouchFrame) {
        outQueue?.offer(PacketEncoder.encode(frame))
    }

    fun stop() {
        running = false
        outQueue = null
        try {
            serverSocket?.close()
        } catch (_: IOException) {
        }
    }

    private fun acceptLoop() {
        while (running) {
            try {
                ServerSocket().use { ss ->
                    ss.reuseAddress = true
                    ss.bind(InetSocketAddress(InetAddress.getLoopbackAddress(), port))
                    serverSocket = ss
                    while (running) {
                        val socket = ss.accept() // blocks for the next (single) client
                        handleConnection(socket)
                    }
                }
            } catch (_: IOException) {
                if (running) {
                    try {
                        Thread.sleep(BACKOFF_MS)
                    } catch (_: InterruptedException) {
                        return
                    }
                }
            }
        }
    }

    private fun handleConnection(socket: Socket) {
        socket.tcpNoDelay = true
        val queue = LinkedBlockingQueue<ByteArray>()
        outQueue = queue
        var reader: Thread? = null
        try {
            val out = BufferedOutputStream(socket.getOutputStream())
            // HELLO first (docs/02 §3).
            hello?.let {
                out.write(PacketEncoder.encode(it))
                out.flush()
            }
            reader = Thread({ readLoop(socket, socket.getInputStream(), queue) }, "pad-reader")
                .apply { isDaemon = true; start() }

            while (running && !socket.isClosed) {
                val bytes = queue.poll(POLL_MS, TimeUnit.MILLISECONDS) ?: continue
                out.write(bytes)
                out.flush() // immediate, no Nagle batching (docs/01 §3)
            }
        } catch (_: IOException) {
            // Client disconnected or write failed — fall through to cleanup.
        } finally {
            outQueue = null
            reader?.interrupt()
            try {
                socket.close()
            } catch (_: IOException) {
            }
        }
    }

    // Reads PC->phone packets. Phase A only answers PING with PONG echo (docs/02 §5);
    // HAPTIC (Phase D) and anything else is ignored.
    private fun readLoop(socket: Socket, input: InputStream, queue: BlockingQueue<ByteArray>) {
        val decoder = StreamDecoder()
        val buf = ByteArray(READ_BUF)
        try {
            while (running) {
                val n = input.read(buf)
                if (n < 0) break
                val chunk = buf.copyOf(n)
                for (pkt in decoder.feed(chunk)) {
                    if (pkt is DecodedPacket.PingPacket) {
                        queue.offer(PacketEncoder.encodePong(pkt.value))
                    }
                }
            }
        } catch (_: IOException) {
            // Socket closed.
        } finally {
            try {
                socket.close() // unblock the writer promptly on EOF
            } catch (_: IOException) {
            }
        }
    }

    companion object {
        const val DEFAULT_PORT = 38917
        private const val POLL_MS = 1000L
        private const val BACKOFF_MS = 200L
        private const val READ_BUF = 4096
    }
}
