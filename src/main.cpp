/**
 * NIME Two-Handed Musical Controller
 * 
 * Hardware: Electrosmith Daisy Seed
 * Framework: Arduino (via DaisyDuino)
 * 
 * Left Hand (Note Articulation):
 *   - 5 buttons for scale degrees (D8-D12)
 *   - VL53L0X ToF sensor for waveform morphing (I2C1: D11=SDA, D12=SCL)
 * 
 * Right Hand (Modifiers):
 *   - 5 buttons for control (D15-D19)
 *   - Thumb = SHIFT key for combinations
 * 
 * Additional:
 *   - Volume pot on A5
 *   - Audio output: 48kHz stereo
 */

#include "DaisyDuino.h"
#include <Adafruit_VL53L0X.h>
#include <Adafruit_MSA301.h>
#include <Wire.h>

// Create separate I2C bus for MSA301 (I2C4 on D13/D14)
// TwoWire constructor: TwoWire(SDA_PIN, SCL_PIN)
// Note: STM32 pin names used as per DaisyDuino convention
TwoWire Wire1(PB_9, PB_8);  // I2C4: SDA=D14 (PB_9), SCL=D13 (PB_8)

DaisyHardware hw;
Oscillator oscSine[5];  // Sine oscillators for each button
Oscillator oscTri[5];   // Triangle oscillators for each button

// Envelope System
const float ATTACK_TIME = 0.05f;   // 50ms attack for gentler onset (prevents clipping)
const float RELEASE_TIME = 0.15f;  // 150ms release for smooth fade
const float ATTACK_TIME_ARP = 0.01f;   // 10ms attack for arpeggio (fast, minimal click)
const float RELEASE_TIME_ARP = 0.03f;  // 30ms release for arpeggio (quick cutoff)
struct NoteEnvelope {
  float level;              // Current envelope amplitude (0.0 to 1.0)
  bool isActive;            // Note is playing
  bool isReleasing;         // In release phase
  unsigned long attackStartTime;
  unsigned long releaseStartTime;
};
NoteEnvelope envelopes[5];

// Chord tone tracking (for multi-note chord playback)
struct ChordNote {
  int midiNote;             // MIDI note to play
  int sineOscIdx;           // Which sine oscillator (0-4)
  int triOscIdx;            // Which tri oscillator (0-4)
  int buttonIndex;          // Which left-hand button triggered this chord (for release matching)
  bool isActive;            // Currently playing
};
const int MAX_CHORD_NOTES = 15;  // 5 buttons × 3 notes each
ChordNote chordNotes[MAX_CHORD_NOTES];
int numActiveChordNotes = 0;

// Volume Control
const int VOLUME_PIN = A5;
const int VOLUME_CHANGE_THRESHOLD = 10;  // ADC counts hysteresis to reduce jitter
const float VOLUME_SCALE = 0.5f;         // Maximum volume (0.0 to 1.0)
int lastVolumeRaw = -1;

//////////////
// Left hand
//////////////

// Distance Sensor (VL53L0X Time-of-Flight)
Adafruit_VL53L0X sensor = Adafruit_VL53L0X();

// Accelerometer (MSA311 3-axis) - I2C address 0x62
Adafruit_MSA311 accel = Adafruit_MSA311();
bool accelAvailable = false;

// IMU calibration parameters
float accelCenterX = 0.0f;              // Calibrated center X acceleration (for pitch window)
float accelCenterZ = 0.0f;              // Calibrated center Z acceleration (unused, kept for symmetry)

// Distance sensor parameters
const int DISTANCE_CHANGE_THRESHOLD = 5;      // Minimum change in mm to process
const int DISTANCE_MIN = 50;                  // Minimum distance for mapping (mm)
const int DISTANCE_MAX = 300;                 // Maximum distance for mapping (mm)
const unsigned long SENSOR_INTERVAL = 50;     // Poll interval in ms
int lastDistance = -1;
unsigned long lastSensorRead = 0;
bool tofAvailable = false;

// Sliding Window (Accelerometer-based note selection via tilt)
// Offset is tracked in SEMITONES (for consistent range across all scales)
// But snaps to scale degrees when playing notes (locks to current scale)
const int WINDOW_SIZE = 5;                    // Number of notes in window
int windowOffsetSemitones = 0;                // Current offset in SEMITONES (not scale degrees)
int windowOffsetDegrees = 0;                  // Display/UI only: nearest scale degree to current offset
const int MAX_WINDOW_OFFSET_DOWN = 36;        // -36 semitones (3 octaves down to C0)
const int MAX_WINDOW_OFFSET_UP = 24;          // +24 semitones (2 octaves up, capped for brightness)
float accelPositionOffset = 0.0f;             // Integrated position from center
float lastAccelX = 0.0f;                      // Previous X acceleration
float smoothedAccelX = 0.0f;                  // Low-pass filtered accelX
unsigned long lastAccelRead = 0;
const unsigned long ACCEL_INTERVAL = 20;      // 50Hz polling
const float WINDOW_SENSITIVITY = 0.02f;      // Semitones per G deviation (balanced range of motion)
const float WINDOW_DEADZONE = 20.0f;          // Minimum acceleration deviation to activate window (in G)
const float ACCEL_FILTER_ALPHA = 0.25f;       // Low-pass filter coefficient (0.0-1.0, higher = more responsive)

// Calibration
unsigned long calibrationStartTime = 0;
const unsigned long CALIBRATION_HOLD_TIME = 2000;  // 2 seconds to calibrate
bool isCalibrating = false;

// Left Hand Buttons (Note Articulation)
const int NUM_LEFT_BUTTONS = 5;
Switch leftButton[NUM_LEFT_BUTTONS];
const int leftButtonPins[NUM_LEFT_BUTTONS] = {6, 7, 8, 9, 10};
bool leftButtonStates[NUM_LEFT_BUTTONS] = {false};      // Logical note states (can be latched)
bool leftButtonPrevStates[NUM_LEFT_BUTTONS] = {false};  // Previous physical button states

