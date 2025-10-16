# NIME Controller Architecture Overview

## Project Vision

A two-handed musical instrument controller designed for expressive, constraint-based performance. The left hand articulates notes within a scale, while the right hand modifies musical parameters and system state. The distance sensor adds a gestural dimension for timbral control.

---

## Hardware Architecture

### Platform: Electrosmith Daisy Seed
- **MCU:** STM32H750 (ARM Cortex-M7, 480MHz)
- **Framework:** Arduino via DaisyDuino
- **Audio:** 48kHz stereo output, hardware audio codec
- **Flash:** 64MB QSPI external flash
- **RAM:** 512KB

### Input Hardware

#### Left Hand - Note Articulation
```
Physical Buttons (5):
├── Button 1 (D8)  → Scale degree 1
├── Button 2 (D9)  → Scale degree 2  
├── Button 3 (D10) → Scale degree 3
├── Button 4 (D13) → Scale degree 4 (D11, D12 used by I2C)
└── Button 5 (D14) → Scale degree 5

VL53L0X ToF Sensor (I2C1):
├── SDA: D11
├── SCL: D12
└── Range: 50-300mm for gesture control
```

#### Right Hand - Modifiers
```
Physical Buttons (5):
├── Pinky  (D15) → Octave down
├── Ring   (D16) → Momentary flat
├── Middle (D17) → Momentary sharp  
├── Index  (D18) → Octave up
└── Thumb  (D19) → SHIFT key
```

#### Analog Controls
```
└── Volume Pot (A5) → Global volume (0-50%)
```

---

## Software Architecture

### Layer 1: Hardware Abstraction

**DaisyDuino Layer:**
- Audio callback: `AudioCallback(float **in, float **out, size_t size)`
- Sample rate management: 48kHz
- Switch debouncing: `Switch` class with 1000ms timeout
- Oscillator DSP primitives: `Oscillator` class

**Direct Hardware:**
- Analog read via Arduino ADC functions
- I2C via Arduino Wire library
- VL53L0X via Adafruit library

### Layer 2: State Management

#### Musical State
```cpp
struct MusicalState {
    int currentOctave;        // 1-8
    int currentKey;           // 0-11 (semitone offset)
    int currentScale;         // MAJOR_PENTATONIC, BLUES, CHROMATIC
    int pitchOffset;          // Momentary ±1 semitone
    int currentScaleNotes[5]; // Calculated MIDI notes
}
```

#### Control State
```cpp
struct ControlState {
    bool leftButtonStates[5];      // Logical note on/off
    bool leftButtonPrevStates[5];  // Previous physical state
    bool rightButtonStates[5];     // Current physical state
    bool rightButtonPrevStates[5]; // Previous physical state
    bool latchMode;                // Note latch toggle
    int currentMode;               // Single/Major/Minor chord
}
```

#### Audio State
```cpp
struct AudioState {
    float volume;           // Global volume (0.0-0.5)
    float waveformBlend;    // 0.0=sine, 1.0=triangle
    float sineAmp;          // Sine oscillator amplitude
    float triAmp;           // Triangle oscillator amplitude
    Oscillator oscSine[5];  // Per-note sine oscillators
    Oscillator oscTri[5];   // Per-note triangle oscillators
}
```

### Layer 3: Control Logic

#### Input Processing Pipeline
```
Physical Input → Debouncing → State Detection → Logic Processing → State Update
```

**Right Hand Processing:**
1. Read all button states
2. Detect combinations (thumb + others)
3. Process modal controls (shift held vs released)
4. Update musical state (scale, octave, key, mode)
5. Apply pitch offsets to playing notes
6. Store previous states

**Left Hand Processing:**
1. Read all button states  
2. Detect rising/falling edges
3. Check current mode (normal, latch, key-set)
4. Update logical note states
5. Trigger/re-trigger oscillators
6. Store previous states

**Analog & Sensor Processing:**
1. Read volume pot (with hysteresis)
2. Poll ToF sensor (rate limited)
3. Map distance to waveform blend
4. Update oscillator amplitudes

### Layer 4: Audio Synthesis

#### Audio Callback (Real-time)
```
For each audio sample:
  1. Initialize mixed signal = 0
  2. For each note (0-4):
     - If note is playing:
       - Process sine oscillator
       - Process triangle oscillator  
       - Add weighted (sineAmp, triAmp) to mix
  3. Apply global volume (with polyphony attenuation)
  4. Output to left and right channels
```

**Key Characteristics:**
- **Zero latency:** Direct oscillator → output
- **Equal-power crossfade:** `sin²(x) + cos²(x) = 1` for constant perceived volume
- **Polyphony limiting:** `0.3f` multiplier prevents clipping with 5 simultaneous notes

---

## Control Flow Patterns

