# AGENTS.md - Development Context & Decisions

---
## ⚠️ CRITICAL: Context & Thread Tracking

### Before Starting Work
1. **Read this entire AGENTS.md** to understand project state and conventions
2. **Use `read_thread` on relevant threads** from the history below based on what you're working on:
   - Hardware/wiring issues → Phase 1 threads
   - Audio synthesis → Phase 2 threads
   - Button/input handling → Phase 3 threads
   - Gesture control (VL53L0X) → Phase 4 threads
   - Performance/edge cases → Phase 5 threads
3. **Check `git log` and `git diff`** when you need to understand recent changes or debug regressions

### Before Ending Work
**ALWAYS add this thread's ID to the "NIME MIDI Controller Thread History" section at the bottom of this file.**

Format: `- [T-{id}](https://ampcode.com/threads/T-{id}) - Brief description of what was done`

The current thread URL is in your Environment context. This maintains project continuity across sessions.

**Do this EARLY** - as soon as you've made meaningful changes, add the thread to history. Don't wait until the end, as context may run out unexpectedly.

---

## Project Overview
Two-handed musical instrument controller on Electrosmith Daisy Seed (STM32H750, ARM Cortex-M7). Features scale-locked note articulation, modal control switching, and gesture-based timbral control.

## Environment Management

### Tool Choice: PlatformIO
- **Why PlatformIO:** Manages embedded dependencies, handles DFU upload, cross-platform
- **Framework:** Arduino (via DaisyDuino)

### Commands
```bash
pio run                    # Build
pio run -t upload          # Upload via DFU (hold BOOT, press RESET first)
pio run -t clean           # Clean build
pio device monitor         # Serial monitor (115200 baud)
```

---

## Hardware Configuration

### Core Platform
- **Electrosmith Daisy Seed** (STM32H750, 480MHz ARM Cortex-M7)
- **Audio codec**: Built-in 48kHz stereo

### Pin Assignments

#### Left Hand (Note Articulation)
- Buttons: D8, D9, D10, D13, D14
- VL53L0X Sensor: I2C1 (SDA=D11, SCL=D12)

#### Right Hand (Modifiers)
- Buttons: D15 (pinky), D16 (ring), D17 (middle), D18 (index), D19 (thumb)

#### Analog Controls
- Volume Pot: A5

### Dependencies (managed by PlatformIO)
- DaisyDuino (from electro-smith GitHub)
- Adafruit_VL53L0X@^1.2.4
- Adafruit MSA301@^1.1.1
- Wire (I2C library)

---

## Audio & Synthesis

### Parameters
- **Sample Rate:** 48kHz
- **Synthesis:** Dual oscillator per note (sine + triangle) with equal-power crossfade
- **Polyphony:** 5 simultaneous notes maximum
- **Control Rate:** ~1kHz (1ms loop interval)
- **Sensor Poll Rate:** 20Hz (50ms interval)

### Key Configuration (in src/main.cpp)
```cpp
const float VOLUME_SCALE = 0.5f;          // Max volume (0.0-1.0)
const int DISTANCE_MIN = 50;              // Min distance (mm)
const int DISTANCE_MAX = 300;             // Max distance (mm)
const int OCTAVE_MIN = 1;
const int OCTAVE_MAX = 8;
```

---

## File Structure

```
nime-midi-controller/
├── src/
│   └── main.cpp              # Main application code
├── include/                  # Header files
├── lib/                      # Local libraries
├── docs/
│   ├── reference/
│   │   └── CONTROL_REFERENCE.md  # Visual control reference
│   └── technical/
│       ├── ARCHITECTURE_OVERVIEW.md  # System architecture
│       └── IMPLEMENTATION_PLAN.md    # Feature roadmap
├── platformio.ini            # PlatformIO configuration
└── AGENTS.md                 # This file
```

---

## Development Workflow

### Upload Process
1. Put Daisy Seed in bootloader mode (hold BOOT button, press RESET)
2. Run `pio run -t upload`
3. Device auto-resets after upload

### Debugging
- Serial monitor shows I2C scan, button events, sensor readings
- Check `pio device monitor --baud 115200`

---

## Known Limitations & Workarounds

1. **VL53L0X sensor detection:** Must be on I2C address 0x29. Check wiring if not detected.

2. **Button debounce:** Default 1000ms debounce - adjust if buttons feel unresponsive.

3. **DFU upload:** Requires manual bootloader entry (BOOT + RESET).

---

## NIME MIDI Controller Thread History

**When starting a new thread for controller work, reference the most recent thread below as context.**

### Phase 1: Initial Hardware Bring-Up & IO
(Add threads here: PlatformIO setup, DaisyDuino integration, button wiring, basic audio out)

### Phase 2: Audio Synthesis & Voicing
(Add threads here: oscillator implementation, polyphony, waveform morphing)

### Phase 3: Input Mapping & Scale Logic
(Add threads here: note mapping, scales, modes, key selection)

### Phase 4: Gesture Control & Sensors
(Add threads here: VL53L0X distance sensor, accelerometer, gesture-to-parameter mapping)

### Phase 5: Performance Tuning & Edge Cases
(Add threads here: latency, noise issues, weird interaction bugs, UX polish)

---

## References
- [DaisyDuino GitHub](https://github.com/electro-smith/DaisyDuino)
- [CONTROL_REFERENCE.md](./docs/reference/CONTROL_REFERENCE.md)
- [ARCHITECTURE_OVERVIEW.md](./docs/technical/ARCHITECTURE_OVERVIEW.md)
