package com.pillbox.app.ui

import android.app.TimePickerDialog
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.LinearLayout
import android.widget.Spinner
import android.widget.TextView
import androidx.cardview.widget.CardView
import androidx.fragment.app.Fragment
import com.google.android.material.switchmaterial.SwitchMaterial
import com.pillbox.app.R
import com.pillbox.app.data.LocalStorage
import com.pillbox.app.model.MedicationPlan

class PlanFragment : Fragment() {

    private lateinit var storage: LocalStorage
    private lateinit var periodsContainer: LinearLayout
    private lateinit var btnAddPlan: Button
    private lateinit var btnSync: Button
    private var onSyncClicked: (() -> Unit)? = null

    private val periods = listOf(
        PeriodInfo(0, "早上"),
        PeriodInfo(1, "中午"),
        PeriodInfo(2, "晚上")
    )

    data class PeriodInfo(val id: Int, val name: String)

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_plan, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        storage = LocalStorage(requireContext())
        periodsContainer = view.findViewById(R.id.periodsContainer)
        btnAddPlan = view.findViewById(R.id.btnAddPlan)
        btnSync = view.findViewById(R.id.btnSync)
        buildPeriodSections()
        btnSync.setOnClickListener { onSyncClicked?.invoke() }
        btnAddPlan.setOnClickListener { addExtraPlan() }
    }

    fun setOnSyncClicked(callback: () -> Unit) {
        onSyncClicked = callback
    }

    fun getPlans(): List<MedicationPlan> {
        val plans = mutableListOf<MedicationPlan>()
        extractPlansFromView(periodsContainer, plans)
        return plans.take(6).mapIndexed { index, plan ->
            plan.copy(period = index)
        }
    }

    private fun extractPlansFromView(view: ViewGroup, plans: MutableList<MedicationPlan>) {
        for (i in 0 until view.childCount) {
            val child = view.getChildAt(i)
            if (child is LinearLayout && child.tag == "plan_row") {
                val timeBtn = child.getChildAt(0) as? Button ?: continue
                val spinner = child.getChildAt(1) as? Spinner ?: continue
                val switch = child.getChildAt(2) as? SwitchMaterial ?: continue
                val parts = timeBtn.text.toString().split(":")
                val hour = parts.getOrNull(0)?.toIntOrNull() ?: continue
                val minute = parts.getOrNull(1)?.toIntOrNull() ?: continue
                val periodTag = child.getTag(R.id.tag_period) as? Int ?: continue
                plans.add(MedicationPlan(spinner.selectedItemPosition, hour, minute, periodTag, switch.isChecked))
            } else if (child is ViewGroup) {
                extractPlansFromView(child, plans)
            }
        }
    }

    private fun buildPeriodSections() {
        periodsContainer.removeAllViews()
        val plans = storage.loadPlans()
        val slotOptions = buildSlotOptions(storage.getDemoSlots())
        periods.forEach { period ->
            periodsContainer.addView(createPeriodRow(period, plans.find { it.period == period.id }, slotOptions))
        }
    }

    private fun buildSlotOptions(slots: Map<Int, String>): List<String> {
        return (0..5).map { i -> "${i + 1}号药仓 - ${slots[i] ?: "空"}" }
    }

    private fun createPeriodRow(period: PeriodInfo, existingPlan: MedicationPlan?, slotOptions: List<String>): CardView {
        val card = CardView(requireContext()).apply {
            radius = 14f
            cardElevation = 1f
            setContentPadding(16, 16, 16, 16)
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply { bottomMargin = 12 }
        }
        val container = LinearLayout(requireContext()).apply { orientation = LinearLayout.VERTICAL }
        container.addView(TextView(requireContext()).apply {
            text = period.name
            textSize = 17f
            setTextColor(Color.parseColor("#333333"))
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            setPadding(0, 0, 0, 10)
        })
        val row = LinearLayout(requireContext()).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            tag = "plan_row"
            setTag(R.id.tag_period, period.id)
        }
        val hour = existingPlan?.hour ?: when (period.id) { 0 -> 8; 1 -> 12; 2 -> 19; else -> 8 }
        val minute = existingPlan?.minute ?: 0
        val timeBtn = Button(requireContext()).apply {
            text = String.format("%02d:%02d", hour, minute)
            minWidth = 120
            setOnClickListener { showTimePicker(this) }
        }
        val spinner = Spinner(requireContext()).apply {
            adapter = ArrayAdapter(requireContext(), android.R.layout.simple_spinner_dropdown_item, slotOptions)
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f).apply { marginEnd = 10 }
            setSelection((existingPlan?.slotId ?: 0).coerceIn(0, 5))
        }
        val switch = SwitchMaterial(requireContext()).apply { isChecked = existingPlan?.enabled ?: true }
        row.addView(timeBtn)
        row.addView(spinner)
        row.addView(switch)
        container.addView(row)
        card.addView(container)
        return card
    }

    private fun showTimePicker(btn: Button) {
        val parts = btn.text.toString().split(":")
        val h = parts.getOrNull(0)?.toIntOrNull() ?: 8
        val m = parts.getOrNull(1)?.toIntOrNull() ?: 0
        TimePickerDialog(requireContext(), { _, hour, minute ->
            btn.text = String.format("%02d:%02d", hour, minute)
        }, h, m, true).show()
    }

    private fun addExtraPlan() {
        periodsContainer.addView(createPeriodRow(PeriodInfo(-1, "自定义提醒"), null, buildSlotOptions(storage.getDemoSlots())))
    }
}
