# Contributing to PebbleAAPS

Thank you for your interest in contributing to **PebbleAAPS**! Whether you are fixing bugs, improving typography and design, optimizing battery life, or adding features, we welcome your contributions.

---

## 📜 1. Code of Conduct & Licensing

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**. All contributions submitted to this repository will be licensed under GPLv3.

---

## 🛠️ 2. Setting Up Your Development Environment

### Prerequisites
1. **Pebble SDK (v4.17 or later)**: Installed via `pebble-tool` or Rebble SDK toolchain.
2. **GCC Toolchain**: Required for ARM cross-compilation (`arm-none-eabi-gcc`) as well as host testing (`gcc`).
3. **Python 3.x & PIL (Pillow)**: Used by Pebble SDK build tools and screenshot generator scripts.

### Quick Verification
Run the following in your shell to confirm your development tools are installed:
```bash
pebble --version
gcc --version
```

---

## 🔄 3. Development Workflow (TDD First)

We follow a **Test-Driven Development (TDD)** workflow. Business logic lives strictly in `src/c/logic.c` and is completely decoupled from Pebble SDK UI dependencies.

### Step 1: Run Host Unit Tests
Before touching UI components, implement or update logic tests in `tests/test_logic.c` and run the host test suite:
```bash
gcc -Wall -Wextra src/c/logic.c tests/test_logic.c -o test_runner && ./test_runner && rm test_runner
```
All unit tests MUST pass cleanly with zero warnings or errors.

### Step 2: Build Watchapp Bundle
Compile the watchface application for all Pebble platforms (`basalt`, `chalk`, `diorite`, `emery`):
```bash
pebble build
```
The compiled bundle will be created at `build/PebbleAAPS.pbw`.

### Step 3: Emulator & Visual Screenshot Testing
You can launch the QEMU emulator to test layout modifications and capture screenshots:
```bash
# Start Virtual Display (if running headless/SSH)
Xvfb :99 -screen 0 1280x800x24 &

# Launch Emery Emulator & Install App
pebble install --emulator emery --vnc

# Send Mock AAPS AppMessage Payload
pebble send-app-message --int 0=120 1=5 4=$(date +%s) --string 2="0.32 U" 3="15g" 5="0.90" 6="(0.02|0.31)" 7="+3" 8="+5"

# Capture Screenshot
pebble screenshot resources/screenshots/emery_screenshot.png
```

Alternatively, use the helper scripts in `scripts/`:
- `python3 scripts/generate_clean_screenshots.py` — Generates Light Mode Emery layout screenshots.
- `./scripts/generate_store_screenshots.py` — Automates QEMU app message injection and captures pixel-perfect store screenshots.

---

## 📂 4. Project Structure

```
├── LICENSE                 # GNU General Public License v3.0
├── README.md               # User overview & installation guide
├── TASKS.md                # Task roadmap & implementation checklist
├── CONTRIBUTING.md         # This guide
├── package.json            # Pebble app manifest, UUID, keys, pebble-scalable
├── wscript                 # Pebble SDK Waf build rules
├── src/c/
│   ├── logic.h / logic.c   # Pure C business logic (Host testable)
│   ├── state_store.h/.c    # Pebble persistent storage wrapper
│   └── pebble_watchface.c  # Pebble UI, Tick handlers, GPaths & AppMessage inbox
├── tests/
│   └── test_logic.c        # Host unit test runner
├── docs/
│   ├── README.md           # Documentation index
│   ├── design/             # Coordinate tables & visual design specs
│   └── eng/                # Architecture docs & AAPS Pebble Protocol spec
├── scripts/                # Screenshot, asset, and emulator automation scripts
└── resources/              # PNG images, TTF fonts, and store screenshots
```

---

## 📐 5. Code Style & Guidelines

1. **Memory & RAM Safety**:
   - Avoid dynamic heap allocation (`malloc`/`free`) inside event callbacks or draw handlers.
   - Disassociate `GBitmap` resources from `BitmapLayer` before destroying to prevent use-after-free crashes.
2. **Responsive Layouts**:
   - Always use `pebble-scalable` macros (`scl_x()`, `scl_y()`, `scl_font()`) for UI layer positioning to ensure compatibility across all Pebble screen sizes.
3. **Documentation**:
   - Keep `docs/design/pebble_aaps_design_spec.md` and `docs/eng/androidaps_pebble_protocol.md` updated when changing AppMessage keys or coordinate layouts.

---

## 🔀 6. Submitting Pull Requests

1. Fork the repository and create a feature branch (`git checkout -b feature/my-cool-feature`).
2. Verify all host tests pass (`gcc -Wall -Wextra src/c/logic.c tests/test_logic.c -o test_runner && ./test_runner`).
3. Ensure `pebble build` succeeds across all platforms.
4. Commit your changes with descriptive commit messages following Conventional Commits (e.g., `feat(ui): ...`, `fix(logic): ...`, `docs: ...`).
5. Open a Pull Request on GitHub with a clear description and screenshot preview if UI elements were changed.
