// Launcher screen. Phone2pad's default entry point when the user taps the app
// icon: a minimal portrait screen with the USB setup steps and a "start pad"
// button that opens BlackPadActivity (the landscape black touchpad surface).
//
// BlackPadActivity stays the pad-mode-only Activity and remains exported so the PC
// client can still launch it directly via:
//   adb shell am start -n com.phone2pad/.BlackPadActivity
// UI is built in code (no XML layout, no Material dependency) to match the
// existing code-based view style.
package com.phone2pad

import android.content.Intent
import android.content.pm.ActivityInfo
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

class MainActivity : android.app.Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_PORTRAIT

        val pad = dp(24)
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(pad, pad, pad, pad)
        }

        val title = TextView(this).apply {
            text = "phone2pad"
            textSize = 28f
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            gravity = Gravity.CENTER
        }

        val steps = TextView(this).apply {
            text = listOf(
                "USB 트랙패드로 사용하려면:",
                "",
                "1. PC에서 phone2pad_client.exe 실행",
                "2. USB 케이블로 PC와 연결, USB 디버깅 허용",
                "3. 이 화면에서 [트랙패드 모드 시작] 누르기",
                "",
                "검정 화면에서만 터치 입력이 PC로 전송됩니다.",
            ).joinToString("\n")
            textSize = 16f
            gravity = Gravity.CENTER
            setPadding(0, dp(24), 0, dp(32))
        }

        val startButton = Button(this).apply {
            text = "트랙패드 모드 시작"
            setOnClickListener {
                startActivity(Intent(this@MainActivity, BlackPadActivity::class.java))
            }
        }

        root.addView(title, wrapContent())
        root.addView(steps, wrapContent())
        root.addView(startButton, wrapContent())
        root.setBackgroundColor(Color.WHITE)
        setContentView(root)
    }

    private fun wrapContent() = LinearLayout.LayoutParams(
        ViewGroup.LayoutParams.WRAP_CONTENT,
        ViewGroup.LayoutParams.WRAP_CONTENT,
    )

    private fun dp(value: Int): Int =
        (value * resources.displayMetrics.density).toInt()
}
