# NIME Controller - Feature Implementation Plan

**Date:** November 2025  
**Status:** Comprehensive feature mapping against brainstorming doc

---

## Executive Summary

The current implementation covers **core playability** (50% of intended feature set). The brainstorming document outlines an ambitious framework with visual/haptic feedback, advanced gesture control, and extended sensor capabilities.

**Missing feature categories:**
1. **Visual Feedback System** - RGB LED strip integration (NOT STARTED)
2. **Haptic Feedback** - Vibration motor for confirmations (NOT STARTED)
3. **Advanced Gesture Control** - IMU pitch bend and modulation (PARTIALLY IMPLEMENTED)
4. **Palm Sensor Context-Awareness** - Mode-dependent effects (NOT STARTED)
5. **Extended Chord Modes** - Full chord voicing (INFRASTRUCTURE ONLY)
6. **MIDI Output** - External synth control (NOT STARTED)

---

## Feature Comparison Matrix

### ✅ IMPLEMENTED FEATURES

| Feature | Status | Implementation Details |
|---------|--------|------------------------|
| **5 Left-Hand Buttons (Note Articulation)** | ✅ Complete | Scale degree mapping, envelope-based attack/release |
| **Octave Control (Index/Pinky Up/Down)** | ✅ Complete | Range 1-8, constrained |
| **Scale Selection (Major Pent / Blues / Chromatic)** | ✅ Complete | Via Thumb + Index/Middle/Ring |
| **Momentary Sharp/Flat** | ✅ Complete | Middle/Ring buttons, ±1 semitone |
| **Latch/Sustain Mode** | ✅ Complete | Toggle via Thumb + Pinky, re-trigger on re-press |
| **Key/Transposition Control** | ✅ Complete | Middle + Ring (key set mode), 12 semitone offsets |
| **Distance Sensor (ToF)** | ✅ Complete | VL53L0X, 50-300mm range, sine/triangle blend |
| **Basic Chord Modes** | ✅ Partial | Index + Middle / Index + Ring combos recognized, but chord voicing not fully implemented |
| **Audio Synthesis (Sine/Triangle)** | ✅ Complete | 48kHz, equal-power crossfade, polyphony up to 5 notes |
| **Envelope System** | ✅ Complete | 20ms attack, 150ms release, per-note independent |
| **Volume Control** | ✅ Complete | Potentiometer-based, A5 analog input |

---

### ❌ NOT IMPLEMENTED FEATURES

| Feature | Status | Priority | Effort | Details |
|---------|--------|----------|--------|---------|
| **RGB LED Strip (NeoPixels)** | ❌ Not Started | HIGH | Medium | Visual feedback for key, octave, mode. Requires WS2812B integration. |
| **Haptic Vibration Motor** | ❌ Not Started | HIGH | Low | Tactile confirmation. Requires PWM or GPIO. |
| **IMU/Accelerometer** | ❌ Disabled | MEDIUM | Medium | MSA301 present but I2C conflicts. Would enable pitch bend (tilt) and vibrato (roll). |
| **Mode-Aware Palm Effects** | ❌ Not Started | MEDIUM | Medium | Context-aware sensor function: distortion (single), strum (chord), filter/vibrato (latch). |
| **Full Chord Voicing** | ❌ Not Started | MEDIUM | Low | Infrastructure exists; needs chord data and oscillator scaling. |
| **MIDI Output** | ❌ Not Started | LOW | Medium | External synth control via USB/Serial MIDI. |
| **Preset System** | ❌ Not Started | LOW | High | Save/recall scale/octave/key configurations. |
| **Sequencer** | ❌ Not Started | LOW | High | Record and loop button patterns. |
| **Effects Chain** | ❌ Not Started | LOW | High | Reverb, delay, filter post-synthesis. |
| **User-Definable Scales** | ❌ Not Started | LOW | Medium | Beyond 3 built-in scales. |

---

## Detailed Implementation Plan

### Phase 1: Visual & Haptic Feedback (High Priority)

**Goal:** Implement the feedback system outlined in Section 5 of the brainstorming doc.

#### Task 1.1: RGB LED Strip Integration

**What:** Add NeoPixel (WS2812B) LED strip to right-hand controller

**Hardware Requirements:**
- 5-7 WS2812B RGB LEDs
- GPIO pin (recommended D20 or unused pin)
- 470Ω resistor on data line
- Adafruit_NeoPixel library

