package com.pillbox.app.weight

enum class WeightChangeEvent {
    Placed,
    Taken
}

/**
 * Detects clear pillbox weight changes from HX711 readings.
 *
 * Input unit is centigram (cg), same as STM32 STAT weightCg.
 * 500cg = 5.00g.
 */
class WeightChangeDetector(
    private val thresholdCg: Long = DEFAULT_THRESHOLD_CG,
    private val compareWindowMs: Long = DEFAULT_COMPARE_WINDOW_MS
) {
    private data class Reading(val timeMs: Long, val weightCg: Long)

    private enum class State {
        Empty,
        HasMedicine
    }

    private val history = ArrayDeque<Reading>()
    private var state: State = State.Empty

    fun onWeightCg(weightCg: Long, nowMs: Long = System.currentTimeMillis()): WeightChangeEvent? {
        history.addLast(Reading(nowMs, weightCg))
        while (history.size > 1 && nowMs - history[1].timeMs >= compareWindowMs) {
            history.removeFirst()
        }

        val tenSecondsAgo = history.firstOrNull()
        if (tenSecondsAgo == null || nowMs - tenSecondsAgo.timeMs < compareWindowMs) {
            return null
        }

        return when (state) {
            State.Empty -> {
                if (weightCg - tenSecondsAgo.weightCg >= thresholdCg) {
                    state = State.HasMedicine
                    resetHistoryTo(nowMs, weightCg)
                    WeightChangeEvent.Placed
                } else {
                    null
                }
            }
            State.HasMedicine -> {
                if (tenSecondsAgo.weightCg - weightCg >= thresholdCg) {
                    state = State.Empty
                    resetHistoryTo(nowMs, weightCg)
                    WeightChangeEvent.Taken
                } else {
                    null
                }
            }
        }
    }

    fun reset() {
        history.clear()
        state = State.Empty
    }

    private fun resetHistoryTo(nowMs: Long, weightCg: Long) {
        history.clear()
        history.addLast(Reading(nowMs, weightCg))
    }

    companion object {
        const val DEFAULT_THRESHOLD_CG: Long = 500L
        const val DEFAULT_COMPARE_WINDOW_MS: Long = 10_000L
    }
}
