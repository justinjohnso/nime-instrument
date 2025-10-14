# 🎹 NIME Controller - Visual Quick Reference

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          🎵 TWO-HANDED CONTROLLER 🎵                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## ✋ LEFT HAND - Note Articulation

```
        ┌────────────────────────────────────────┐
        │     🎹 5 FINGER BUTTONS (D8-D12)      │
        ├────────────────────────────────────────┤
        │  [1] [2] [3] [4] [5]                  │
        │   │   │   │   │   │                   │
        │  Play scale degrees in current key    │
        │                                        │
        │  Normal Mode:  Press = ON, Release = OFF│
        │  Latch Mode:   Press = LATCH & RE-TRIGGER│
        │  Key Set Mode: Press = SELECT ROOT KEY │
        └────────────────────────────────────────┘
```

```
        ┌────────────────────────────────────────┐
        │    📏 DISTANCE SENSOR (Palm/ToF)       │
        ├────────────────────────────────────────┤
        │                                        │
        │  🌊 ←──── WAVEFORM MORPH ────→ 📐    │
        │   SINE              MIX         TRIANGLE│
        │  (smooth)                      (bright)│
        │                                        │
        │  FAR ═══════════════════════════ CLOSE │
        │  300mm        150mm           50mm     │
        │                                        │
        │  💡 Tip: "Squeeze" to add brightness!  │
        └────────────────────────────────────────┘
```

---

## 🎛️ RIGHT HAND - Modifiers & State Control

```
        ┌────────────────────────────────────────┐
        │     BUTTON LAYOUT (D15-D19)            │
        ├────────────────────────────────────────┤
        │                                        │
        │     [INDEX]  [MIDDLE]  [RING]  [PINKY]│
        │        ↑        ↑        ↑       ↓    │
        │     OCTAVE+   SHARP♯   FLAT♭  OCTAVE- │
        │                                        │
        │              [THUMB]                   │
        │                 ↓                      │
        │            SHIFT KEY                   │
        └────────────────────────────────────────┘
```

### 🎯 SINGLE BUTTON ACTIONS

```
╔══════════╦════════════════════════════════════╗
║  BUTTON  ║           FUNCTION                 ║
╠══════════╬════════════════════════════════════╣
║  INDEX   ║  ⬆️  OCTAVE UP (tap)               ║
║  PINKY   ║  ⬇️  OCTAVE DOWN (tap)             ║
║  MIDDLE  ║  ♯  MOMENTARY SHARP (hold)         ║
║  RING    ║  ♭  MOMENTARY FLAT (hold)          ║
╚══════════╩════════════════════════════════════╝
```

### ⚡ SHIFT COMBINATIONS (Hold THUMB + ...)

```
╔═══════════════════╦═══════════════════════════════╗
║  THUMB + BUTTON   ║         FUNCTION              ║
╠═══════════════════╬═══════════════════════════════╣
║  THUMB + INDEX    ║  🎵 Major Pentatonic Scale    ║
║  THUMB + MIDDLE   ║  🎸 Blues Scale               ║
║  THUMB + RING     ║  🎹 Chromatic Scale           ║
║  THUMB + PINKY    ║  🔒 TOGGLE LATCH MODE         ║
║                   ║     (OFF = clear all notes)   ║
╚═══════════════════╩═══════════════════════════════╝
```

### 🎼 CHORD MODES (Hold Combination)

```
╔═══════════════════╦═══════════════════════════════╗
║   BUTTON COMBO    ║         CHORD MODE            ║
╠═══════════════════╬═══════════════════════════════╣
║  INDEX + MIDDLE   ║  🎵 MAJOR CHORD MODE          ║
║  INDEX + RING     ║  🎶 MINOR CHORD MODE          ║
╚═══════════════════╩═══════════════════════════════╝
        Release both buttons → Return to Single Note
```

### 🎸 KEY CHANGE MODE

```
╔═══════════════════╦═══════════════════════════════╗
║   BUTTON COMBO    ║         FUNCTION              ║
╠═══════════════════╬═══════════════════════════════╣
║  MIDDLE + RING    ║  🔑 ENTER KEY SET MODE        ║
╚═══════════════════╩═══════════════════════════════╝

    While holding MIDDLE + RING:
    ┌─────────────────────────────────────────┐
    │  LEFT BUTTONS → Select root note        │
    │  RIGHT INDEX  → Transpose semitone UP   │
    │  RIGHT PINKY  → Transpose semitone DOWN │
    │  RELEASE ALL  → Confirm new key         │
    └─────────────────────────────────────────┘
```