// Oscillator ref counting to prevent envelope conflicts from button overlaps
int oscRefCount[NUM_LEFT_BUTTONS] = {0};  // Track how many notes are using each oscillator

///////////////
// Right hand
///////////////

// Right Hand Buttons (Modifiers & Control)
const int NUM_RIGHT_BUTTONS = 5;
Switch rightButton[NUM_RIGHT_BUTTONS];
const int rightButtonPins[NUM_RIGHT_BUTTONS] = {15, 16, 17, 18, 19};  // D15-D19
bool rightButtonStates[NUM_RIGHT_BUTTONS] = {false};
bool rightButtonPrevStates[NUM_RIGHT_BUTTONS] = {false};

// Right Hand Button Mapping (array indices)
enum RightHandButtons {
  RIGHT_PINKY = 0,    // Pitch window mode (D15) - tilt left/right to slide through scale
  RIGHT_RING = 1,     // Momentary flat (D16)
  RIGHT_MIDDLE = 2,   // Momentary sharp (D17)
  RIGHT_INDEX = 3,    // Calibrate (D18) - hold with pinky for 2s
  RIGHT_THUMB = 4     // SHIFT key (D19)
};

//////////////////////
// Musical Structure
/////////////////////
// Audio Parameters
float volume = 0.3f;                // Global volume (0.0 to 1.0)
float waveformBlend = 0.0f;         // Blend position (0.0 = sine, 1.0 = triangle)
float sineAmp = 1.0f;               // Sine wave amplitude (equal-power crossfade)
float triAmp = 0.0f;                // Triangle wave amplitude (equal-power crossfade)
float triBoost = 1.0f;              // Boost triangle amplitude for more dramatic morph

// Scale & Key Settings
const int OCTAVE_MIN = 1;
const int OCTAVE_MAX = 8;
int currentOctave = 4;                  // Start in middle octave (MIDI note 60 = C4)
int currentKey = 0;                     // Root note offset (0 = C)
int pitchOffset = 0;                    // Momentary sharp/flat in semitones

enum ScaleType {
  SCALE_MAJOR_PENTATONIC = 0,
  SCALE_MINOR_PENTATONIC = 1,
  SCALE_CHROMATIC = 2
};
int currentScale = SCALE_MAJOR_PENTATONIC;

// Scale intervals (semitones from root, mapped to 5 buttons)
const int majorPentatonic[] = {0, 2, 4, 7, 9};    // C, D, E, G, A
const int minorPentatonic[] = {0, 3, 5, 7, 10};   // C, Eb, F, G, Bb
const int chromaticScale[] = {0, 1, 2, 3, 4};     // C, C#, D, D#, E (chromatic uses raw semitone offsets, not scale degrees)

int currentScaleNotes[NUM_LEFT_BUTTONS];          // Current MIDI note numbers

/////////////////////
// Additional setup
////////////////////
/**
 * Scan I2C bus for connected devices
 * Useful for debugging sensor connections
 */
void i2cScan() {
  Serial.println("I2C scan starting...");
  byte count = 0;
  
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  I2C device found at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      count++;
      delay(1);
    }
  }
  
  if (count == 0) {
    Serial.println("  No I2C devices found");
  } else {
    Serial.print("  Total devices found: ");
    Serial.println(count);
  }
}

// Play Modes
enum PlayMode {
  MODE_SINGLE_NOTE = 0,     // Individual note per button
  MODE_CHORD = 1,           // Triad chords (major/minor determined by scale)
  MODE_ARPEGGIO = 2         // Auto-cycle through triad notes when button held
};
int currentMode = MODE_SINGLE_NOTE;
// Latch mode disabled - use MODE_CHORD and MODE_ARPEGGIO instead
bool suboctaveEnabled = false;      // When true, add lower octave doubling for thicker sound

// Chord voicing intervals (semitones from root)
const int majorChordIntervals[3] = {0, 4, 7};     // Root, Major 3rd, Perfect 5th
const int minorChordIntervals[3] = {0, 3, 7};     // Root, Minor 3rd, Perfect 5th
const int CHORD_NOTES_PER_VOICE = 3;              // Notes per chord (root, 3rd, 5th)

// Chord state tracking (which chord voices are currently active)
struct ChordVoice {
  int midiNote;           // MIDI note number being played
  int oscillatorIndex;    // Which oscillator pair is handling this note (0-4)
  bool isActive;          // Currently playing
};
ChordVoice chordVoices[5 * CHORD_NOTES_PER_VOICE];  // Max 5 buttons × 3 notes each
int numActiveChordVoices = 0;

// Arpeggio state (for MODE_ARPEGGIO)
unsigned long lastArpStepTime = 0;
const unsigned long ARP_INTERVAL_MS = 175;  // Time between arpeggio notes (ms) - adjust by ear
int currentArpNoteIndex = 0;                // Which note in the triad (0=root, 1=3rd, 2=5th)
int currentArpButton = -1;                  // Which button is currently arpeggiated (-1 = none)

// Forward declarations
void releaseNote(int noteIndex);
int scaleDegreesToSemitones(int degrees);
int semitoneOffsetToNearestScaleDegree(int semitoneOffset);

/**
 * Window shifting helper functions
 * Range: C0 (down) to A5 (up)
 */
int clampWindowSemitones(int semitones) {
  return constrain(semitones, -MAX_WINDOW_OFFSET_DOWN, MAX_WINDOW_OFFSET_UP);
}

