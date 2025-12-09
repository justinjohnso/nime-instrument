# 🎹 NIME Controller - Visual Quick Reference (v0.5)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          🎵 TWO-HANDED CONTROLLER 🎵                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## ✋ LEFT HAND - Note Articulation

```
        ┌────────────────────────────────────────┐
        │     🎹 5 FINGER BUTTONS (D6-D10)      │
        ├────────────────────────────────────────┤
        │  [1] [2] [3] [4] [5]                  │
        │   │   │   │   │   │                   │
        │  Play scale degrees in current key    │
        │                                        │
        │  Normal Mode:     Press = ON, Release = OFF│
        │  Arpeggio Mode:   Hold = CYCLE TRIAD  │
        │  Latch Mode:      Press = LATCH & RE-TRIGGER│
        │  Chord Mode:      Press = PLAY CHORD  │
        │  Key Set Mode:    Press = SELECT ROOT KEY │
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
        │  ⚠️  May not be enabled in v0.5       │
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
        │    WINDOW+   SHARP♯   FLAT♭  WINDOW-  │
        │                                        │
        │              [THUMB]                   │
        │                 ↓                      │
        │            SHIFT KEY                   │
        └────────────────────────────────────────┘
```

### 🎯 SINGLE BUTTON ACTIONS (No Thumb)

```
╔══════════╦════════════════════════════════════╗
║  BUTTON  ║           FUNCTION                 ║
╠══════════╬════════════════════════════════════╣
║  INDEX   ║  ⬆️  WINDOW UP +1 scale degree     ║
║  PINKY   ║  ⬇️  WINDOW DOWN -1 scale degree   ║
║  MIDDLE  ║  ♯  MOMENTARY SHARP (hold)         ║
║          ║     +1 semitone (chromatic)        ║
║  RING    ║  ♭  MOMENTARY FLAT (hold)          ║
║          ║     -1 semitone (chromatic)        ║
╚══════════╩════════════════════════════════════╝

⚠️  Note: Sharp/flat are CHROMATIC shifts, not scale-aware
    For example, in C major, sharp on E gives F (not F#)
```

### 🎼 COMBO ACTIONS (No Thumb)

```
╔═══════════════════╦═══════════════════════════════╗
║   BUTTON COMBO    ║         FUNCTION              ║
╠═══════════════════╬═══════════════════════════════╣
║  MIDDLE + RING    ║  🔄 CYCLE PLAY MODE           ║
║                   ║  Single → Major Chord →       ║
║                   ║  Minor Chord → Arpeggio →     ║
║                   ║  loop back to Single          ║
╚═══════════════════╩═══════════════════════════════╝

⚠️  Note: Index/Pinky not available for combos
    (conflict with window shifting)
```

### ⚡ SHIFT COMBINATIONS (Hold THUMB + ...)

#### Window Shifting
```
╔═══════════════════╦═══════════════════════════════╗
║  THUMB + BUTTON   ║         FUNCTION              ║
╠═══════════════════╬═══════════════════════════════╣
║  THUMB + INDEX    ║  ⬆️  WINDOW UP +1 octave      ║
║                   ║     (+12 semitones)           ║
║  THUMB + PINKY    ║  ⬇️  WINDOW DOWN -1 octave    ║
║                   ║     (-12 semitones)           ║
╚═══════════════════╩═══════════════════════════════╝
```

#### Mode Toggles & Scale Selection
```
╔═══════════════════╦═══════════════════════════════╗
║  THUMB + BUTTON   ║         FUNCTION              ║
╠═══════════════════╬═══════════════════════════════╣
║  THUMB + MIDDLE   ║  🔒 TOGGLE LATCH MODE         ║
║                   ║     (OFF = clear all notes)   ║
║  THUMB + RING     ║  🔄 CYCLE SCALE               ║
║                   ║     Major → Minor → Chromatic ║
╚═══════════════════╩═══════════════════════════════╝
```





---

## 💡 PERFORMANCE TIPS