---

## 💡 PERFORMANCE TIPS

```
╔════════════════════════════════════════════════════════════════════╗
║                    🎭 PERFORMANCE WORKFLOWS                        ║
╚════════════════════════════════════════════════════════════════════╝

┌──────────────────────────────────────────────────────────────────┐
│ 🎼 Building Sustained Chords                                     │
├──────────────────────────────────────────────────────────────────┤
│  1. Enable Latch:    THUMB + PINKY (tap)                        │
│  2. Press Notes:     Tap multiple left-hand buttons             │
│  3. Each Note:       Stays ON until latch disabled              │
│  4. Re-trigger:      Press latched button again for attack      │
│  5. Clear All:       THUMB + PINKY (tap again)                  │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 🌊 Expressive Timbral Control                                    │
├──────────────────────────────────────────────────────────────────┤
│  • Move hand CLOSE to sensor → Bright, buzzy triangle wave      │
│  • Move hand FAR from sensor → Smooth, mellow sine wave         │
│  • Works on ALL playing notes (including latched!)              │
│  • Use like a "wah" effect for live expression                  │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 🎸 Quick Key/Scale Changes                                       │
├──────────────────────────────────────────────────────────────────┤
│  Change Scale:   THUMB + INDEX/MIDDLE/RING (instant)            │
│  Change Key:     MIDDLE + RING, then tap left buttons           │
│  Octave Jump:    Rapid INDEX taps = climb, PINKY taps = descend │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 🔄 Quick Reset to Defaults                                       │
├──────────────────────────────────────────────────────────────────┤
│  • Turn OFF latch mode → Clears all sustained notes             │
│  • Release chord buttons → Returns to single-note mode          │
│  • Move hand FAR → Returns to smooth sine tone                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 🎼 DEFAULT STARTUP STATE

```
╔═══════════════════╦═══════════════════════════════╗
║    PARAMETER      ║          DEFAULT              ║
╠═══════════════════╬═══════════════════════════════╣
║  Key              ║  C Major                      ║
║  Octave           ║  4 (Middle C range)           ║
║  Scale            ║  Major Pentatonic             ║
║  Mode             ║  Single Note                  ║
║  Latch            ║  OFF                          ║
║  Waveform         ║  Sine (hand far)              ║
╚═══════════════════╩═══════════════════════════════╝
```

---

## 🔧 HARDWARE PINOUT REFERENCE

```
┌─────────────────────────────────────────────────────────┐
│  LEFT HAND CONTROLLER                                   │
├─────────────────────────────────────────────────────────┤
│  • Buttons:          D8, D9, D10, D13, D14             │
│  • Distance Sensor:  I2C1 (SDA=D11, SCL=D12)           │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  RIGHT HAND CONTROLLER                                  │
├─────────────────────────────────────────────────────────┤
│  D15 → PINKY    (Octave Down)                          │
│  D16 → RING     (Combos)                               │
│  D17 → MIDDLE   (Flat/Combos)                          │
│  D18 → INDEX    (Octave Up)                            │
│  D19 → THUMB    (SHIFT Key)                            │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  OTHER                                                  │
├─────────────────────────────────────────────────────────┤
│  • Volume Pot:  A5                                     │
└─────────────────────────────────────────────────────────┘
```

---

## 📊 SERIAL MONITOR FEEDBACK

```
The serial monitor displays real-time feedback:
  ✅ Note ON/OFF/LATCHED/RE-TRIGGERED events
  ✅ Current octave (1-8)
  ✅ Active scale (Major Pentatonic / Blues / Chromatic)
  ✅ Play mode (Single Note / Major Chord / Minor Chord)
  ✅ Latch status (ON/OFF)
  ✅ Waveform blend (Sine %% / Triangle %%)
  ✅ Distance readings (mm)
  ✅ Volume changes (%)
```

---

```
╔════════════════════════════════════════════════════════════════════╗
║          🎵 ENJOY PLAYING YOUR NIME CONTROLLER! 🎵                ║
╚════════════════════════════════════════════════════════════════════╝
```