void setWindowByDegrees(int degrees) {
  windowOffsetDegrees = degrees;
  windowOffsetSemitones = clampWindowSemitones(scaleDegreesToSemitones(windowOffsetDegrees));
  Serial.print("Window offset: ");
  Serial.print(windowOffsetDegrees);
  Serial.print(" degrees (");
  Serial.print(windowOffsetSemitones);
  Serial.println(" semitones)");
}

void setWindowBySemitones(int semitones) {
  windowOffsetSemitones = clampWindowSemitones(semitones);
  windowOffsetDegrees = semitoneOffsetToNearestScaleDegree(windowOffsetSemitones);
  Serial.print("Window offset: ");
  Serial.print(windowOffsetSemitones);
  Serial.print(" semitones (");
  Serial.print(windowOffsetDegrees);
  Serial.println(" degrees)");
}

void clearAllNotes() {
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    leftButtonStates[i] = false;
    releaseNote(i);  // Trigger envelope release for smooth fade-out
  }
  Serial.println("All notes cleared");
}

/**
 * Update the current scale notes based on octave, key, and scale type
 * Calculates MIDI note numbers for each of the 5 buttons
 * Window offset (in scale degrees) is applied dynamically when playing notes
 */
void updateScaleNotes() {
  int baseNote = (currentOctave * 12) + currentKey;

  switch (currentScale) {
    case SCALE_MAJOR_PENTATONIC:
      for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
        currentScaleNotes[i] = baseNote + majorPentatonic[i];
      }
      break;
    case SCALE_MINOR_PENTATONIC:
      for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
        currentScaleNotes[i] = baseNote + minorPentatonic[i];
      }
      break;
    case SCALE_CHROMATIC:
      for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
        currentScaleNotes[i] = baseNote + chromaticScale[i];
      }
      break;
  }
};

/**
 * Convert scale degree offset to semitone offset
 * Maps degree indices to scale positions (handles octave wrapping)
 * Example: Major Pentatonic {0, 2, 4, 7, 9}
 *   scaleDegreesToSemitones(1) = 2 (degree 1 → position 1 → 2 semitones)
 *   scaleDegreesToSemitones(5) = 12 (degree 5 → octave 1 + position 0 → 12 semitones)
 */
int scaleDegreesToSemitones(int degrees) {
  if (degrees == 0) return 0;
  
  const int* scaleIntervals = nullptr;
  int scaleLength = 5;
  
  switch (currentScale) {
    case SCALE_MAJOR_PENTATONIC:
      scaleIntervals = majorPentatonic;
      break;
    case SCALE_MINOR_PENTATONIC:
      scaleIntervals = minorPentatonic;
      break;
    case SCALE_CHROMATIC:
      scaleIntervals = chromaticScale;
      break;
  }
  
  int direction = (degrees > 0) ? 1 : -1;
  int absDegrees = abs(degrees);
  
  // Calculate which octave and position within octave
  int octaves = absDegrees / scaleLength;
  int posInOctave = absDegrees % scaleLength;
  
  // Semitones = octave offset (12 per octave) + scale position
  int semitones = (octaves * 12) + scaleIntervals[posInOctave];
  
  return semitones * direction;
}

/*
 * Convert a semitone offset to the nearest scale degree
 * (inverse of scaleDegreesToSemitones)
 * Searches all degrees to find the true nearest (no early break, octave wrapping is non-monotonic)
 */
int semitoneOffsetToNearestScaleDegree(int semitoneOffset) {
  if (semitoneOffset == 0) return 0;
  
  int direction = (semitoneOffset > 0) ? 1 : -1;
  int targetSemitones = abs(semitoneOffset);
  int bestDegree = 0;
  int bestDistance = abs(scaleDegreesToSemitones(0) - targetSemitones);
  
  // Search all possible degrees (no early break - octave wrapping creates non-monotonic distance)
  int maxSearchSemitones = (semitoneOffset > 0) ? MAX_WINDOW_OFFSET_UP : MAX_WINDOW_OFFSET_DOWN;
  for (int d = 1; d <= maxSearchSemitones; d++) {
    int semitones = abs(scaleDegreesToSemitones(d));
    int distance = abs(semitones - targetSemitones);
    
    if (distance < bestDistance) {
      bestDistance = distance;
      bestDegree = d;
    }
  }
  
  return bestDegree * direction;
}

/**
 * Print current sliding window information
 */
void printWindow() {
  Serial.print("Window: ");
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    Serial.print(currentScaleNotes[i] + windowOffsetSemitones);
    if (i < NUM_LEFT_BUTTONS - 1) Serial.print(", ");
  }
  Serial.print(" (offset: ");
  Serial.print(windowOffsetSemitones);
  Serial.print(" semitones");
  if (windowOffsetDegrees != 0) {
    Serial.print(" / degree ");
    Serial.print(windowOffsetDegrees);
  }
  Serial.println(")");
}

/**
 * Soft clipping function to prevent harsh distortion
 * Uses tanh for smooth saturation
 */
float softClip(float sample) {
  return tanhf(sample * 1.5f) / 1.5f;  // Gentle saturation
}

/**
 * Process envelope for a note (Attack/Release)
 * Returns current envelope level (0.0 to 1.0)
 */
float processEnvelope(int noteIndex) {
  NoteEnvelope &env = envelopes[noteIndex];
  
  if (!env.isActive) {
    return 0.0f;
  }
  
  unsigned long currentTime = millis();
  
  // Use faster envelopes for arpeggio mode to prevent popping
  float attackTime = (currentMode == MODE_ARPEGGIO) ? ATTACK_TIME_ARP : ATTACK_TIME;
  float releaseTime = (currentMode == MODE_ARPEGGIO) ? RELEASE_TIME_ARP : RELEASE_TIME;
  
  if (env.isReleasing) {
    // Release phase
    float elapsed = (currentTime - env.releaseStartTime) / 1000.0f;
    if (elapsed >= releaseTime) {
      env.isActive = false;
      env.level = 0.0f;
      return 0.0f;
    }
    env.level = 1.0f - (elapsed / releaseTime);
  } else {
    // Attack phase
    float elapsed = (currentTime - env.attackStartTime) / 1000.0f;
    if (elapsed >= attackTime) {
      env.level = 1.0f;
    } else {
      env.level = elapsed / attackTime;
    }
  }
  
  return env.level;
}

