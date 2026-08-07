# Pebble AAPS Watchface (Pebble Time 2 MVP)

A lightweight, low-power, and robust Pebble C watchface designed specifically for the **Pebble Time 2** (Emery platform, 200x228 color display) that connects to **AndroidAPS (AAPS)** to show real-time glucose status.

---

## Features

- **Pebble Time 2 Optimization**: Layout, typography, and resources tailored specifically for the 200x228 pixel 64-color screen.
- **Large Readability**: Uses high-visibility `LECO_36` and `LECO_42` fonts for time and glucose values.
- **Dynamic Asset Loading**: Dynamically loads 48x48 color trend icons as needed. This prevents heap fragmentation and keeps the RAM footprint under 2.3KB.
- **TDD (Test-Driven Development) Architecture**: Decouples AAPS state tracking and age formatting logic from the Pebble SDK. This enables host-machine compilation (`gcc`) and automated unit testing without requiring an active emulator.
- **Data Freshness Cues**: Updates reading ages every minute using the Pebble Tick Service. Turns age text **Red** (e.g., `15m ago`) when readings exceed 15 minutes old to warn you of stale data.
- **Out-of-Bounds Protection**: Safely boundary guards AAPS enum trend array indexes, falling back to a neutral gray `?` icon if invalid inputs are received instead of crashing the watchface.

---

## Screen Layout

```
+-----------------------------------+
|             10:42 AM              |  <- System Time (LECO font)
|                                   |
|       120     [Color Arrow]       |  <- Blood Glucose & Trend Arrow (48x48)
|                                   |
|              3m ago               |  <- Age of reading (Red if >= 15m)
+-----------------------------------+
```

---

## File Structure

```
├── package.json          # App metadata, UUID settings, keys mapping
├── wscript               # Pebble SDK build configuration rules
├── README.md             # This document
├── TASKS.md              # Roadmap and verification checklist
├── src/
│   └── c/
│       ├── logic.h       # Decoupled state struct & logic declarations
│       ├── logic.c       # Logic implementation (C-only, no Pebble SDK dependencies)
│       └── pebble_watchface.c # Main Pebble UI, Tick, and AppMessage shell
├── tests/
│   └── test_logic.c      # Host unit test suite runner
└── resources/
    └── images/           # 48x48 color arrow PNG resource icons
```

---

## Local Build & Host-Based Testing

### 1. Run Unit Tests (TDD Cycle)
Compile and run the test harness on your host machine to verify core logic:
```bash
gcc -Wall -Wextra src/c/logic.c tests/test_logic.c -o test_runner && ./test_runner && rm test_runner
```

### 2. Build Watchapp Bundle
Compile and build the final Pebble `.pbw` application package:
```bash
pebble build
```
The compiled bundle will be output to `build/PebbleAAPS.pbw`.

---

## Emulator Testing (Headless/VNC)

If working over SSH or headless:

1. **Start virtual display**:
   ```bash
   Xvfb :99 -screen 0 800x600x24 &
   ```
2. **Install and run the Emery emulator with VNC enabled**:
   ```bash
   pebble install --emulator emery --vnc
   ```
3. **Capture screenshot to verify UI**:
   ```bash
   pebble screenshot tmp/screenshots/test.png
   ```
4. **Send mock data updates**:
   - *Normal update*:
     ```bash
     pebble send-app-message --int 0=115 1=5 4=$(date +%s)
     ```
   - *Stale data update*:
     ```bash
     pebble send-app-message --int 0=130 1=3 4=$(( $(date +%s) - 1200 ))
     ```
   - *Out-of-bounds boundary fallback*:
     ```bash
     pebble send-app-message --int 0=95 1=15 4=$(date +%s)
     ```

---

## Hardware Sideloading & Integration

1. Copy the compiled bundle `build/PebbleAAPS.pbw` to your Android phone.
2. Tap the file in your phone's File Manager and select **Gadgetbridge** (or the official **Pebble app**) to install it on your watch.
3. In **AndroidAPS**, open the **Config Builder**, check the box next to **Pebble** in the *Sync plugins* section, and confirm settings match. AAPS will now automatically sync readings directly to your watchface.