**Software Implementation:**

```cpp
// Add to constants
const int LED_PIN = D20;
const int NUM_LEDS = 7;
const int LED_BRIGHTNESS = 255;

// Color mapping for Circle of Fifths
const uint32_t LED_COLORS[12] = {
  // C, C#, D, D#, E, F, F#, G, G#, A, A#, B
  0xFF0000,  // C=Red
  0xFF4400,  // C#=Orange-Red
  0xFF8800,  // D=Orange
  0xFFCC00,  // D#=Gold
  0xFFFF00,  // E=Yellow
  0xCCFF00,  // F=Lime
  0x00FF00,  // F#=Green
  0x00FFCC,  // G=Cyan
  0x0088FF,  // G#=Sky Blue
  0x0000FF,  // A=Blue
  0x8800FF,  // A#=Violet
  0xFF00FF   // B=Magenta
};

// LED state management
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
void updateLEDDisplay() {
  // Key color on full strip
  uint32_t keyColor = LED_COLORS[currentKey];
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, keyColor);
  }
  
  // White marker for octave position
  int octaveMarker = 3 + (currentOctave - 4);  // Center at octave 4
  octaveMarker = constrain(octaveMarker, 0, NUM_LEDS - 1);
  strip.setPixelColor(octaveMarker, 0xFFFFFF);
  
  // Mode visualization
  if (currentMode == MODE_MAJOR_CHORD) {
    // Highlight 1st, 3rd, 5th LEDs
    strip.setPixelColor(0, 0xFFFFFF);
    strip.setPixelColor(2, 0xFFFFFF);
    strip.setPixelColor(4, 0xFFFFFF);
  } else if (currentMode == MODE_MINOR_CHORD) {
    // Highlight 1st, 3rd (flat), 5th
    strip.setPixelColor(0, 0xCCCCFF);
    strip.setPixelColor(2, 0xCCCCFF);
    strip.setPixelColor(4, 0xCCCCFF);
  }
  
  strip.show();
}

void confirmationAnimation() {
  // White wipe left-to-right
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0xFFFFFF);
    strip.show();
    delay(50);
  }
  // Return to normal
  updateLEDDisplay();
}
```

**Integration Points:**
- Call `updateLEDDisplay()` in main loop after state changes
- Call `confirmationAnimation()` when scale/key/mode changes
- Call `strip.begin()` in `setup()`

**Estimated Effort:** 4-6 hours
**Dependencies:** Adafruit_NeoPixel library

---

#### Task 1.2: Vibration Motor Integration

**What:** Add haptic feedback via small vibration motor

**Hardware Requirements:**
- 5V vibration motor (coin cell type)
- GPIO pin for PWM control (D21)
- N-channel MOSFET for power switching (2N7000 or similar)
- 1N4148 diode for back-EMF protection

**Software Implementation:**

```cpp
const int VIBRATION_PIN = D21;
const int VIBRATION_DURATION = 50;  // ms
const int VIBRATION_INTENSITY = 200; // 0-255 PWM

void hapticConfirm(int duration = VIBRATION_DURATION, int intensity = VIBRATION_INTENSITY) {
  analogWrite(VIBRATION_PIN, intensity);
  delay(duration);
  analogWrite(VIBRATION_PIN, 0);
}

void hapticPattern_ScaleChange() {
  hapticConfirm(50, 255);
  delay(100);
  hapticConfirm(50, 255);
}

void hapticPattern_KeyChange() {
  hapticConfirm(100, 200);
}

void hapticPattern_LatchToggle() {
  hapticConfirm(30, 200);
  delay(30);
  hapticConfirm(30, 200);
}
```

**Integration Points:**
- Call `hapticConfirm()` on scale changes (Thumb + Index/Middle/Ring)
- Call `hapticPattern_KeyChange()` on key set mode confirmation
- Call `hapticPattern_LatchToggle()` on latch mode toggle
- Set `pinMode(VIBRATION_PIN, OUTPUT)` in `setup()`

**Estimated Effort:** 2-3 hours
**Dependencies:** None (uses standard Arduino PWM)

---

### Phase 2: Gesture Control Enhancement (Medium Priority)

**Goal:** Unlock IMU capabilities and implement mode-aware palm sensor effects

#### Task 2.1: Re-enable MSA301 Accelerometer (I2C Bus Resolution)

**Problem:** MSA301 and VL53L0X conflict on I2C bus

