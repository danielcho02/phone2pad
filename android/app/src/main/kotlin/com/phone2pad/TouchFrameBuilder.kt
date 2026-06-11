// Converts an Android MotionEvent into a wire TouchFrame (docs/02-PROTOCOL.md §4).
// The phone is a dumb sensor: no gesture detection, scaling, or smoothing here —
// coordinates are taken from the landscape Activity surface as-is.
package com.phone2pad

import android.view.MotionEvent
import com.phone2pad.proto.Contact
import com.phone2pad.proto.TouchFrame

object TouchFrameBuilder {
    // u16 coordinate range on the wire.
    private const val MAX_COORD = 0xFFFF

    /**
     * Build a frame for DOWN / MOVE / POINTER_UP / UP actions.
     *
     * Lift rule (docs/02 §4): on ACTION_UP / ACTION_POINTER_UP the lifting pointer
     * is still present in the event at [MotionEvent.getActionIndex]; we report it
     * once with tip=0. It is naturally excluded from subsequent events.
     */
    fun fromMotionEvent(event: MotionEvent): TouchFrame {
        val action = event.actionMasked
        val upIndex = if (action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_UP) {
            event.actionIndex
        } else {
            -1
        }
        val contacts = ArrayList<Contact>(event.pointerCount)
        for (i in 0 until event.pointerCount) {
            contacts.add(
                Contact(
                    id = event.getPointerId(i),
                    tip = i != upIndex,
                    confidence = true, // palm rejection is Phase D
                    x = clamp(event.getX(i)),
                    y = clamp(event.getY(i)),
                )
            )
        }
        return TouchFrame(timestampUs(event), contacts)
    }

    /** ACTION_CANCEL (docs/02 §4): all current contacts reported once as tip=0. */
    fun cancelFrame(event: MotionEvent): TouchFrame {
        val contacts = ArrayList<Contact>(event.pointerCount)
        for (i in 0 until event.pointerCount) {
            contacts.add(
                Contact(
                    id = event.getPointerId(i),
                    tip = false,
                    confidence = true,
                    x = clamp(event.getX(i)),
                    y = clamp(event.getY(i)),
                )
            )
        }
        return TouchFrame(timestampUs(event), contacts)
    }

    // event.eventTime is a monotonic uptime in ms; u32 wrap is allowed by the proto.
    private fun timestampUs(event: MotionEvent): Long = event.eventTime * 1000L

    private fun clamp(v: Float): Int = v.toInt().coerceIn(0, MAX_COORD)
}