/**
 * Trigger envelope attack for a note
 */
void triggerNote(int noteIndex) {
  NoteEnvelope &env = envelopes[noteIndex];
  env.isActive = true;
  env.isReleasing = false;
  env.attackStartTime = millis();
  env.level = 0.0f;
}

/**
 * Release a note (start release phase)
 */
void releaseNote(int noteIndex) {
  NoteEnvelope &env = envelopes[noteIndex];
  if (env.isActive && !env.isReleasing) {
    env.isReleasing = true;
    env.releaseStartTime = millis();
  }
}

/**
 * Apply pitch offset (sharp/flat) to all currently playing notes
 * Used for momentary pitch bend via right hand buttons
 * Applies to the CURRENT notes (including window offset)
 */
void applyPitchOffset() {
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    if (leftButtonStates[i]) {
      // Calculate base note with window offset
      int offsetToApply = windowOffsetSemitones;
      if (currentScale != SCALE_CHROMATIC) {
        int snappedOffsetDegrees = semitoneOffsetToNearestScaleDegree(windowOffsetSemitones);
        offsetToApply = scaleDegreesToSemitones(snappedOffsetDegrees);
      }
      
      // Apply both window offset AND pitch offset to currently playing note
      int shiftedNote = currentScaleNotes[i] + offsetToApply + pitchOffset;
      float freq = mtof(shiftedNote);
      oscSine[i].SetFreq(freq);
      oscTri[i].SetFreq(freq);
    }
  }
}

/**
 * Trigger a chord (major or minor) with root note and 3rd + 5th intervals
 * Distributes 3 chord tones across available oscillators (0-4)
 * Each chord uses 3 oscillator indices from the pool
 */
void triggerChord(int buttonIndex, bool isMajor) {
  if (buttonIndex < 0 || buttonIndex >= NUM_LEFT_BUTTONS) return;
  
  // Apply window offset like single note mode does
  int offsetToApply = windowOffsetSemitones;
  if (currentScale != SCALE_CHROMATIC) {
    int snappedOffsetDegrees = semitoneOffsetToNearestScaleDegree(windowOffsetSemitones);
    offsetToApply = scaleDegreesToSemitones(snappedOffsetDegrees);
  }
  
  const int* intervals = isMajor ? majorChordIntervals : minorChordIntervals;
  int rootNote = currentScaleNotes[buttonIndex] + offsetToApply + pitchOffset;
  
  // Clear any previous chord tones from this button
  for (int i = 0; i < MAX_CHORD_NOTES; i++) {
    if (chordNotes[i].isActive && chordNotes[i].buttonIndex == buttonIndex) {
      chordNotes[i].isActive = false;
    }
  }
  
  // Build 3-note chord
  int chordNotesArray[3];
  chordNotesArray[0] = rootNote;                    // Root
  chordNotesArray[1] = rootNote + intervals[1];     // 3rd
  chordNotesArray[2] = rootNote + intervals[2];     // 5th
  
  // Assign oscillators: use buttonIndex + offset for each chord note
  // Simple allocation: chord notes use oscillators in sequence
  int oscOffset = (buttonIndex * 3) % 5;  // Spread chords across available oscillators
  
  for (int noteIdx = 0; noteIdx < 3; noteIdx++) {
    if (numActiveChordNotes < MAX_CHORD_NOTES) {
      ChordNote &cn = chordNotes[numActiveChordNotes];
      cn.midiNote = chordNotesArray[noteIdx];
      cn.sineOscIdx = (oscOffset + noteIdx) % NUM_LEFT_BUTTONS;
      cn.triOscIdx = (oscOffset + noteIdx) % NUM_LEFT_BUTTONS;
      cn.buttonIndex = buttonIndex;
      cn.isActive = true;
      
      // Set frequency immediately
      float freq = mtof(cn.midiNote);
      oscSine[cn.sineOscIdx].SetFreq(freq);
      oscTri[cn.triOscIdx].SetFreq(freq);
      
      // Only trigger if this oscillator hasn't been triggered yet in this chord
      if (oscRefCount[cn.sineOscIdx] == 0) {
        triggerNote(cn.sineOscIdx);
      }
      oscRefCount[cn.sineOscIdx]++;
      
      numActiveChordNotes++;
    }
  }
  
  // Log chord
  Serial.print("Chord triggered - Button ");
  Serial.print(buttonIndex + 1);
  Serial.print(" (");
  Serial.print(isMajor ? "Major" : "Minor");
  Serial.print(") - Root: ");
  Serial.print(chordNotesArray[0]);
  Serial.print(", 3rd: ");
  Serial.print(chordNotesArray[1]);
  Serial.print(", 5th: ");
  Serial.print(chordNotesArray[2]);
  Serial.println();
}

/**
 * Release a chord (all voices for a button)
 */
void releaseChord(int buttonIndex) {
  if (currentMode == MODE_CHORD) {
    // Release all chord notes associated with this button
    for (int i = 0; i < MAX_CHORD_NOTES; i++) {
      if (chordNotes[i].isActive && chordNotes[i].buttonIndex == buttonIndex) {
        int oscIdx = chordNotes[i].sineOscIdx;
        oscRefCount[oscIdx]--;
        // Only release the oscillator if no other notes are using it
        if (oscRefCount[oscIdx] == 0) {
          releaseNote(oscIdx);
        }
        chordNotes[i].isActive = false;
      }
    }
    // Compact the array
    int writeIdx = 0;
    for (int i = 0; i < MAX_CHORD_NOTES; i++) {
      if (chordNotes[i].isActive) {
        chordNotes[writeIdx++] = chordNotes[i];
      }
    }
    numActiveChordNotes = writeIdx;
  }
}