**Solutions (pick one):**

**Option A: Separate I2C Buses**
- MSA301 → I2C0 (alternate pins)
- VL53L0X → I2C1 (current: D11/D12)
- Requires remapping sensor pins

**Option B: Multiplexed Address**
- Add I2C multiplexer (TCA9548A)
- Route both sensors through mux on single bus
- More complex but cleaner

**Option C: Disable VL53L0X, use MSA301 only**
- Remove distance sensor functionality
- Simplify to just IMU
- **Not recommended** (loses current timbral control)

**Recommendation:** Use Option A (separate I2C buses) for simplicity

**Implementation:**

```cpp
// Update I2C1 assignment
// Wire1.begin(); // MSA301 on I2C1 (new)
// Wire.begin();  // VL53L0X on I2C0 (existing)

// MSA301 initialization
#include <MSA301.h>
MSA301 accel;

void setup() {
  // ... existing code ...
  
  // VL53L0X on I2C0 (Wire)
  Wire.begin(26, 25);  // SDA=D26, SCL=D25 (remapped pins)
  
  // MSA301 on I2C1 (Wire1)
  Wire1.begin(11, 12); // SDA=D11, SCL=D12 (freed from VL53L0X)
  
  if (!accel.begin(0x26, &Wire1)) {
    Serial.println("MSA301 not found!");
    imuAvailable = false;
  }
}

void processIMU() {
  if (!imuAvailable) return;
  
  // Read accelerometer data
  accel.readData();
  float pitchAxis = accel.getAccelX();  // Tilt for pitch bend
  float rollAxis = accel.getAccelZ();   // Twist for modulation
  
  // Pitch bend (MIDI CC#1 equivalent)
  // Map pitch axis to ±2 semitone bend (±0.5 semitone per unit)
  float pitchBendAmount = constrain(pitchAxis / 8.0f, -1.0f, 1.0f);
  pitchBendMIDI = 64 + (pitchBendAmount * 16);  // Center at 64
  
  // Modulation/Vibrato (MIDI CC#1)
  // Map roll axis to modulation depth (0-127)
  float modulationAmount = constrain(rollAxis / 16.0f, 0.0f, 1.0f);
  modulationMIDI = modulationAmount * 127;
  
  // Apply to all active notes
  for (int i = 0; i < 5; i++) {
    if (envelopes[i].isActive) {
      // Apply pitch bend by adjusting oscillator frequency
      float bendedFreq = mtof(currentScaleNotes[i]) * pow(2.0f, pitchBendAmount / 12.0f);
      oscSine[i].SetFreq(bendedFreq);
      oscTri[i].SetFreq(bendedFreq);
      
      // Apply modulation (simplified: depth to vibrato amount)
      // Full implementation would add vibrato LFO
    }
  }
}
```

**Integration Points:**
- Add `imuAvailable` flag
- Call `processIMU()` in main loop
- Update `AudioCallback()` to use pitch-bent frequencies

**Estimated Effort:** 6-8 hours (includes I2C remapping, testing)
**Dependencies:** MSA301 library, pin remapping

---

#### Task 2.2: Mode-Aware Palm Sensor Effects

**What:** Make distance sensor function depend on current mode

**Implementation:**

```cpp
void processPalmSensor() {
  if (!tofAvailable) return;
  
  int distance = readToFSensor();
  float blend = map(distance, 50, 300, 0.0f, 1.0f);
  blend = constrain(blend, 0.0f, 1.0f);
  
  // Effect changes based on current mode
  if (currentMode == MODE_SINGLE_NOTE) {
    // Single Note: Distortion/Drive (via waveform blend + triangle boost)
    waveformBlend = blend;
    // Could add drive/saturation parameter later
    Serial.print("Distortion: ");
  } else if (currentMode == MODE_MAJOR_CHORD || currentMode == MODE_MINOR_CHORD) {
    // Chord Mode: Strum/Arpeggiator speed
    // !! Note: Requires arpeggiator implementation
    float strumSpeed = map(blend, 0.0f, 1.0f, 0.1f, 2.0f);  // Notes per second
    // Serial.print("Strum Speed: "); Serial.println(strumSpeed);
  } else if (latchMode) {
    // Latch Mode: Filter sweep or vibrato depth
    float filterOrVibrato = blend;
    // Could control filter cutoff or vibrato depth
    // Serial.print("Filter/Vibrato: ");
  }
  
  Serial.print(distance);
  Serial.println(" mm");
}
```

