# PebbleAAPS Documentation Index

Welcome to the **PebbleAAPS** technical documentation hub! This directory contains architectural specifications, design blueprints, protocol definitions, and development guides for contributors, forkers, and companion app developers.

---

## 📚 Technical Documentation Map

### 1. 🏗️ Architecture & Code Organization
- **[`docs/eng/architecture.md`](eng/architecture.md)**
  - C module separation (`logic.c`, `state_store.c`, `pebble_watchface.c`).
  - Decoupled TDD host testing (`tests/test_logic.c`).
  - Screen scaling via `pebble-scalable`.
  - Palette swapping memory optimization (`gbitmap_get_palette`).

### 2. 📡 Companion App Integration Protocol
- **[`docs/eng/androidaps_pebble_protocol.md`](eng/androidaps_pebble_protocol.md)**
  - Pebble AppMessage key mappings (`0` to `12`).
  - Expected data types (Integers, Strings, Byte Arrays).
  - Glucose unit handling (`mg/dL` vs `mmol/L`).
  - 36-point Glucose History Graph byte encoding.

### 3. 🎨 UI Layout & Design Blueprint
- **[`docs/design/pebble_aaps_design_spec.md`](design/pebble_aaps_design_spec.md)**
  - High-Contrast Light Mode design rationale.
  - Precise element coordinate tables and font scaling rules.
  - Palette-swapped trend arrow mechanics.
  - Dashed target range line drawing rules.

### 4. 🛠️ Development & Contribution Guide
- **[`CONTRIBUTING.md`](../CONTRIBUTING.md)**
  - Local toolchain installation & setup (`pebble-tool`, GCC).
  - Running host unit tests (`gcc -Wall src/c/logic.c tests/test_logic.c ...`).
  - Compiling `.pbw` bundles (`pebble build`).
  - Emulator screenshot & state testing tools (`scripts/`).
  - Pull Request workflow and coding guidelines.

---

## 🔍 Quick Links for New Developers
- **Root README**: [`README.md`](../README.md)
- **Rebble Store Listing**: [`PEBBLE_DESCRIPTION.md`](PEBBLE_DESCRIPTION.md)
- **Roadmap & Tasks**: [`TASKS.md`](../TASKS.md)
- **License**: [`LICENSE`](../LICENSE) (GNU General Public License v3.0)