/**
 * Process distance sensor with mode-aware behavior
 * MODE_SINGLE_NOTE: Distance → distortion/drive (waveform blending + triangle boost)
 * MODE_CHORD: Distance → strum speed simulation
 * MODE_LATCH: Distance → filter/vibrato depth control
 */
void processDistanceSensor() {
  if (!tofAvailable) return;
  
  if (millis() - lastSensorRead < SENSOR_INTERVAL) return;
  
  if (!sensor.isRangeComplete()) {
    lastSensorRead = millis();
    return;
  }
  
  int distance = sensor.readRange();
  lastSensorRead = millis();
  
  // Only process significant distance changes
  if (abs(distance - lastDistance) <= DISTANCE_CHANGE_THRESHOLD) {
    return;
  }
  
  // Map distance to normalized blend factor (0.0 = far, 1.0 = close)
  float palmBlend = map(constrain(distance, DISTANCE_MIN, DISTANCE_MAX), 
                        DISTANCE_MIN, DISTANCE_MAX, 0, 100) / 100.0f;
  
  if (currentMode == MODE_SINGLE_NOTE) {
    // Single Note Mode: Distance → Distortion/Drive
    // Close distance = more triangle (more aggressive)
    // Far distance = more sine (pure tone)
    
    waveformBlend = 1.0f - palmBlend;  // Invert: far (high distance) = pure sine, close = triangle
    
    // Equal-power crossfade to maintain constant perceived volume
    float blendRadians = waveformBlend * (PI / 2.0f);  // 0 to π/2
    triAmp = sinf(blendRadians);      // 0.0 to 1.0 (curved)
    sineAmp = cosf(blendRadians);     // 1.0 to 0.0 (curved)
    
    // Boost triangle for more dramatic timbral difference
    triBoost = 1.0f + (waveformBlend * 0.8f);  // 1.0x to 1.8x boost
    
    Serial.print("Single Note - Distance: ");
    Serial.print(distance);
    Serial.print(" mm -> Distortion: ");
    Serial.print(waveformBlend * 100, 0);
    Serial.print("% (Tri boost: ");
    Serial.print(triBoost, 2);
    Serial.println("x)");
    
  } else if (currentMode == MODE_CHORD) {
    // Chord Mode: Distance → Strum/Arpeggiator Speed
    // Note: Full arpeggiator implementation requires envelope retrigger system
    // For now, we store the strum speed parameter for future use
    
    // Map distance to strum speed (0.5 - 2.0 notes per second)
    float strumSpeed = 0.5f + (palmBlend * 1.5f);
    
    Serial.print("Chord Mode - Distance: ");
    Serial.print(distance);
    Serial.print(" mm -> Strum Speed: ");
    Serial.print(strumSpeed, 2);
    Serial.println(" notes/sec (arpeggiator: reserved)");
    
  } else if (currentMode == MODE_ARPEGGIO) {
    // Arpeggio Mode: Distance → Arpeggiator Speed Control
    // Map distance to arpeggio tempo
    float arpSpeed = 0.5f + (palmBlend * 1.5f);
    
    Serial.print("Arpeggio Mode - Distance: ");
    Serial.print(distance);
    Serial.print(" mm -> Tempo: ");
    Serial.print(arpSpeed, 2);
    Serial.println(" notes/sec");
  }
  
  lastDistance = distance;
}