**Requires:**
- Arpeggiator implementation (for chord strum effect)
- Filter integration (for filter sweep)
- Vibrato LFO (for vibrato effect)

**Estimated Effort:** 8-12 hours (complex feature)

---

### Phase 3: Chord System Completion (Medium Priority)

**Goal:** Implement full chord voicing instead of placeholder mode recognition

#### Task 3.1: Chord Voicing Database

**What:** Define major and minor chord intervals for all keys

```cpp
// Chord intervals (semitones from root)
const int majorChordIntervals[3] = {0, 4, 7};     // Root, Major 3rd, Perfect 5th
const int minorChordIntervals[3] = {0, 3, 7};     // Root, Minor 3rd, Perfect 5th
const int seventhIntervals[4] = {0, 4, 7, 10};    // For future: Maj7 chords
const int ninthIntervals[5] = {0, 4, 7, 10, 14};  // For future: Maj9 chords

void triggerChord(bool isMajor) {
  if (currentMode != MODE_MAJOR_CHORD && currentMode != MODE_MINOR_CHORD) {
    return;
  }
  
  const int* intervals = isMajor ? majorChordIntervals : minorChordIntervals;
  int numNotes = 3;
  
  // Play root + third + fifth based on button presses
  for (int buttonIdx = 0; buttonIdx < 5; buttonIdx++) {
    if (leftButtonStates[buttonIdx]) {
      // Calculate root note for this button's scale degree
      int rootNote = currentScaleNotes[buttonIdx];
      
      // Play chord tones: root, third, fifth
      for (int i = 0; i < numNotes; i++) {
        int chordNote = rootNote + intervals[i];
        // Trigger oscillator for this chord tone
        // (Would require expanding oscillator array or multiplexing)
      }
    }
  }
}
```

**Challenge:** Current oscillator array (5 total) insufficient for 3+ simultaneous chords.
**Solution:** Either limit to 1 chord at a time OR expand oscillator array to 15+ oscillators

**Estimated Effort:** 4-6 hours (assuming 1 chord at a time)

---

### Phase 4: MIDI Output (Low Priority)

**Goal:** Enable control of external synthesizers

#### Task 4.1: USB MIDI Interface

**What:** Send note on/off and CC messages via USB

**Implementation (leverages Daisy Seed USB device capability):**

```cpp
#include <USB.h>  // Daisy Seed USB support

// Configure USB MIDI
void setupUSBMIDI() {
  // USB configured in setup() via DaisyDuino framework
}

void sendMIDINoteOn(int channel, int note, int velocity) {
  // Send via USB MIDI
  usbMIDI.sendNoteOn(note, velocity, channel);
}

void sendMIDINoteOff(int channel, int note, int velocity) {
  usbMIDI.sendNoteOff(note, velocity, channel);
}

void sendMIDICC(int channel, int cc, int value) {
  usbMIDI.sendControlChange(cc, value, channel);
}
```

**Integration Points:**
- When left button pressed: `sendMIDINoteOn(0, midiNote, 100)`
- When left button released: `sendMIDINoteOff(0, midiNote, 0)`
- When octave changes: Send CC#48 (NRPN) with octave value
- When scale changes: Send CC#32 with scale index

**Estimated Effort:** 3-4 hours

---

### Phase 5: Advanced Features (Low Priority)

#### Task 5.1: Preset System

**Hardware:** Use QSPI flash (available on Daisy Seed, 64MB)

```cpp
struct Preset {
  int octave;
  int key;
  int scale;
  int mode;
  bool latchEnabled;
};

const int MAX_PRESETS = 10;
Preset presets[MAX_PRESETS];

void savePreset(int slotNumber) {
  // Write to flash at offset
  presets[slotNumber] = {currentOctave, currentKey, currentScale, currentMode, latchMode};
  // Commit to flash (requires QSPI library)
}

void loadPreset(int slotNumber) {
  currentOctave = presets[slotNumber].octave;
  currentKey = presets[slotNumber].key;
  currentScale = presets[slotNumber].scale;
  currentMode = presets[slotNumber].mode;
  latchMode = presets[slotNumber].latchEnabled;
  updateLEDDisplay();
}
```

**Estimated Effort:** 6-8 hours (includes flash management)

---

#### Task 5.2: Arpeggiator/Sequencer

**Simple Version:** Play latched notes in sequence

