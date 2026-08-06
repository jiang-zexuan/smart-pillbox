package com.pillbox.app.ui

import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.pillbox.app.R
import com.pillbox.app.data.LocalStorage
import com.pillbox.app.model.MedicationRecord
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale

class HistoryFragment : Fragment() {

    private lateinit var storage: LocalStorage
    private lateinit var tvHistoryDate: TextView
    private lateinit var btnPrevDay: TextView
    private lateinit var btnNextDay: TextView
    private lateinit var statTaken: TextView
    private lateinit var statMissed: TextView
    private lateinit var statPending: TextView
    private lateinit var historyRecords: LinearLayout
    private var currentDate = Calendar.getInstance()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_history, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        storage = LocalStorage(requireContext())
        tvHistoryDate = view.findViewById(R.id.tvHistoryDate)
        btnPrevDay = view.findViewById(R.id.btnPrevDay)
        btnNextDay = view.findViewById(R.id.btnNextDay)
        statTaken = view.findViewById(R.id.statTaken)
        statMissed = view.findViewById(R.id.statMissed)
        statPending = view.findViewById(R.id.statPending)
        historyRecords = view.findViewById(R.id.historyRecords)
        btnPrevDay.setOnClickListener { changeDay(-1) }
        btnNextDay.setOnClickListener { changeDay(1) }
        refresh()
    }

    private fun changeDay(delta: Int) {
        currentDate.add(Calendar.DAY_OF_YEAR, delta)
        refresh()
    }

    private fun refresh() {
        tvHistoryDate.text = SimpleDateFormat("yyyy年M月d日 EEEE", Locale.CHINESE).format(currentDate.time)
        val allRecords = storage.loadRecords()
        val todayRecords = allRecords.filter {
            val recCal = Calendar.getInstance().apply { timeInMillis = it.timestamp }
            recCal.get(Calendar.YEAR) == currentDate.get(Calendar.YEAR) &&
                recCal.get(Calendar.DAY_OF_YEAR) == currentDate.get(Calendar.DAY_OF_YEAR)
        }
        statTaken.text = todayRecords.count { it.taken }.toString()
        statMissed.text = todayRecords.count { !it.taken }.toString()
        statPending.text = "0"
        buildRecordList(todayRecords)
    }

    private fun buildRecordList(records: List<MedicationRecord>) {
        historyRecords.removeAllViews()
        if (records.isEmpty()) {
            historyRecords.addView(TextView(requireContext()).apply {
                text = "当天无服药记录"
                textSize = 13f
                setTextColor(Color.parseColor("#BBBBBB"))
                gravity = Gravity.CENTER
                setPadding(8, 24, 8, 24)
            })
            return
        }
        records.forEach { historyRecords.addView(createRecordRow(it)) }
    }

    private fun createRecordRow(record: MedicationRecord): View {
        val row = LinearLayout(requireContext()).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, 12, 0, 12)
        }
        val timeFmt = SimpleDateFormat("HH:mm", Locale.getDefault())
        row.addView(TextView(requireContext()).apply {
            text = "${record.medicineName} · ${record.slotId + 1}号药仓"
            textSize = 15f
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            setTextColor(Color.parseColor("#333333"))
        })
        row.addView(TextView(requireContext()).apply {
            text = "${timeFmt.format(Date(record.timestamp))} · ${if (record.taken) "已服用" else "未服用"}"
            textSize = 12f
            setTextColor(Color.parseColor("#888888"))
        })
        return row
    }
}