```
╔════════════════════════════════════════════════════════════════════╗
║                    🎭 PERFORMANCE WORKFLOWS                        ║
╚════════════════════════════════════════════════════════════════════╝

┌──────────────────────────────────────────────────────────────────┐
│ 🎵 Playing Across Octaves (Window Shifting)                     │
├──────────────────────────────────────────────────────────────────┤
│  1. Start in middle range (default)                             │
│  2. Press INDEX (no thumb) → shift window up by scale degree    │
│  3. Press PINKY (no thumb) → shift window down by scale degree  │
│  4. For big jumps: THUMB + INDEX/PINKY → shift by octave       │
│  5. Window range: C2 to C5 (±36 semitones)                     │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 🔄 Mode Cycling (Middle + Ring)                                 │
├──────────────────────────────────────────────────────────────────┤
│  1. Start:       Single Note (default)                          │
│  2. Press combo: Cycles to Major Chord mode                     │
│  3. Press again: Minor Chord mode                               │
│  4. Press again: Arpeggio mode                                  │
│  5. Press again: Back to Single Note                            │
│  6. Pattern:     Single→Major→Minor→Arp→loop                   │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 🔁 Arpeggio Mode (Auto-Cycling Triads)                          │
├──────────────────────────────────────────────────────────────────┤
│  1. Activate:    MIDDLE + RING until you reach Arpeggio         │
│  2. Hold button: Automatically cycles root → 3rd → 5th          │
│  3. Rhythm:      ~140ms per note (7 notes/second)               │
│  4. Switch:      Release and press different button             │
│  5. Exit:        MIDDLE + RING until back to Single Note        │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 🎼 Building Sustained Chords (Latch Mode)                       │
├──────────────────────────────────────────────────────────────────┤
│  1. Enable Latch:    THUMB + MIDDLE (tap)                       │
│  2. Press Notes:     Tap multiple left-hand buttons             │
│  3. Each Note:       Stays ON until latch disabled              │
│  4. Re-trigger:      Press latched button again for attack      │
│  5. Clear All:       THUMB + MIDDLE (toggle off)                │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 🎹 Playing Chords (Scale-Aware)                                  │
├──────────────────────────────────────────────────────────────────┤
│  1. Enter Mode:  MIDDLE + RING → Chord mode                     │
│  2. Chord Type:  Follows current scale                          │
│                  (Minor Pent = minor, others = major)           │
│  3. Press Button: Plays root + 3rd + 5th together               │
│  4. Multi-Chord: Press multiple buttons for layered chords      │
│  5. Switch Type: THUMB + RING (cycle scale)                     │
│  6. Exit:        MIDDLE + RING until back to Single Note        │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 🌊 Expressive Timbral Control (if sensor enabled)               │
├──────────────────────────────────────────────────────────────────┤
│  • Move hand CLOSE to sensor → Bright, buzzy triangle wave      │
│  • Move hand FAR from sensor → Smooth, mellow sine wave         │
│  • Works on ALL playing notes (including latched!)              │
│  • Use like a "wah" effect for live expression                  │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 🎸 Quick Scale Changes                                           │
├──────────────────────────────────────────────────────────────────┤
│  Cycle Scale:    THUMB + RING (tap repeatedly to cycle)         │
│  Window Jump:    THUMB + INDEX/PINKY for octave shifts          │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│ 🔄 Quick Reset to Defaults                                       │
├──────────────────────────────────────────────────────────────────┤
│  • Turn OFF latch mode → Clears all sustained notes             │
│  • Turn OFF arpeggio → Returns to normal single-note mode       │
│  • Release chord buttons → Returns to single-note mode          │
│  • Move hand FAR → Returns to smooth sine tone (if sensor works)│
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
║  Arpeggio         ║  OFF                          ║
║  Window Offset    ║  0 (no shift)                 ║
║  Waveform         ║  Sine (hand far)              ║
║  IMU/Accel        ║  DISABLED (v0.5)              ║
╚═══════════════════╩═══════════════════════════════╝
```