void AudioCallback(float **in, float **out, size_t size) {
  for (size_t i = 0; i < size; i++) {
    float mixedSig = 0.0f;
    int activeNotes = 0;
    
    // Mix oscillators with envelope and crossfade
    for (int j = 0; j < NUM_LEFT_BUTTONS; j++) {
      float envLevel = processEnvelope(j);
      
      if (envLevel > 0.001f) {  // Only process if envelope is active
        activeNotes++;
        float sineSig = oscSine[j].Process() * sineAmp;
        float triSig = oscTri[j].Process() * triAmp * triBoost;
        mixedSig += (sineSig + triSig) * envLevel;
      }
    }
    
    // Dynamic polyphony scaling (reduce volume as more notes play)
    if (activeNotes > 0) {
      float polyScale = 1.0f / sqrtf((float)activeNotes);
      mixedSig *= polyScale;
    }
    
    // Apply volume
    mixedSig *= volume * 0.4f;
    
    // Soft clipping to prevent harsh distortion
    mixedSig = softClip(mixedSig);
    
    out[0][i] = mixedSig; // left out
    out[1][i] = mixedSig; // right out
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);  // Increased from 100ms to ensure serial is ready
  
  // MARKER: Identify firmware version for dual-wire I2C test
  Serial.println("\n========================================");
  Serial.println("FIRMWARE BUILD: Dual-Wire I2C (Wire1 test)");
  Serial.println("========================================\n");

  // init Daisy
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  float sample_rate = DAISY.get_samplerate();

  // init oscillators (sine and triangle pairs)
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    oscSine[i].Init(sample_rate);
    oscSine[i].SetWaveform(Oscillator::WAVE_SIN);
    oscSine[i].SetAmp(1.0f);
    
    oscTri[i].Init(sample_rate);
    oscTri[i].SetWaveform(Oscillator::WAVE_TRI);
    oscTri[i].SetAmp(1.0f);  // Now controlled by blend factor
    
    // Initialize envelopes
    envelopes[i].level = 0.0f;
    envelopes[i].isActive = false;
    envelopes[i].isReleasing = false;
  }

  DAISY.begin(AudioCallback); // start audio processing
  pinMode(VOLUME_PIN, INPUT); // volume pot

  // I2C sensor initialization - Both sensors on I2C1
  // Daisy Seed STM32H750 I2C1: D12 (SDA), D11 (SCL)
  // Both VL53L0X (0x29) and MSA311 (0x62) on same bus
  // VL53L0X continuous ranging deferred until after MSA311 init to avoid contention

  // Initialize I2C1 (default Wire object)
  Serial.println("Initializing Wire (I2C1)...");
  Wire.begin();
  Wire.setClock(400000);
  
  delay(200);  // Give I2C bus time to stabilize
  
  // Run I2C scan to see what's connected
  Serial.println("\n=== I2C Scan on Bus 1 ===");
  i2cScan();
  Serial.println("=== Scan complete ===\n");
  
  delay(100);
  
  // Initialize VL53L0X (address 0x29) on I2C1
  Serial.println("Initializing VL53L0X ToF sensor (0x29) on I2C1...");
  if (sensor.begin(0x29, false, &Wire)) {
    Serial.println("✓ VL53L0X OK");
    tofAvailable = true;
  } else {
    Serial.println("✗ VL53L0X initialization failed");
    tofAvailable = false;
  }
  
  delay(200);  // Sensor stabilization
  
  // Initialize MSA311 (address 0x62) on I2C1
  // DISABLED for v0.5: Using button-based window control instead of IMU
  Serial.println("MSA311 accelerometer DISABLED (using button-based window control)");
  accelAvailable = false;
  
  // Uncomment below to re-enable IMU in future version:
  // if (accel.begin(0x62, &Wire)) {
  //   Serial.println("✓ MSA311 OK - IMU features enabled");
  //   accelAvailable = true;
  // } else {
  //   Serial.println("✗ MSA311 initialization failed");
  //   accelAvailable = false;
  // }
  
  delay(150);
  
  // Start VL53L0X continuous ranging AFTER both sensors initialized
  // This avoids I2C bus contention during initialization
  if (tofAvailable) {
    Serial.println("Starting VL53L0X continuous ranging...");
    sensor.startRangeContinuous();
  }

  // left hand buttons
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    leftButton[i].Init(1000, true, leftButtonPins[i], INPUT_PULLUP);
  }

  // right hand buttons
  for (int i = 0; i < NUM_RIGHT_BUTTONS; i++) {
    rightButton[i].Init(1000, true, rightButtonPins[i], INPUT_PULLUP);
  }

  updateScaleNotes();  // init scale notes
  Serial.println("Two-handed NIME controller initialized!");
  Serial.println("Left hand: Note articulation (D8-D12)");
  Serial.println("Right hand: Modifiers (D15-D19)");
  Serial.println("Current key: C, Octave: 4, Scale: Major Pentatonic");
}

/**
 * Update arpeggiator - cycles through triad notes when button held in MODE_ARPEGGIO
 */
void updateArpeggiator() {
  if (currentMode != MODE_ARPEGGIO) return;
  
  unsigned long now = millis();
  if (now - lastArpStepTime < ARP_INTERVAL_MS) return;
  lastArpStepTime = now;
  
  // Find which button is currently held
  int heldButton = -1;
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    if (leftButtonStates[i]) {
      heldButton = i;
      break;  // Just use first held button for now
    }
  }
  
  if (heldButton == -1) {
    // No button held, silence current note
    if (currentArpButton >= 0) {
      releaseNote(currentArpButton);
      currentArpButton = -1;
    }
    return;
  }
  
  // Build triad for current button (major/minor based on scale)
  bool isMajor = (currentScale != SCALE_MINOR_PENTATONIC);
  const int* intervals = isMajor ? majorChordIntervals : minorChordIntervals;
  
  int offsetToApply = windowOffsetSemitones;
  if (currentScale != SCALE_CHROMATIC) {
    int snapped = semitoneOffsetToNearestScaleDegree(windowOffsetSemitones);
    offsetToApply = scaleDegreesToSemitones(snapped);
  }
  
  int rootNote = currentScaleNotes[heldButton] + offsetToApply + pitchOffset;
  int triadNotes[3] = {
    rootNote,                    // Root
    rootNote + intervals[1],     // 3rd
    rootNote + intervals[2]      // 5th
  };
  
  // Ensure previous note is fully released before triggering next
  if (currentArpButton >= 0 && currentArpButton != heldButton) {
    releaseNote(currentArpButton);
  }
  
  // Play current note in sequence
  int noteToPlay = triadNotes[currentArpNoteIndex];
  float freq = mtof(noteToPlay);
  
  oscSine[heldButton].SetFreq(freq);
  oscTri[heldButton].SetFreq(freq);
  oscSine[heldButton].Reset();
  oscTri[heldButton].Reset();
  triggerNote(heldButton);
  
  currentArpButton = heldButton;
  
  // Advance to next note in triad
  currentArpNoteIndex = (currentArpNoteIndex + 1) % 3;
  
  Serial.print("Arp: Note ");
  Serial.println(noteToPlay);
}

