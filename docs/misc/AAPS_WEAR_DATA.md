Here is the complete list of data available to send from the AndroidAPS mobile app to a watch companion, based on the Wear OS implementation.

  The transmission logic resides in DataHandlerMobile.kt and the data types are defined in EventData.kt.
  ──────
  ### 1. Blood Glucose Status (EventData.kt)

  Sent immediately on every new BG reading.

  • sgv (Double): Raw sensor glucose value (always in mg/dL).
  • sgvString (String): Formatted BG value in the user's preferred units (e.g., "120" or "6.7").
  • glucoseUnits (String): Unit label (e.g., "mg/dL" or "mmol/L").
  • slopeArrow (String): Trend arrow symbol (e.g., ⇈, ↑, ↗, →, ↘, ↓, ⇊).
  • delta (String): Change since last reading (e.g., "+5" or "-0.3").
  • deltaDetailed (String): High-precision delta.
  • avgDelta (String): Average change over a short window.
  • avgDeltaDetailed (String): High-precision average delta.
  • sgvLevel (Long): Threshold state (1 = high alert, -1 = low alert, 0 = normal).
  • high (Double): User's high mark threshold (mg/dL).
  • low (Double): User's low mark threshold (mg/dL).
  • timeStamp (Long): Epoch timestamp of the glucose reading.
  • deltaMgdl (Double?): Raw delta value in mg/dL.
  • avgDeltaMgdl (Double?): Raw average delta value in mg/dL.

  ### 2. Loop & Pump Status (EventData.kt)

  The primary state summary of the pump and loop calculations.

  • externalStatus (String): Multi-line string with details on active temp targets, loop runtimes, and sensor state.
  • iobSum (String): Total Insulin On Board (IOB) (e.g., "1.65").
  • iobDetail (String): IOB broken down by bolus vs basal as "(bolus|basal)" (e.g., "(1.20|0.45)").
  • cob (String): Carbs On Board (COB) (e.g., "15").
  • currentBasal (String): Current basal rate (e.g., "1.20 U/h").
  • battery (String): Phone battery percentage.
  • rigBattery (String): Battery of the rig / pump uploader.
  • openApsStatus (Long): Timestamp of the last loop run/enactment.
  • bgi (String): Blood Glucose Impact (BGI) (e.g., "-1.2" or "+0.4").
  • batteryLevel (Int): Warning level (1 = ≥30%, 0 = <30%).
  • patientName (String): Profile owner's name (useful for followers).
  • tempTarget (String): Current active temporary target (e.g., "110-130 mg/dL").
  • tempTargetLevel (Int): Target state color indicator (2 = active temp target, 1 = overridden/green, 0 = default).
  • reservoir (Double): Remaining insulin in pump reservoir (units).
  • reservoirString (String): Formatted reservoir level (e.g. "120 U").
  • reservoirLevel (Int): Reservoir warning state (2 = urgent, 1 = warning, 0 = normal).

  ### 3. Display Preferences (EventData.kt)

  User settings required to format and validate inputs locally.

  • wearControl (Boolean): Whether watch controls (bolus, temp targets, etc.) are enabled.
  • unitsMgdl (Boolean): true if mg/dL, false if mmol/L.
  • bolusPercentage (Int): Default correction percentage.
  • maxCarbs (Int): Maximum carb safety threshold.
  • maxBolus (Double): Maximum bolus safety threshold.
  • insulinButtonIncrement1 / 2 (Double): Value increments for insulin input buttons.
  • carbsButtonIncrement1 / 2 (Int): Value increments for carb input buttons.

  ### 4. Historic Glucose Data (EventData.kt)

  Used to build a trend graph on the watch.

  • entries (ArrayList<SingleBg>): Array of recent historical BG values (typically bucketed data).

  ### 5. Treatment History (EventData.kt)

  Recent treatment events (past 5.5 hours) and prediction values.

  • temps: Temporary basal runs (startTime, startBasal, endTime, endBasal, amount).
  • basals: Scheduled basal profiles (startTime, endTime, amount).
  • boluses: Treatments (date timestamp, bolus units, carbs grams, isSMB boolean, isValid boolean).
  • predictions: Projected future glucose values calculated by the loop (ArrayList<SingleBg>).

  ### 6. Quick Wizard Presets (EventData.kt)

  Pre-configured bolus/carb ratios defined on the phone.

  • entries: Array of presets containing guid, buttonText, carbs, and validity start/end times (validFrom / validTo in minutes from midnight).

  ### 7. Custom Actions & Automations (EventData.kt)

  Registered automated profiles or action events.


### Blood Glucose Status (EventData.SingleBg) Fields & Defaults