### Button State Machine (Per Button)

```
Physical State:    RELEASED ←→ PRESSED
                      ↓            ↓
Logical State:   [Mode Dependent Processing]
                      ↓            ↓
                   Note OFF    Note ON (or latched)
```

### Right Hand Modal Control

```
SHIFT Released:
├── Index     → Octave Up
├── Middle    → Momentary Sharp (hold)
├── Ring      → Momentary Flat (hold)
└── Pinky     → Octave Down

SHIFT Held:
├── Index                → Select Major Pentatonic
├── Middle               → Select Blues Scale
├── Ring                 → Select Chromatic Scale
├── Pinky                → Toggle Latch Mode
├── Index + Middle       → Major Chord Mode
├── Index + Ring         → Minor Chord Mode
└── Middle + Ring        → Key Set Mode
```

### Gesture Control Mapping

```
Distance Sensor (50-300mm):
├── 50mm   → 100% Triangle (bright, buzzy)
├── 175mm  → 50/50 Blend
└── 300mm  → 100% Sine (smooth, mellow)

Equal-power crossfade curve:
  triAmp  = sin(blend * π/2)
  sineAmp = cos(blend * π/2)
```

---

## Scale System

### Scale Definition
Each scale maps 5 buttons to semitone intervals from the root note:

```cpp
Major Pentatonic: [0, 2, 4, 7, 9]   // C, D, E, G, A
Blues:            [0, 3, 5, 6, 7]   // C, Eb, F, F#, G
Chromatic:        [0, 1, 2, 3, 4]   // C, C#, D, D#, E
```

### Note Calculation
```
MIDI Note = (octave × 12) + key + scale_interval[button]

Examples:
- Octave 4, Key C (0), Major Pentatonic, Button 0:
  = (4 × 12) + 0 + 0 = 48 (C3 in some conventions, C4 in others)
  
- Octave 5, Key D (2), Blues, Button 2:
  = (5 × 12) + 2 + 5 = 67 (G4)
```

### Pitch Offset (Momentary Sharp/Flat)
```
When sharp/flat pressed:
  Applied MIDI Note = Base Note + pitchOffset
  
Effect:
  - Shifts ALL currently playing notes
  - Returns to base pitch on release
  - ±1 semitone only
```

---

## Performance Optimization

### Control Rate (1kHz)
- Main loop: 1ms delay
- Button scanning: every loop
- Volume reading: every loop (with hysteresis)
- Adequate for human interaction timing

### Sensor Poll Rate (20Hz)
- Distance sensor: 50ms interval
- Sufficient for gestural control
- Prevents I2C bus saturation

### Audio Rate (48kHz)
- Callback processes 48,000 samples/second
- Minimal branching in callback
- All oscillators pre-initialized
- Simple additive synthesis

### Memory Footprint
- Static allocation only
- 10 oscillator instances (small)
- No dynamic memory allocation
- Stack usage minimal

---

## Design Patterns

### 1. Hysteresis Pattern
**Problem:** Noisy analog inputs cause flickering output  
**Solution:** Threshold-based change detection

```cpp
if (abs(newValue - lastValue) > THRESHOLD) {
    // Process change
    lastValue = newValue;
}
```

**Applied to:**
- Volume pot (10 ADC count threshold)
- Distance sensor (5mm threshold)

### 2. Edge Detection Pattern
**Problem:** Need to detect button press/release moments  
**Solution:** Compare current vs previous state

```cpp
bool rising  = pressed && !wasPressed;   // Just pressed
bool falling = !pressed && wasPressed;   // Just released
```

**Enables:**
- One-shot actions on press/release
- Latch toggle behavior
- Modal control switching

### 3. Modal Control Pattern
**Problem:** Limited buttons, many functions  
**Solution:** SHIFT key creates alternate button meanings

```cpp
if (shiftHeld) {
    // Alternate functions
} else {
    // Normal functions
}
```

**Benefit:** 5 right-hand buttons provide 10+ functions

### 4. Equal-Power Crossfade
**Problem:** Linear crossfade causes volume dip in middle  
**Solution:** Trigonometric amplitude curves

```cpp
float angle = blend * (PI / 2.0f);
amp1 = sin(angle);  // 0.0 → 1.0
amp2 = cos(angle);  // 1.0 → 0.0
// amp1² + amp2² = 1 (constant power)
```

**Result:** Perceptually constant volume across blend range

### 5. State Synchronization Pattern
**Problem:** Audio state vs control state consistency  
**Solution:** Update oscillators immediately when state changes

```cpp
void updateMusicalState() {
    calculateNewNotes();
    for (playing notes) {
        oscillator.SetFreq(newFreq);
    }
}
```

**Ensures:** Audio always reflects current musical state

---

## Error Handling & Robustness

