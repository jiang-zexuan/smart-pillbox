package com.pillbox.app.ui

import android.app.AlertDialog
import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.InputType
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.cardview.widget.CardView
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.gridlayout.widget.GridLayout
import com.pillbox.app.R
import com.pillbox.app.data.LocalStorage
import com.pillbox.app.model.MedicationRecord
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale

class HomeFragment : Fragment() {

    private lateinit var storage: LocalStorage
    private lateinit var bleBar: LinearLayout
    private lateinit var bleDot: View
    private lateinit var bleText: TextView
    private lateinit var tvDate: TextView
    private lateinit var tvTime: TextView
    private lateinit var slotGrid: GridLayout
    private lateinit var todayRecords: LinearLayout
    private lateinit var timeHandler: Handler
    private var onSlotChanged: ((Int, String?) -> Unit)? = null
    private var onSlotSelected: ((Int) -> Unit)? = null

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_home, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        storage = LocalStorage(requireContext())
        bleBar = view.findViewById(R.id.bleBar)
        bleDot = view.findViewById(R.id.bleDot)
        bleText = view.findViewById(R.id.bleText)
        tvDate = view.findViewById(R.id.tvDate)
        tvTime = view.findViewById(R.id.tvTime)
        slotGrid = view.findViewById(R.id.slotGrid)
        todayRecords = view.findViewById(R.id.todayRecords)

        updateTime()
        buildSlotGrid()
        buildTodayRecords()

        timeHandler = Handler(Looper.getMainLooper())
        timeHandler.postDelayed(object : Runnable {
            override fun run() {
                updateTime()
                timeHandler.postDelayed(this, 1000)
            }
        }, 1000)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        timeHandler.removeCallbacksAndMessages(null)
    }

    fun setOnSlotChanged(callback: (Int, String?) -> Unit) {
        onSlotChanged = callback
    }

    fun setOnSlotSelected(callback: (Int) -> Unit) {
        onSlotSelected = callback
    }

    fun updateBleState(connected: Boolean) {
        if (connected) {
            bleBar.background = ContextCompat.getDrawable(requireContext(), R.drawable.bg_ble_bar_green)
            bleDot.visibility = View.VISIBLE
            bleText.text = getString(R.string.ble_connected)
        } else {
            bleBar.background = ContextCompat.getDrawable(requireContext(), R.drawable.bg_ble_bar)
            bleDot.visibility = View.GONE
            bleText.text = getString(R.string.ble_disconnected)
        }
    }

    fun refreshSlots() {
        if (::slotGrid.isInitialized) buildSlotGrid()
    }

    private fun updateTime() {
        val now = Calendar.getInstance()
        tvDate.text = SimpleDateFormat("yyyy年M月d日 EEEE", Locale.CHINESE).format(now.time)
        tvTime.text = SimpleDateFormat("HH:mm", Locale.getDefault()).format(now.time)
    }

    private fun buildSlotGrid() {
        slotGrid.removeAllViews()
        val slots = storage.getDemoSlots()
        val plans = storage.loadPlans()
        for (i in 0..5) {
            val card = createSlotCard(i, slots[i], plans)
            val params = GridLayout.LayoutParams().apply {
                width = 0
                height = GridLayout.LayoutParams.WRAP_CONTENT
                columnSpec = GridLayout.spec(i % 3, 1f)
                rowSpec = GridLayout.spec(i / 3)
                setMargins(5, 5, 5, 5)
            }
            slotGrid.addView(card, params)
        }
    }

    private fun createSlotCard(
        slotId: Int,
        medName: String?,
        plans: List<com.pillbox.app.model.MedicationPlan>
    ): CardView {
        val card = LayoutInflater.from(requireContext()).inflate(R.layout.item_slot, slotGrid, false) as CardView
        val slotNumber: TextView = card.findViewById(R.id.slotNumber)
        val slotIcon: TextView = card.findViewById(R.id.slotIcon)
        val slotName: TextView = card.findViewById(R.id.slotName)
        val slotTime: TextView = card.findViewById(R.id.slotTime)

        slotNumber.text = "${slotId + 1}号药仓"
        val plan = plans.find { it.slotId == slotId && it.enabled }

        if (!medName.isNullOrBlank()) {
            slotIcon.text = "■"
            slotName.text = medName
            slotName.setTextColor(Color.parseColor("#333333"))
            card.setCardBackgroundColor(Color.parseColor("#F0FAF4"))
            plan?.let {
                val periodName = when (it.period) {
                    0 -> "早"
                    1 -> "中"
                    2 -> "晚"
                    else -> ""
                }
                slotTime.text = "$periodName ${String.format("%02d:%02d", it.hour, it.minute)}"
                slotTime.visibility = View.VISIBLE
            }
        } else {
            slotIcon.text = "□"
            slotName.text = "空仓"
            slotName.setTextColor(Color.parseColor("#BBBBBB"))
            slotTime.visibility = View.GONE
        }

        card.setOnClickListener {
            onSlotSelected?.invoke(slotId)
            showEditSlotDialog(slotId, medName)
        }
        return card
    }

    private fun showEditSlotDialog(slotId: Int, currentName: String?) {
        val input = EditText(requireContext()).apply {
            inputType = InputType.TYPE_CLASS_TEXT
            hint = "输入药品名称"
            setText(currentName.orEmpty())
            selectAll()
            setPadding(32, 24, 32, 24)
        }

        AlertDialog.Builder(requireContext())
            .setTitle("编辑 ${slotId + 1}号药仓")
            .setMessage("默认药仓为空。输入药品名可手动放入；点清空可设为空仓。")
            .setView(input)
            .setPositiveButton("保存") { _, _ ->
                val name = input.text.toString().trim()
                if (name.isBlank()) {
                    Toast.makeText(requireContext(), "药品名不能为空；如需空仓请点清空", Toast.LENGTH_SHORT).show()
                    return@setPositiveButton
                }
                storage.saveSlotMedicine(slotId, name)
                refreshSlots()
                onSlotChanged?.invoke(slotId, name)
                Toast.makeText(requireContext(), "${slotId + 1}号药仓：$name", Toast.LENGTH_SHORT).show()
            }
            .setNeutralButton("清空") { _, _ ->
                storage.clearSlotMedicine(slotId)
                refreshSlots()
                onSlotChanged?.invoke(slotId, null)
                Toast.makeText(requireContext(), "${slotId + 1}号药仓已清空", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("取消", null)
            .show()
    }

    private fun buildTodayRecords() {
        todayRecords.removeAllViews()
        val records = storage.getDemoRecords()
        if (records.isEmpty()) {
            todayRecords.addView(TextView(requireContext()).apply {
                text = "暂无记录"
                textSize = 13f
                setTextColor(Color.parseColor("#BBBBBB"))
                gravity = Gravity.CENTER
                setPadding(8, 8, 8, 8)
            })
            return
        }
        records.forEach { todayRecords.addView(createRecordRow(it)) }
    }

    private fun createRecordRow(record: MedicationRecord): View {
        val row = LinearLayout(requireContext()).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, 12, 0, 12)
            gravity = Gravity.CENTER_VERTICAL
        }
        val info = TextView(requireContext()).apply {
            val fmt = SimpleDateFormat("HH:mm", Locale.getDefault())
            text = "${fmt.format(Date(record.timestamp))}  ${record.medicineName}  ${record.slotId + 1}号药仓"
            textSize = 14f
            setTextColor(Color.parseColor("#333333"))
        }
        row.addView(info)
        return row
    }
}