```cpp
struct SequenceStep {
  int buttonIndex;
  unsigned long duration;  // ms
};

unsigned long lastStepTime = 0;
int currentStepIndex = 0;

void updateArpeggiator() {
  if (!latchMode) return;
  
  unsigned long now = millis();
  if (now - lastStepTime > arpeggioSpeed) {
    lastStepTime = now;
    
    // Count active notes
    vector<int> activeButtons;
    for (int i = 0; i < 5; i++) {
      if (leftButtonStates[i]) activeButtons.push_back(i);
    }
    
    if (activeButtons.empty()) return;
    
    // Rotate through active notes
    currentStepIndex = (currentStepIndex + 1) % activeButtons.size();
    // Trigger envelope re-start for current step
  }
}
```

**Estimated Effort:** 8-10 hours

---

#### Task 5.3: Filter Integration

**Option A: Software IIR Filter (Butterworth)**
```cpp
// Low-pass filter for subtractive synthesis
float filterOutput = applyButterworthLPF(oscillatorMix, cutoffFrequency);
```

**Option B: Hardware Filter (if analog filter available)**
- Requires additional analog hardware
- Could use op-amp based SVF (State Variable Filter)

**Estimated Effort:** 10-15 hours (Option A)

---

## Summary Timeline & Prioritization (Revised)

### **PHASE 1: Musical Feature Parity (Priority: CRITICAL)**

**Goal:** Implement all core musical features from brainstorming doc before visual/haptic feedback

#### 1.1 Chord Voicing Implementation (4-6 hrs)
- Implement full major/minor chord note generation
- Map left-hand buttons to chord tones (root + intervals)
- Test all 12 keys with both chord types
- **Blocker Status:** None - can start immediately

#### 1.2 I2C Bus Resolution & MSA301 Re-enablement (2-3 hrs)

**Current Status:** MSA301 is disabled (line 397-399 in main.cpp) due to I2C bus conflicts. Both sensors fighting on Wire (I2C0).

**Solution:** Assign MSA301 to Wire1 (I2C1), keep VL53L0X on Wire (I2C0)
- Daisy Seed has two I2C buses available
- VL53L0X: Wire (I2C0) with SDA=D11, SCL=D12 (current setup)
- MSA301 → Wire1 (I2C1) with SDA=D26, SCL=D25
- Update MSA301 initialization code to use Wire1

**Code Change:**
```cpp
Wire.begin(11, 12);        // VL53L0X on I2C0 (keep existing)
Wire1.begin(26, 25);       // MSA301 on I2C1 (new)
accel.begin(0x26, &Wire1); // Pass Wire1 to library
```

- **Blocker Status:** Blocks IMU features
- **Note:** Much simpler than anticipated - just pin assignment and library call update

#### 1.3 IMU Integration - Pitch Bend & Modulation (4-6 hrs)
- Re-enable MSA301 accelerometer reading (now that I2C resolved)
- Implement pitch bend from tilt (X-axis): ±2 semitones
- Implement modulation from roll (Z-axis): vibrato/tremolo
- Map to all active notes in real-time
- **Blocker Status:** Depends on 1.2

#### 1.4 Mode-Aware Palm Sensor Effects (8-12 hrs)
- **Single Note Mode:** Distance → distortion/drive (triangle boost)
- **Chord Mode:** Distance → strum/arpeggiator speed
- **Latch Mode:** Distance → filter sweep or vibrato depth
- Smooth transitions between modes
- **Blocker Status:** Depends on 1.1, 1.3

**Subtotal: 18-28 hours** (reduced from 22-32 due to simpler I2C resolution)
**Target Completion:** Achieve feature parity with brainstorming doc (musical features complete)

---

### **PHASE 2: Visual & Haptic Feedback (Priority: HIGH)**

#### 2.1 RGB LED Strip Integration (4-6 hrs)
- NeoPixel strip for key color + octave position
- Mode visualization (chord patterns)
- State change confirmation animations

#### 2.2 Vibration Motor Integration (2-3 hrs)
- Haptic confirmation on state changes
- Pattern feedback for different modes

**Subtotal: 6-9 hours**

---

### **PHASE 3: Extended Features (Priority: MEDIUM)**

#### 3.1 MIDI Output (3-4 hrs)
#### 3.2 Preset System (6-8 hrs)
#### 3.3 Arpeggiator (8-10 hrs)
#### 3.4 Filter Integration (10-15 hrs)