### Sensor Failure Handling
```cpp
if (sensor.begin()) {
    tofAvailable = true;
    // Use sensor
} else {
    tofAvailable = false;
    // Continue without sensor
}
```
**Philosophy:** Graceful degradation over hard failure

### Range Constraints
- Octave: `constrain(value, 1, 8)`
- Distance: `constrain(value, 50, 300)`
- Volume: `0.0 to 0.5` range
**Philosophy:** Never generate out-of-range values

### Debouncing
- Hardware switches with DaisyDuino `Switch` class
- 1000ms timeout for reliable debouncing
**Philosophy:** Prevent switch noise at hardware level

---

## Musical Design Decisions

### Why Major Pentatonic as Default?
- **No dissonant intervals:** Any combination sounds consonant
- **Cultural familiarity:** Recognizable in many musical traditions  
- **Playability:** Encourages exploration without "wrong notes"

### Why 5 Buttons for Notes?
- **One-hand ergonomics:** Fingertip reach without repositioning
- **Complete phrase capability:** Can play melodic ideas
- **Not overwhelming:** Constraint encourages creativity

### Why Distance → Timbre?
- **Visual gesture:** Readable by audience
- **Continuous control:** Smooth expressive parameter
- **Physical metaphor:** "Squeeze" for brightness feels natural

### Why Latch Mode?
- **Drone capability:** Sustain notes while playing melody over them
- **Solo performance:** Build chord/texture, then play over it
- **Compositional tool:** Set and forget background elements

### Why Momentary Sharp/Flat?
- **Expressive ornaments:** Blue notes, grace notes, bends
- **Non-commitment:** Return to base pitch immediately
- **Performance gesture:** Visible technique

---

## Extension Points (Future Work)

### Immediate Possibilities
1. **Chord Modes:** Implement major/minor chord logic (infrastructure exists)
2. **MIDI Output:** Add MIDI note messages for external synths
3. **Additional Waveforms:** Square, sawtooth, or custom wavetables
4. **Envelope Control:** ADSR per note for more dynamic articulation

### Medium-Term Extensions
1. **Preset System:** Save/recall scale/octave/effect configurations
2. **Sequencer:** Record and loop button patterns
3. **Effects Chain:** Reverb, delay, filter after oscillator mix
4. **Scale Library:** User-definable scales (not just 3 presets)

### Architectural Considerations
1. **Configuration File:** Extract constants to `config.h`
2. **State Machine:** Formalize right-hand control as explicit FSM
3. **Function Extraction:** Break large functions into testable units
4. **Unit Tests:** Test musical logic (scales, mappings) in isolation

---

## Development Context

### Why PlatformIO?
- **Library management:** Easier than Arduino IDE
- **Version control:** Better integration than Arduino
- **Build system:** More control over compilation
- **Professional workflow:** IDE-agnostic toolchain

### Why DaisyDuino (Arduino Framework)?
- **Rapid prototyping:** Familiar Arduino API
- **Hardware abstraction:** DaisyDuino handles audio codec setup
- **Community resources:** Examples and libraries available
- **Good enough performance:** Audio quality is excellent

### Why Not Pure DSP?
- **Development speed:** Arduino framework faster to iterate
- **Project scope:** Not doing advanced DSP (just oscillators)
- **Maintainability:** Simpler code, easier to modify
- **Future:** Can always optimize specific parts later

---

## Performance Characteristics

### Latency
- **Control to audio:** < 1ms (audio callback runs continuously)
- **Button to sound:** Negligible (human perception ~10-20ms threshold)
- **Sensor to timbre:** < 50ms (20Hz poll rate)
**Result:** Feels immediate and responsive

### Polyphony
- **Maximum:** 5 simultaneous notes
- **CPU load:** Minimal (simple additive synthesis)
- **Headroom:** Volume scaled to prevent clipping
**Result:** Can play full chords without distortion

### Gesture Response
- **Distance range:** 250mm (50-300mm)
- **Hysteresis:** 5mm (prevents jitter)
- **Mapping resolution:** Continuous via `map()`
**Result:** Smooth, controllable timbral changes

---

## Summary

This is a **constraint-based, gesture-augmented musical interface** optimized for:

1. **Playability:** Scale-locked notes prevent "wrong" notes
2. **Expressiveness:** Gestural control adds performance dimension  
3. **Accessibility:** Simple button interface, no traditional technique required
4. **Audience Readability:** Visual gestures map directly to sonic results

The architecture balances:
- **Simplicity:** Minimal moving parts, easy to understand
- **Extensibility:** Clear extension points for future features
- **Performance:** Efficient enough for real-time audio
- **Maintainability:** Well-documented, organized code

**Design Philosophy:** "Complex behavior from simple, well-designed constraints"
