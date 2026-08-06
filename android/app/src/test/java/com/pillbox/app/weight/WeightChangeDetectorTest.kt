package com.pillbox.app.weight

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class WeightChangeDetectorTest {

    @Test
    fun firstReadingOnlySetsBaseline() {
        val detector = WeightChangeDetector()

        assertNull(detector.onWeightCg(120, nowMs = 0))
        assertNull(detector.onWeightCg(130, nowMs = 10_000))
    }

    @Test
    fun increaseAtLeastFiveGramsComparedWithTenSecondsAgoReportsMedicinePlacedOnce() {
        val detector = WeightChangeDetector()

        detector.onWeightCg(0, nowMs = 0)

        assertNull(detector.onWeightCg(700, nowMs = 9_999))
        assertEquals(WeightChangeEvent.Placed, detector.onWeightCg(700, nowMs = 10_000))
        assertNull(detector.onWeightCg(720, nowMs = 20_000))
    }

    @Test
    fun decreaseAtLeastFiveGramsComparedWithTenSecondsAgoAfterPlacedReportsMedicineTaken() {
        val detector = WeightChangeDetector()

        detector.onWeightCg(0, nowMs = 0)
        detector.onWeightCg(650, nowMs = 10_000)

        assertNull(detector.onWeightCg(0, nowMs = 19_999))
        assertEquals(WeightChangeEvent.Taken, detector.onWeightCg(0, nowMs = 20_000))
        assertNull(detector.onWeightCg(20, nowMs = 30_000))
    }

    @Test
    fun resetClearsTenSecondHistory() {
        val detector = WeightChangeDetector()

        detector.onWeightCg(300, nowMs = 0)
        detector.reset()

        assertNull(detector.onWeightCg(900, nowMs = 10_000))
        assertEquals(WeightChangeEvent.Placed, detector.onWeightCg(1400, nowMs = 20_000))
    }
}
