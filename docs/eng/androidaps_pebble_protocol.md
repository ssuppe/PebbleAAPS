# Pebble Data Protocol Expansion (AndroidAPS Companion Plugin)

## Overview
This design document outlines the necessary changes on the **AndroidAPS mobile application side** (specifically inside the `plugins:pebble` module) to expand the Pebble AppMessage dictionary payload. By sending these additional fields, the companion app will enable the Pebble watchface to display active treatments, insulin metrics, rate of change delta, and a historical glucose trend graph.

---

## 1. Key Definitions (`PebbleKeys.kt`)
Update the `PebbleKeys` object to include the new keys for pump/loop status metrics, delta values, graph boundaries, and the historical trend array:

```kotlin
package app.aaps.plugins.pebble

object PebbleKeys {
    const val BG = 0               // int32
    const val TREND = 1            // int32
    const val IOB = 2              // String (e.g. "0.32 U")
    const val COB = 3              // String (e.g. "0g")
    const val TIME = 4             // int32 (timestamp in seconds)
    const val BASAL = 5            // String (e.g. "0.90")
    const val IOB_DETAIL = 6       // String (e.g. "(0.02|0.31)")
    const val DELTA = 7            // String (e.g. "+3" or "+0.2")
    const val AVG_DELTA = 8        // String (e.g. "+5" or "+0.3")
    const val GLUCOSE_HISTORY = 9  // Byte Array (36 bytes representing BG/2)
    const val LOW_TARGET = 10      // int32 (low glucose target line, e.g. 70)
    const val HIGH_TARGET = 11     // int32 (high glucose target line, e.g. 180)
}
```

---

## 2. Expanded Data Structure (`EnrichedData.kt`)
Update `EnrichedData` to wrap the new metrics:

```kotlin
package app.aaps.plugins.pebble.data

data class EnrichedData(
    val bg: Double?,
    val trend: Int?,
    val time: Long,
    val iob: String?,
    val cob: String?,
    val basal: String?,
    val iobDetail: String?,
    val delta: String?,
    val avgDelta: String?,
    val history: ByteArray?,
    val lowTarget: Int?,
    val highTarget: Int?
)
```

---

## 3. Data Extraction and Formatting (`PebblePlugin.kt`)
In `PebblePlugin.sendData()`, retrieve and format the necessary AAPS values.

### A. Extract Status and Treatments
Query the `iobCobCalculator`, `glucoseStatusProvider`, and `profileFunction` to extract the metrics similarly to how Wear OS handles it:

```kotlin
val profile = profileFunction.getProfile()
var iobSum = ""
var iobDetail = ""
var cobString = ""
var currentBasal = ""
var delta = ""
var avgDelta = ""
var lowLine = 70
var highLine = 180

if (config.appInitialized && profile != null) {
    // 1. IOB (Bolus + Basal)
    val bolusIob = iobCobCalculator.calculateIobFromBolus().round()
    val basalIob = iobCobCalculator.calculateIobFromTempBasalsIncludingConvertedExtended().round()
    iobSum = decimalFormatter.to2Decimal(bolusIob.iob + basalIob.basaliob) + " U"
    iobDetail = "(${decimalFormatter.to2Decimal(bolusIob.iob)}|${decimalFormatter.to2Decimal(basalIob.basaliob)})"

    // 2. COB
    cobString = iobCobCalculator.getCobInfo("WatcherUpdaterService").generateCOBString(decimalFormatter)

    // 3. Basal Rate
    currentBasal = processedTbrEbData.getTempBasalIncludingConvertedExtended(System.currentTimeMillis())?.toStringShort(rh) 
        ?: decimalFormatter.to2Decimal(profile.getBasal())

    // 4. Targets
    val units = profileFunction.getUnits()
    lowLine = profileUtil.convertToMgdl(preferences.get(UnitDoubleKey.OverviewLowMark), units).toInt()
    highLine = profileUtil.convertToMgdl(preferences.get(UnitDoubleKey.OverviewHighMark), units).toInt()

    // 5. Delta & Avg Delta
    val glucoseStatus = glucoseStatusProvider.getGlucoseStatusData(true)
    if (glucoseStatus != null) {
        // Delta string uses mg/dL or mmol/L representation based on preferences
        delta = deltaString(glucoseStatus.delta, glucoseStatus.delta * Constants.MGDL_TO_MMOLL, units)
        avgDelta = deltaString(glucoseStatus.shortAvgDelta, glucoseStatus.shortAvgDelta * Constants.MGDL_TO_MMOLL, units)
    }
}
```

### B. Serialize 36-Point Glucose History
Retrieve recent historical glucose readings from the `AutosensDataStore` and pack them into a 36-byte array. Scale each BG value by dividing by 2 to fit in a single byte (`uint8_t` in C):

```kotlin
val readings = iobCobCalculator.ads.getBgReadingsDataTableCopy()
val sortedReadings = readings.filter { it.isValid }.sortedByDescending { it.timestamp }

val historySize = minOf(sortedReadings.size, 36)
val historyBytes = ByteArray(36) { 0.toByte() } // default to 0 representing empty slots

for (i in 0 until historySize) {
    val reading = sortedReadings[i]
    val targetIndex = 36 - 1 - i // latest reading at index 35 (right edge)
    val scaledValue = (reading.value / 2).toInt().coerceIn(0, 255)
    historyBytes[targetIndex] = scaledValue.toByte()
}
```

---

## 4. Dictionary Mapping (`PebbleDataMapper.kt`)
Update the mapper to append all new fields into the `PebbleDictionary` payload:

```kotlin
package app.aaps.plugins.pebble

import app.aaps.plugins.pebble.data.EnrichedData
import com.getpebble.android.kit.util.PebbleDictionary
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class PebbleDataMapper @Inject constructor() {
    fun map(data: EnrichedData): PebbleDictionary {
        val dict = PebbleDictionary()
        
        // 1. BG, Trend, and Time
        data.bg?.let { dict.addInt32(PebbleKeys.BG, it.toInt()) }
        data.trend?.let { dict.addInt32(PebbleKeys.TREND, it) }
        dict.addInt32(PebbleKeys.TIME, (data.time / 1000).toInt())
        
        // 2. Active Treatments (IOB/COB/Basal)
        data.iob?.let { dict.addString(PebbleKeys.IOB, it) }
        data.cob?.let { dict.addString(PebbleKeys.COB, it) }
        data.basal?.let { dict.addString(PebbleKeys.BASAL, it) }
        data.iobDetail?.let { dict.addString(PebbleKeys.IOB_DETAIL, it) }
        
        // 3. Deltas
        data.delta?.let { dict.addString(PebbleKeys.DELTA, it) }
        data.avgDelta?.let { dict.addString(PebbleKeys.AVG_DELTA, it) }
        
        // 4. Targets
        data.lowTarget?.let { dict.addInt32(PebbleKeys.LOW_TARGET, it) }
        data.highTarget?.let { dict.addInt32(PebbleKeys.HIGH_TARGET, it) }
        
        // 5. Glucose History
        data.history?.let { dict.addBytes(PebbleKeys.GLUCOSE_HISTORY, it) }

        return dict
    }
}
```