void handleRightHand() {
  // Check for combinations
  bool thumbPressed = rightButtonStates[RIGHT_THUMB];
  bool indexPressed = rightButtonStates[RIGHT_INDEX];
  bool middlePressed = rightButtonStates[RIGHT_MIDDLE];
  bool ringPressed = rightButtonStates[RIGHT_RING];
  bool pinkyPressed = rightButtonStates[RIGHT_PINKY];
  
  // Detect rising edges (just pressed)
  bool thumbRising = thumbPressed && !rightButtonPrevStates[RIGHT_THUMB];
  bool indexRising = indexPressed && !rightButtonPrevStates[RIGHT_INDEX];
  bool middleRising = middlePressed && !rightButtonPrevStates[RIGHT_MIDDLE];
  bool ringRising = ringPressed && !rightButtonPrevStates[RIGHT_RING];
  bool pinkyRising = pinkyPressed && !rightButtonPrevStates[RIGHT_PINKY];
  
  // NO THUMB PRESSED (Window control & momentary effects)
  if (!thumbPressed) {
    // Single button actions
    {
      // PINKY: Window DOWN 1 scale degree
      if (pinkyRising) {
        setWindowByDegrees(windowOffsetDegrees - 1);
      }
      // INDEX: Window UP 1 scale degree  
      if (indexRising) {
        setWindowByDegrees(windowOffsetDegrees + 1);
      }
      
      // MIDDLE: Momentary sharp (hold)
      if (middlePressed && !rightButtonPrevStates[RIGHT_MIDDLE]) {
        pitchOffset = 1;
        applyPitchOffset();
        Serial.println("Momentary Sharp (#): +1 semitone");
      } else if (!middlePressed && rightButtonPrevStates[RIGHT_MIDDLE]) {
        pitchOffset = 0;
        applyPitchOffset();
        Serial.println("Sharp Released");
      }
      
      // RING: Momentary flat (hold)
      if (ringPressed && !rightButtonPrevStates[RIGHT_RING]) {
        pitchOffset = -1;
        applyPitchOffset();
        Serial.println("Momentary Flat (♭): -1 semitone");
      } else if (!ringPressed && rightButtonPrevStates[RIGHT_RING]) {
        pitchOffset = 0;
        applyPitchOffset();
        Serial.println("Flat Released");
      }
    }
  }
  // THUMB HELD (Mode switches & octave window shifts)
  else {
    // Single button actions with thumb (check these first to avoid conflicts)
    // THUMB + PINKY: Window DOWN 1 octave
    if (pinkyRising && !indexPressed && !middlePressed && !ringPressed) {
      setWindowBySemitones(windowOffsetSemitones - 12);
    }
    // THUMB + INDEX: Window UP 1 octave
    else if (indexRising && !pinkyPressed && !middlePressed && !ringPressed) {
      setWindowBySemitones(windowOffsetSemitones + 12);
    }
    // THUMB + MIDDLE: Cycle through scales (Major Pent -> Minor Pent -> Chromatic -> loop)
    else if (middleRising && !indexPressed && !pinkyPressed && !ringPressed) {
      // Cycle to next scale
      currentScale = (currentScale + 1) % 3;  // 0, 1, 2, then back to 0
      updateScaleNotes();
      switch (currentScale) {
        case SCALE_MAJOR_PENTATONIC:
          Serial.println("Scale: Major Pentatonic");
          break;
        case SCALE_MINOR_PENTATONIC:
          Serial.println("Scale: Minor Pentatonic");
          break;
        case SCALE_CHROMATIC:
          Serial.println("Scale: Chromatic");
          break;
      }
    }
    // THUMB + RING: Cycle through play modes (Single Note -> Chord -> Arpeggio -> loop)
    else if (ringRising && !indexPressed && !middlePressed && !pinkyPressed) {
      switch (currentMode) {
        case MODE_SINGLE_NOTE:
          currentMode = MODE_CHORD;
          Serial.print("Mode: Chord (");
          Serial.print(currentScale == SCALE_MINOR_PENTATONIC ? "Minor" : "Major");
          Serial.println(")");
          break;
        case MODE_CHORD:
          currentMode = MODE_ARPEGGIO;
          Serial.println("Mode: Arpeggio");
          break;
        case MODE_ARPEGGIO:
          currentMode = MODE_SINGLE_NOTE;
          clearAllNotes();
          Serial.println("Mode: Single Note");
          break;
      }
    }
  }
  
  // Store previous states
  for (int i = 0; i < NUM_RIGHT_BUTTONS; i++) {
    rightButtonPrevStates[i] = rightButtonStates[i];
  }

};

void handleLeftHand() {
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    leftButton[i].Debounce();

    bool pressed     = leftButton[i].Pressed();        // current physical state
    bool wasPressed  = leftButtonPrevStates[i];        // previous physical state
    bool rising      =  pressed && !wasPressed;        // just pressed
    bool falling     = !pressed &&  wasPressed;        // just released

    if (currentMode == MODE_CHORD) {
      // Chord mode: press triggers chord (major/minor based on scale)
      // Note: Chords don't latch - they release immediately on button release
      if (rising) {
        bool isMajor = (currentScale != SCALE_MINOR_PENTATONIC);  // Minor pent = minor chords, else major
        triggerChord(i, isMajor);
      }
      if (falling) {
        releaseChord(i);
        Serial.print("Chord OFF - Button ");
        Serial.println(i + 1);
      }
    } else if (currentMode == MODE_ARPEGGIO) {
      // Arpeggio mode: button press just sets state, arpeggiator handles notes
      if (rising) {
        leftButtonStates[i] = true;
        Serial.print("Arp button ON: ");
        Serial.println(i + 1);
      }
      if (falling) {
        leftButtonStates[i] = false;
        if (currentArpButton == i) {
          releaseNote(i);
          currentArpButton = -1;
        }
        Serial.print("Arp button OFF: ");
        Serial.println(i + 1);
      }
    } else {
      // Normal: press = ON, release = OFF (single note mode)
      if (rising) {
        leftButtonStates[i] = true;
        int offsetToApply = windowOffsetSemitones;
        // For non-chromatic scales, snap to scale degrees
        if (currentScale != SCALE_CHROMATIC) {
          int snappedOffsetDegrees = semitoneOffsetToNearestScaleDegree(windowOffsetSemitones);
          offsetToApply = scaleDegreesToSemitones(snappedOffsetDegrees);
        }
        int note = currentScaleNotes[i] + offsetToApply;
        float freq = mtof(note);
        oscSine[i].SetFreq(freq);
        oscTri[i].SetFreq(freq);
        triggerNote(i);  // Start envelope attack
        Serial.print("Note ON - Button ");
        Serial.print(i + 1);
        Serial.print(", MIDI Note: ");
        Serial.print(note);
        Serial.print(" (");
        Serial.print(freq);
        Serial.print(" Hz, windowOffset: ");
        Serial.print(windowOffsetSemitones);
        Serial.println(" semitones)");
      }
      if (falling) {
        leftButtonStates[i] = false;
        releaseNote(i);  // Start envelope release
        Serial.print("Note OFF - Button ");
        Serial.println(i + 1);
      }
    }

    // Update previous physical state
    leftButtonPrevStates[i] = pressed;
  }
};