Field Name       │ Type    │ Unit Mode: mg/dL (… │ Unit Mode: mmol/L (e.g.… │ Used by defa… │ Notes
──────────────────┼─────────┼─────────────────────┼──────────────────────────┼───────────────┼───────────────────────────────────────────────────────────────────────────────────────────
sgv              │ Double  │ 120.0               │ 120.0 (always raw mg/dL) │      Yes      │ Used to calculate angles, position points on the history graph, and assess thresholds.
sgvString        │ String  │ "120"               │ "6.7"                    │      Yes      │ Drawn as the primary large blood glucose text on all watchfaces.
glucoseUnits     │ String  │ "mg/dL"             │ "mmol/L"                 │      No       │ Not rendered directly; watchfaces check settings (Preferences.unitsMgdl) instead.
slopeArrow       │ String  │ "↑" (Single Up),    │ Same as mg/dL            │      Yes      │ Rendered immediately next to the glucose value or mapped to an arrow/direction icon.
                │         │ "→" (Flat)          │                          │               │
delta            │ String  │ "+5" or "-12"       │ "+0.3" or "-0.7"         │      Yes      │ Rendered as the standard rate-of-change text value below/next to the glucose number.
deltaDetailed    │ String  │ "+5.2" or "-12.0"   │ "+0.29" or "-0.67"       │   Optional    │ Rendered in place of delta if the user enables the "Detailed Delta" watch setting.
avgDelta         │ String  │ "+3" or "-8"        │ "+0.2" or "-0.4"         │      Yes      │ Enabled by default in most layouts to show the average delta trend.
avgDeltaDetailed │ String  │ "+2.7" or "-8.1"    │ "+0.15" or "-0.45"       │   Optional    │ Rendered in place of avgDelta if "Detailed Delta" is enabled in watch settings.
sgvLevel         │ Long    │ 0L (Normal), 1L     │ Same as mg/dL            │      Yes      │ Crucial for styling; determines the color of the BG text (e.g., Red for low, Yellow for
                │         │ (High), -1L (Low)   │                          │               │ high, White/Green for normal).
high             │ Double  │ 180.0               │ 180.0 (always raw mg/dL) │      Yes      │ Defines the upper bound lines on the graph and checks for high warnings.
low              │ Double  │ 70.0                │ 70.0 (always raw mg/dL)  │      Yes      │ Defines the lower bound lines on the graph and checks for low warnings.
timeStamp        │ Long    │ 1786272465000L      │ Same as mg/dL            │      Yes      │ Used to calculate the minutes-since-last-sync/stale warnings (displays --- or warning
                │         │                     │                          │               │ states if the time delta is too large).
deltaMgdl        │ Double? │ 5.2                 │ 5.2                      │      Yes      │ Read by custom user complications and CustomWatchface.kt configuration mapping.
avgDeltaMgdl     │ Double? │ 2.67                │ 2.67                     │      Yes      │ Read by custom user complications and CustomWatchface.kt configuration mapping.

### Loop & Pump Status (EventData.Status) Fields & Defaults

Field Name      │ Type   │ Value Example                       │ Used by default in WearOS… │ Notes / Usage
─────────────────┼────────┼─────────────────────────────────────┼────────────────────────────┼───────────────────────────────────────────────────────────────────────────────────────────
externalStatus  │ String │ "" (empty string) or "Disabled      │            Yes             │ Concatenated on the bottom status line to show warning status text if the closed loop is
               │        │ Loop\n"                             │                            │ turned off.
iobSum          │ String │ "1.65"                              │            Yes             │ Displays total active insulin (IOB) on the main screen (e.g. renders as "1.65 U").
iobDetail       │ String │ "(1.20|0.45)"                       │          Optional          │ Shows the split between bolus and basal IOB when "Detailed IOB" is enabled in settings.
cob             │ String │ "15" or "15g"                       │            Yes             │ Displays current carbs on board. Renders directly on the screen (e.g. "15g").
currentBasal    │ String │ "1.20 U/h"                          │            Yes             │ Displays the active basal rate (standard basal profile or temporary basal rate).
bgi             │ String │ "-1.2" or "+0.4"                    │          Optional          │ Blood Glucose Impact string. Can be toggled on/off via the "Show BGI" watch setting.
tempTarget      │ String │ "110-130 mg/dL" or "6.1-7.2 mmol/L" │            Yes             │ Displays the active target range. Only shown when there is an active profile target or
               │        │                                     │                            │ temporary target.
tempTargetLevel │ Int    │ 0, 1, or 2                          │            Yes             │ Color-codes the temporary target text: • 0: Standard Profile (White)• 1: Loop Overridden
               │        │                                     │                            │ Target (Green)• 2: Active Temp Target (Yellow/Orange)