**Subtotal: 27-37 hours**

---

### **PHASE 4: Polish & Optimization (Priority: LOW)**

- User-definable scales
- Advanced sequencer
- Effects chain (reverb, delay)
- Performance optimization

---

## Dependency Graph (Critical Path in Bold)

```
**PHASE 1: MUSICAL PARITY (Critical Path)**
├─ **1.1: Chord Voicing** (independent)
├─ **1.2: I2C Bus Resolution**
│  └─ **1.3: IMU Integration** (pitch bend + modulation)
│     └─ **1.4: Mode-Aware Sensor Effects**
│        ├─ Chord Mode → Arpeggiator (Phase 3)
│        └─ Latch Mode → Filter (Phase 3)
└─ **1.4: Mode-Aware Sensor Effects**
   ├─ Single Note Mode → Distortion (done via triangle boost)
   ├─ Chord Mode → Strum Speed Control
   └─ Latch Mode → Filter/Vibrato Control

PHASE 2: VISUAL/HAPTIC (After Musical Parity)
├─ RGB LED Strip (independent)
└─ Vibration Motor (independent)

PHASE 3: EXTENDED (Can start after Phase 1)
├─ MIDI Output (independent)
├─ Arpeggiator (depends on 1.4)
├─ Filter (depends on 1.4)
└─ Preset System (independent)

PHASE 4: POLISH
├─ User-definable scales
├─ Advanced sequencer
└─ Effects chain
```

---

## Recommendation

**CRITICAL PATH: Complete Phase 1 (Musical Feature Parity) FIRST**

This ensures the instrument matches the brainstorming doc's full musical capabilities before adding visual polish:

1. **Start with 1.1 (Chord Voicing)** - 4-6 hrs
   - Straightforward, no dependencies
   - Unlocks mode switching functionality

2. **Then 1.2 (I2C Bus Resolution)** - 2-3 hrs
   - Move MSA301 to Wire1 (separate from VL53L0X)
   - Simple pin remapping + library update
   - Much easier than anticipated!

3. **Then 1.3 (IMU Integration)** - 4-6 hrs
   - Pitch bend (tilt) implementation
   - Modulation (roll) implementation
   - Integrate with all active notes

4. **Finally 1.4 (Mode-Aware Effects)** - 8-12 hrs
   - Context-dependent palm sensor behavior
   - Most complex but completes the musical vision

**After Phase 1 is complete:**
- Move to Phase 2 (visual/haptic) for polished user experience
- Phase 3 (extended features) can be done in parallel with Phase 2

**Estimated Total for Musical Parity: 18-28 hours**

---

## Risk Assessment

| Task | Risk | Mitigation |
|------|------|-----------|
| RGB LED Integration | Library conflicts, GPIO availability | Test with breadboard first, check DaisyDuino conflicts |
| I2C Bus Resolution | Pin conflicts, sensor communication | Document pin mapping, test with oscilloscope |
| IMU Integration | Calibration issues, sensor drift | Implement calibration gesture, use filtering |
| Chord Voicing | Oscillator/memory limitations | Start with 1 chord at time, consider oscillator expansion |
| MIDI Output | USB driver issues, latency | Test with popular DAW (Ableton, Logic) |
| Flash Writes | Data corruption, wear leveling | Implement CRC checks, limit writes |

---

## Testing Strategy

**Unit-level:**
- Test LED color mapping for all 12 keys
- Test haptic patterns trigger correctly
- Test IMU reading and pitch bend conversion
- Test chord interval calculations

**Integration-level:**
- Play scales through all modes with LED feedback
- Verify haptic confirms state changes
- Test IMU gesture while playing notes
- Verify no audio glitches during state transitions

**Full-system:**
- Perform with all features active
- Test edge cases (rapid mode switches, etc.)
- Verify no latency increase
- Confirm power budget (LEDs, motor, IMU combined)

---

## Conclusion

The current implementation is **musically functional but visually/haptically minimal**. The brainstorming document's vision requires ~60-80 additional hours of development focused on:

1. **Immediate visual/haptic presence** (LEDs, vibration)
2. **Gesture expansion** (IMU pitch bend, modulation)
3. **Chord completion** (full voicing)
4. **External control** (MIDI output)

**Critical path:** RGB LEDs → I2C resolution → IMU integration → mode-aware effects

All other features can be implemented in parallel without blocking core functionality.
