// Black, full-screen, landscape "pad" Activity (docs/01 §2.1).
// Phase A default UX is landscape pad mode: the phone lies flat on the desk and
// acts like a laptop touchpad. The screen is normally dark (brightness driven to 0)
// but touch stays alive. Raw MotionEvents are converted to TouchFrames and streamed
// to the PC; the phone never interprets gestures.
//
// Usability (v0.3.0): the panel has two states.
//  - Pad active (focused, system bars hidden): a brief dim "trackpad mode / how to
//    exit" hint on entry, then the panel fades to fully dark.
//  - System UI showing (window focus lost to the shade, or system bars visible): the
//    pad is otherwise so dark the user can't tell they've left the trackpad, so we just
//    raise brightness slightly (no hint text — the shade/bars would cover it) until focus
//    returns / the bars hide, then re-apply immersive and fade back to dark.
// Immersive mode is only (re-)applied when returning to the pad, never while system
// UI is up, so we don't fight the user's system gestures.
package com.phone2pad

import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.ActivityInfo
import android.graphics.Color
import android.hardware.display.DisplayManager
import android.os.Build
import android.os.Bundle
import android.util.DisplayMetrics
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.TextView
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.phone2pad.proto.Hello
import kotlin.math.roundToInt

class BlackPadActivity : android.app.Activity() {

    private val server = PadSocketServer()

    // Dim hint overlaid on the pad; non-interactive so it never eats touches.
    private lateinit var hintView: TextView
    // Pending "fade the hint then go dark" callback; cancelled on pause / re-show.
    private var dimRunnable: Runnable? = null

    // System-UI state machine. systemUiActive is the last-applied state; starting it true
    // makes the first focus-gain settle into the pad state and show the entry hint.
    private var hasWindowFocus = false
    private var systemBarsVisible = false
    private var systemUiActive = true

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

        // Keep the panel awake; start dark (the hint raises brightness briefly on focus).
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        setBrightness(0f)

        applyImmersive()

        val padView = PadView(this)
        padView.setBackgroundColor(Color.BLACK)

        hintView = TextView(this).apply {
            setTextColor(HINT_COLOR)
            textSize = 14f
            gravity = Gravity.CENTER
            // Inert overlay: a non-clickable, non-focusable view returns false from
            // onTouchEvent, so the FrameLayout dispatches the full multi-touch stream to
            // PadView underneath — the hint never interferes with touch reporting.
            isClickable = false
            isFocusable = false
            isFocusableInTouchMode = false
            visibility = View.GONE
        }

        val root = FrameLayout(this).apply {
            setBackgroundColor(Color.BLACK)
            addView(
                padView,
                FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT,
                ),
            )
            addView(
                hintView,
                FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    Gravity.CENTER,
                ),
            )
        }
        setContentView(root)

        // System-bar visibility via insets is only reliable on API 30+ (on older versions
        // isVisible() returns true unconditionally, which would strand the pad lit). Below R
        // we rely on window focus (the shade always steals focus) — see refreshSystemUiState.
        ViewCompat.setOnApplyWindowInsetsListener(window.decorView) { _, insets ->
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                val visible = insets.isVisible(WindowInsetsCompat.Type.systemBars())
                if (visible != systemBarsVisible) {
                    systemBarsVisible = visible
                    refreshSystemUiState()
                }
            }
            insets
        }
    }

    override fun onResume() {
        super.onResume()
        @Suppress("DEPRECATION")
        lastRotation = windowManager.defaultDisplay.rotation
        server.start(buildHello())
        val dm = getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
        dm.registerDisplayListener(displayListener, null)
        applyImmersive()
    }

    override fun onPause() {
        super.onPause()
        // Stop any in-flight hint timing/animation, then hand brightness control back to
        // the system. The dim steady-state override only applies while the pad is active;
        // leaving it set could darken whatever shows next, so restore it last (after the
        // cancel, whose end-action may have driven brightness to 0f). Reset the state so a
        // later re-entry shows the entry hint again.
        cancelHintTiming()
        setBrightness(WindowManager.LayoutParams.BRIGHTNESS_OVERRIDE_NONE)
        hintView.visibility = View.GONE
        hasWindowFocus = false
        systemBarsVisible = false
        systemUiActive = true
        val dm = getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
        dm.unregisterDisplayListener(displayListener)
        server.stop()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        hasWindowFocus = hasFocus
        refreshSystemUiState()
    }

    /** Drive brightness + hint from whether system UI is in front. "System UI active" =
     *  window focus lost (e.g. the shade is open) OR the system bars are visible. Acts only
     *  on transitions so repeated callbacks don't restart the hint. */
    private fun refreshSystemUiState() {
        val active = !hasWindowFocus || systemBarsVisible
        if (active == systemUiActive) return
        systemUiActive = active
        if (active) {
            raiseForSystemUi()
        } else {
            applyImmersive()  // only re-hide bars when returning to the pad, not while in use
            showExitHintThenDim()
        }
    }

    /** Immersive, system-gesture-safe full screen: hide the bars; they return transiently
     *  on an edge swipe (BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE). The notification shade is
     *  not blocked. minSdk 28 compatible.
     *  https://developer.android.com/develop/ui/views/layout/edge-to-edge */
    private fun applyImmersive() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    /** System UI is up: just raise brightness (no hint text — the shade/status bar would
     *  cover it anyway) so the user can tell something changed and they're in Android's UI,
     *  not the otherwise near-black trackpad. Brightness stays raised until focus/bars
     *  return. */
    private fun raiseForSystemUi() {
        cancelHintTiming()
        hintView.visibility = View.GONE
        setBrightness(SYSTEM_UI_BRIGHTNESS)
    }

    /** Back on the pad: briefly light the panel with the exit hint, then fade out and return
     *  to a fully dark steady state. Burn-in-safe: dim, transient, centered. */
    private fun showExitHintThenDim() {
        cancelHintTiming()
        setBrightness(HINT_BRIGHTNESS)
        hintView.text = EXIT_HINT
        hintView.alpha = 1f
        hintView.visibility = View.VISIBLE
        val r = Runnable {
            hintView.animate().alpha(0f).setDuration(HINT_FADE_MS).withEndAction {
                hintView.visibility = View.GONE
                setBrightness(0f)  // steady-state fully dark (power + OLED burn-in design)
            }
        }
        dimRunnable = r
        hintView.postDelayed(r, HINT_VISIBLE_MS)
    }

    private fun cancelHintTiming() {
        dimRunnable?.let { hintView.removeCallbacks(it) }
        dimRunnable = null
        hintView.animate().cancel()
    }

    private fun setBrightness(value: Float) {
        window.attributes = window.attributes.apply { screenBrightness = value }
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

        // Hint brightness levels (0..1). Both dim; the pad fades back to 0 when settled.
        const val HINT_BRIGHTNESS = 0.12f       // entry/exit hint flash
        const val SYSTEM_UI_BRIGHTNESS = 0.11f  // held while the system bars/shade are up
        const val HINT_VISIBLE_MS = 3500L
        const val HINT_FADE_MS = 600L
        val HINT_COLOR = Color.rgb(0x66, 0x66, 0x66)  // dim grey, low contrast on black

        // Korean to match the app's user-facing copy (MainActivity).
        const val EXIT_HINT = "트랙패드 모드\n뒤로 버튼 또는 뒤로 제스처로 종료"
    }
}