void loop() {
  // Update arpeggiator (if in arpeggio mode)
  updateArpeggiator();
  
  // volume
  int volumeRaw = analogRead(VOLUME_PIN);
  
  if (abs((volumeRaw - lastVolumeRaw)) > VOLUME_CHANGE_THRESHOLD) {
    volume = (volumeRaw / 1023.0f) * VOLUME_SCALE;
    float volumePercent = volume * 200; // Convert back to percentage for display
    Serial.print("Volume: ");
    Serial.print(volumePercent, 1);
    Serial.println("%");
    lastVolumeRaw = volumeRaw;
  }

  // right hand
  for (int i = 0; i < NUM_RIGHT_BUTTONS; i++) {
    rightButton[i].Debounce();
    rightButtonStates[i] = rightButton[i].Pressed();
  }

  handleRightHand();

  // left hand
  handleLeftHand();

  // Distance sensor processing (mode-aware palm effects)
  processDistanceSensor();

  // Pitch window control (accelerometer-based sliding through scale)
  // Right Pinky: enables pitch window mode (tilt left hand to move window)
  // Window locks in place when pinky is released
  // If a note is held while pinky is active, the note pitch slides with the window
  
  bool indexPressed = rightButtonStates[RIGHT_INDEX];
  bool pinkyPressed = rightButtonStates[RIGHT_PINKY];
  
  // Check if any left buttons are actively playing
  bool anyNoteActive = false;
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    if (leftButtonStates[i] && envelopes[i].isActive) {
      anyNoteActive = true;
      break;
    }
  }
  
  // Pitch window mode: pinky enables, tilt left hand to slide window
  // IMPORTANT: !indexPressed ensures index button disables pitch window (including calibration mode)
  if (accelAvailable && pinkyPressed && !indexPressed && !rightButtonStates[RIGHT_THUMB] && (millis() - lastAccelRead >= ACCEL_INTERVAL)) {
    accel.read();
    float accelX = accel.x;
    
    // Apply low-pass filter to smooth accelerometer noise
    smoothedAccelX = (ACCEL_FILTER_ALPHA * accelX) + ((1.0f - ACCEL_FILTER_ALPHA) * smoothedAccelX);
    
    // Calculate deviation from calibrated center (left hand tilt)
    float deviation = smoothedAccelX - accelCenterX;
    
    // Debug: Print accel values every ~500ms for tuning
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint >= 500) {
      Serial.print("[ACCEL] Raw: ");
      Serial.print(accelX, 1);
      Serial.print("G, Smoothed: ");
      Serial.print(smoothedAccelX, 1);
      Serial.print("G, Deviation: ");
      Serial.print(deviation, 1);
      Serial.print("G, Offset: ");
      Serial.print(windowOffsetSemitones);
      Serial.println("st");
      lastDebugPrint = millis();
    }
    
    // Only apply pitch window if tilt exceeds deadzone
    if (abs(deviation) > WINDOW_DEADZONE) {
      // Map tilt to semitone offset (NOT scale degrees)
      accelPositionOffset = deviation * WINDOW_SENSITIVITY;
      
      // Constrain to window bounds (asymmetric: -36 to +24 semitones)
      int newWindowOffsetSemitones = (int)round(accelPositionOffset);
      newWindowOffsetSemitones = constrain(newWindowOffsetSemitones, -MAX_WINDOW_OFFSET_DOWN, MAX_WINDOW_OFFSET_UP);
      
      // If window position changed, update active notes in real-time
      if (newWindowOffsetSemitones != windowOffsetSemitones) {
        int oldOffsetSemitones = windowOffsetSemitones;
        windowOffsetSemitones = newWindowOffsetSemitones;
        
        // Calculate nearest scale degree (for display and snapping)
        windowOffsetDegrees = semitoneOffsetToNearestScaleDegree(windowOffsetSemitones);
        
        Serial.print("[WINDOW CHANGED] ");
        Serial.print(oldOffsetSemitones);
        Serial.print("st -> ");
        Serial.print(windowOffsetSemitones);
        Serial.println("st");
        
        // Slide pitches of held notes with the window
        if (anyNoteActive) {
          for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
            if (leftButtonStates[i] && envelopes[i].isActive) {
              int offsetToApply = windowOffsetSemitones;
              // For non-chromatic scales, snap to scale degrees
              if (currentScale != SCALE_CHROMATIC) {
                int snappedOffsetDegrees = semitoneOffsetToNearestScaleDegree(windowOffsetSemitones);
                offsetToApply = scaleDegreesToSemitones(snappedOffsetDegrees);
              }
              int bentNote = currentScaleNotes[i] + offsetToApply;
              float freq = mtof(bentNote);
              oscSine[i].SetFreq(freq);
              oscTri[i].SetFreq(freq);
            }
          }
        }
        
        // Print window info
        Serial.print("[PITCH WINDOW] ");
        printWindow();
      }
    }
    
    lastAccelX = accelX;
    lastAccelRead = millis();
  }
  // NOTE: Window offset persists when pinky is released - it only resets on calibration or explicit action
  // This allows the pitch to "stick" at the final tilt position

  delay(1);
}