---

## 🎵 MUSICAL MODES

### Single Note Mode (Default)
- Press button = play note
- Release button = stop note
- Each button plays one scale degree
- Window shifting transposes all 5 buttons

### Arpeggio Mode
- Activate: MIDDLE + RING until you reach it
- Hold button = auto-cycle through triad (root, 3rd, 5th)
- Tempo: ~140ms per step
- Only first held button arpeggiated

### Chord Mode (Scale-Aware)
- Activate: MIDDLE + RING (cycle until you reach Chord)
- Chord quality follows scale: Minor Pentatonic = minor chords, others = major
- Press button = play triad chord (root + 3rd + 5th)
- All 3 notes sound together
- Works with window shifting
- Change chord quality: Just cycle scale (THUMB + RING)

### Latch Mode
- Toggle: THUMB + MIDDLE
- Notes stay on after button release
- Press again to retrigger envelope
- Can layer multiple notes
- Turn OFF to clear all

---

## 🔧 HARDWARE PINOUT REFERENCE

```
┌─────────────────────────────────────────────────────────┐
│  LEFT HAND CONTROLLER                                   │
├─────────────────────────────────────────────────────────┤
│  • Buttons:          D6, D7, D8, D9, D10 (5 total)     │
│  • Distance Sensor:  I2C1 (SDA=D11, SCL=D12)           │
│  • Accelerometer:    DISABLED in v0.5                  │
│  • Note: Button pins skip D11 & D12 (reserved I2C)     │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  RIGHT HAND CONTROLLER                                  │
├─────────────────────────────────────────────────────────┤
│  D15 → PINKY    (Window Down / Combos)                 │
│  D16 → RING     (Flat / Combos)                        │
│  D17 → MIDDLE   (Sharp / Combos)                       │
│  D18 → INDEX    (Window Up / Combos)                   │
│  D19 → THUMB    (SHIFT Key)                            │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  OTHER                                                  │
├─────────────────────────────────────────────────────────┤
│  • Volume Pot:  A5                                     │
│  • Audio Out:   48kHz stereo via Daisy Seed            │
└─────────────────────────────────────────────────────────┘
```

---

## 📊 SERIAL MONITOR FEEDBACK

```
The serial monitor displays real-time feedback:
  ✅ Note ON/OFF/LATCHED/RE-TRIGGERED events
  ✅ Window offset (degrees and semitones)
  ✅ Active scale (Major Pentatonic / Minor Pentatonic / Chromatic)
  ✅ Play mode (Single Note / Major Chord / Minor Chord / Arpeggio)
  ✅ Latch status (ON/OFF)
  ✅ Arpeggio note stepping
  ✅ Waveform blend (if ToF sensor active)
  ✅ Distance readings (mm, if ToF sensor active)
  ✅ Volume changes (%)
  ✅ IMU status (DISABLED in v0.5)
```

---

## 🆕 What's New in v0.5

```
✅ Window Shifting (replaces octave buttons)
   - Button-based transposition (no accelerometer needed)
   - ±1 scale degree or ±1 octave
   - Range: C2 to C5

✅ Arpeggio Mode
   - Auto-cycling triads
   - Toggle with INDEX + PINKY

✅ Scale Cycling
   - THUMB + RING cycles through scales
   - Major Pent → Minor Pent → Chromatic → loop

✅ New Button Mapping
   - Thumb as shift key for all combos
   - No conflicting 3-button combos
   - More logical grouping

✅ Gentler Attack
   - 50ms attack time (was 20ms)
   - Eliminates clicking on note onset

✅ Sharp/Flat Fix
   - Now applies to CURRENT notes (with window offset)
   - Not just base scale notes

✅ IMU Disabled
   - Accelerometer-based window shifting removed
   - More reliable button-based control
```

---

```
╔════════════════════════════════════════════════════════════════════╗
║          🎵 ENJOY PLAYING YOUR NIME CONTROLLER! 🎵                ║
╚════════════════════════════════════════════════════════════════════╝
```
