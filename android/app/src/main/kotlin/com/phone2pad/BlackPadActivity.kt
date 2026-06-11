// Black, full-screen, landscape "pad" Activity (docs/01 §2.1).
// Phase A default UX is landscape pad mode: the phone lies flat on the desk and
// acts like a laptop touchpad. The screen shows nothing (brightness 0) but touch
// stays alive. Raw MotionEvents are converted to TouchFrames and streamed to the
// PC; the phone never interprets gestures.
package com.phone2pad

import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.ActivityInfo
import android.graphics.Color
import android.hardware.display.DisplayManager
import android.os.Bundle
import android.util.DisplayMetrics
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.phone2pad.proto.Hello
import kotlin.math.roundToInt

class BlackPadActivity : android.app.Activity() {

    private val server = PadSocketServer()

    // Last display rotation we reported to the PC; used to re-send HELLO only on change.
    private var lastRotation = -1

    // Fires on any display change. A landscape<->reverse-landscape (90<->270) flip keeps
    // the same Configuration, so onConfigurationChanged is unreliable for it; the display
    // listener is. On a rotation change we push a fresh HELLO so the PC re-normalizes
    // coordinates to the canonical frame (docs/02 §3).
    private val displayListener = object : DisplayManager.DisplayListener {
        override fun onDisplayAdded(displayId: Int) {}
        override fun onDisplayRemoved(displayId: Int) {}
        override fun onDisplayChanged(displayId: Int) {
            @Suppress("DEPRECATION")
            val rotation = windowManager.defaultDisplay.rotation
            if (rotation != lastRotation) {
                lastRotation = rotation
                server.sendHello(buildHello())
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Allow both landscapes so a 180° flip is reported (sensorLandscape in the manifest
        // too). Coordinate normalization to the canonical frame is done PC-side from the
        // rotation reported in HELLO.
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE

        // Keep the panel awake; backlight driven to 0 so no pixels are shown.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.attributes = window.attributes.apply { screenBrightness = 0f }

        // Immersive: hide the system bars, let them transiently return on swipe.
        // androidx WindowInsetsController works back to minSdk 28 (the raw
        // android.view.WindowInsetsController is API 30+).
        // https://developer.android.com/develop/ui/views/layout/edge-to-edge
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }

        val padView = PadView(this)
        padView.setBackgroundColor(Color.BLACK)
        setContentView(padView)
    }

    override fun onResume() {
        super.onResume()
        @Suppress("DEPRECATION")
        lastRotation = windowManager.defaultDisplay.rotation
        server.start(buildHello())
        val dm = getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
        dm.registerDisplayListener(displayListener, null)
    }

    override fun onPause() {
        super.onPause()
        val dm = getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
        dm.unregisterDisplayListener(displayListener)
        server.stop()
    }

    private fun buildHello(): Hello {
        val metrics = DisplayMetrics()
        @Suppress("DEPRECATION")
        windowManager.defaultDisplay.getRealMetrics(metrics)
        @Suppress("DEPRECATION")
        val rotation = windowManager.defaultDisplay.rotation
        // In locked landscape, widthPixels is the long edge, heightPixels the short.
        return Hello(
            protocolVersion = 1,
            screenWidthPx = metrics.widthPixels.coerceIn(0, 0xFFFF),
            screenHeightPx = metrics.heightPixels.coerceIn(0, 0xFFFF),
            xdpiX10 = (metrics.xdpi * 10f).roundToInt().coerceIn(0, 0xFFFF),
            ydpiX10 = (metrics.ydpi * 10f).roundToInt().coerceIn(0, 0xFFFF),
            rotation = rotation,
            maxContacts = MAX_CONTACTS,
        )
    }

    /** Full-screen touch surface. Consumes all touch so we see the full stream. */
    private inner class PadView(context: android.content.Context) : View(context) {
        @SuppressLint("ClickableViewAccessibility")
        override fun onTouchEvent(event: MotionEvent): Boolean {
            val frame = if (event.actionMasked == MotionEvent.ACTION_CANCEL) {
                TouchFrameBuilder.cancelFrame(event)
            } else {
                TouchFrameBuilder.fromMotionEvent(event)
            }
            server.sendFrame(frame)
            return true
        }
    }

    private companion object {
        const val MAX_CONTACTS = 10
    }
}
