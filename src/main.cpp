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
const float ATTACK_TIME = 0.02f;   // 20ms attack to eliminate clicks
const float RELEASE_TIME = 0.15f;  // 150ms release for smooth fade
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
const int MAX_WINDOW_OFFSET_SEMITONES = 36;  // ±36 semitones max (3 octaves, consistent across all scales)
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
  MODE_MAJOR_CHORD = 1,     // Root + Major 3rd + Perfect 5th
  MODE_MINOR_CHORD = 2      // Root + Minor 3rd + Perfect 5th
};
int currentMode = MODE_SINGLE_NOTE;
bool latchMode = false;             // When true, buttons latch notes ON

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

// Forward declaration
void releaseNote(int noteIndex);

void clearAllLatchedNotes() {
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    leftButtonStates[i] = false;
    releaseNote(i);  // Trigger envelope release for smooth fade-out
  }
  Serial.println("All latched notes cleared");
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
  for (int d = 1; d <= MAX_WINDOW_OFFSET_SEMITONES; d++) {
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
  
  if (env.isReleasing) {
    // Release phase
    float elapsed = (currentTime - env.releaseStartTime) / 1000.0f;
    if (elapsed >= RELEASE_TIME) {
      env.isActive = false;
      env.level = 0.0f;
      return 0.0f;
    }
    env.level = 1.0f - (elapsed / RELEASE_TIME);
  } else {
    // Attack phase
    float elapsed = (currentTime - env.attackStartTime) / 1000.0f;
    if (elapsed >= ATTACK_TIME) {
      env.level = 1.0f;
    } else {
      env.level = elapsed / ATTACK_TIME;
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
 */
void applyPitchOffset() {
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    if (leftButtonStates[i]) {
      // Note is playing, shift its frequency
      int shiftedNote = currentScaleNotes[i] + pitchOffset;
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
  
  const int* intervals = isMajor ? majorChordIntervals : minorChordIntervals;
  int rootNote = currentScaleNotes[buttonIndex] + pitchOffset;
  
  // Clear any previous chord tones from this button
  for (int i = 0; i < MAX_CHORD_NOTES; i++) {
    if (chordNotes[i].isActive && chordNotes[i].sineOscIdx == buttonIndex) {
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
      cn.isActive = true;
      
      // Set frequency immediately
      float freq = mtof(cn.midiNote);
      oscSine[cn.sineOscIdx].SetFreq(freq);
      oscTri[cn.triOscIdx].SetFreq(freq);
      triggerNote(cn.sineOscIdx);
      
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
  if (currentMode == MODE_MAJOR_CHORD || currentMode == MODE_MINOR_CHORD) {
    // Release all chord notes associated with this button
    for (int i = 0; i < MAX_CHORD_NOTES; i++) {
      if (chordNotes[i].isActive && chordNotes[i].sineOscIdx == buttonIndex) {
        releaseNote(chordNotes[i].sineOscIdx);
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
 * MODE_MAJOR_CHORD/MODE_MINOR_CHORD: Distance → strum speed simulation
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
    
  } else if (currentMode == MODE_MAJOR_CHORD || currentMode == MODE_MINOR_CHORD) {
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
    
  } else if (latchMode) {
    // Latch Mode: Distance → Filter/Vibrato Depth Control
    // This would control filter cutoff or vibrato depth
    // For now, we store the control value for future implementation
    
    float filterOrVibratoDepth = palmBlend;
    
    Serial.print("Latch Mode - Distance: ");
    Serial.print(distance);
    Serial.print(" mm -> Filter/Vibrato Depth: ");
    Serial.print(filterOrVibratoDepth * 100, 0);
    Serial.println("% (filter/vibrato: reserved)");
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
  Serial.println("Initializing MSA311 accelerometer (0x62) on I2C1...");
  if (accel.begin(0x62, &Wire)) {
    Serial.println("✓ MSA311 OK - IMU features enabled");
    accelAvailable = true;
  } else {
    Serial.println("✗ MSA311 initialization failed");
    accelAvailable = false;
  }
  
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

void handleRightHand() {
  // check for combinations
  bool thumbPressed = rightButtonStates[RIGHT_THUMB];
  bool indexPressed = rightButtonStates[RIGHT_INDEX];
  bool middlePressed = rightButtonStates[RIGHT_MIDDLE];
  bool ringPressed = rightButtonStates[RIGHT_RING];
  bool pinkyPressed = rightButtonStates[RIGHT_PINKY];
  
  // Debug: Log button combinations
  static bool lastState[5] = {false};
  if (indexPressed != lastState[RIGHT_INDEX] || pinkyPressed != lastState[RIGHT_PINKY] || thumbPressed != lastState[RIGHT_THUMB]) {
    Serial.print("[BUTTONS] Thumb:");
    Serial.print(thumbPressed ? "1" : "0");
    Serial.print(" Index:");
    Serial.print(indexPressed ? "1" : "0");
    Serial.print(" Middle:");
    Serial.print(middlePressed ? "1" : "0");
    Serial.print(" Ring:");
    Serial.print(ringPressed ? "1" : "0");
    Serial.print(" Pinky:");
    Serial.println(pinkyPressed ? "1" : "0");
    for (int i = 0; i < 5; i++) lastState[i] = rightButtonStates[i];
  }

  // Handle momentary sharp/flat (when thumb NOT pressed)
  if (!thumbPressed) {
    // Momentary sharp (middle finger)
    if (middlePressed && !rightButtonPrevStates[RIGHT_MIDDLE]) {
      pitchOffset = 1;
      applyPitchOffset();
      Serial.println("Momentary Sharp (#): +1 semitone to playing notes");
    } else if (!middlePressed && rightButtonPrevStates[RIGHT_MIDDLE]) {
      pitchOffset = 0;
      applyPitchOffset();
      Serial.println("Sharp Released: back to normal pitch");
    }
    
    // Momentary flat (ring finger)
    if (ringPressed && !rightButtonPrevStates[RIGHT_RING]) {
      pitchOffset = -1;
      applyPitchOffset();
      Serial.println("Momentary Flat (♭): -1 semitone to playing notes");
    } else if (!ringPressed && rightButtonPrevStates[RIGHT_RING]) {
      pitchOffset = 0;
      applyPitchOffset();
      Serial.println("Flat Released: back to normal pitch");
    }
  }

  // thumb ("shift" button)
  if (thumbPressed) {
    // change scale
    if (indexPressed && !rightButtonPrevStates[RIGHT_INDEX]) {
      currentScale = SCALE_MAJOR_PENTATONIC;
      updateScaleNotes();
      Serial.println("Scale: Major Pentatonic");
    }
    if (middlePressed && !rightButtonPrevStates[RIGHT_MIDDLE]) {
      currentScale = SCALE_MINOR_PENTATONIC;
      updateScaleNotes();
      Serial.println("Scale: Minor Pentatonic");
    }
    if (ringPressed && !rightButtonPrevStates[RIGHT_RING]) {
      currentScale = SCALE_CHROMATIC;
      updateScaleNotes();
      Serial.println("Scale: Chromatic");
    }
    // latch
    if (pinkyPressed && !rightButtonPrevStates[RIGHT_PINKY]) {
      latchMode = !latchMode;
      Serial.print("Latch Mode: ");
      Serial.println(latchMode ? "ON" : "OFF");
      // When turning OFF latch mode, clear all latched notes
      if (!latchMode) {
        clearAllLatchedNotes();
      }
    }
    // change chord
    else if (indexPressed && middlePressed) {
      if (currentMode != MODE_MAJOR_CHORD) {
        currentMode = MODE_MAJOR_CHORD;
        Serial.println("Mode: Major Chord");
      }
    } 
    else if (indexPressed && ringPressed) {
      if (currentMode != MODE_MINOR_CHORD) {
        currentMode = MODE_MINOR_CHORD;
        Serial.println("Mode: Minor Chord");
      }
    } 
    // change key
    else if (middlePressed && ringPressed) {
      // key set mode – handled in left hand
      Serial.println("Key Set Mode – Use left hand to select key");
    }
  }
  // single button actions (IMU calibration and other controls)
  else {
    // Check for calibration gesture (both index + pinky held together)
    // Calibrates IMU center position for pitch bend zero-point
    if (indexPressed && pinkyPressed) {
      if (!isCalibrating && calibrationStartTime == 0) {
        calibrationStartTime = millis();
        Serial.println("Hold for 2s to calibrate IMU center position...");
      } else if (millis() - calibrationStartTime >= CALIBRATION_HOLD_TIME) {
        // Calibrate IMU!
        if (accelAvailable) {
          accel.read();
          accelCenterX = accel.x;
          accelCenterZ = accel.z;
          accelPositionOffset = 0.0f;  // Reset pitch window offset after calibration
          Serial.println("=== IMU CALIBRATED ===");
          Serial.print("Center X: ");
          Serial.print(accelCenterX, 2);
          Serial.print("G, Center Z: ");
          Serial.print(accelCenterZ, 2);
          Serial.println("G");
        }
        isCalibrating = false;
        calibrationStartTime = 0;
      }
    } else {
      // Reset calibration timer if buttons released
      if (calibrationStartTime != 0 && !isCalibrating) {
        Serial.println("Calibration cancelled");
      }
      calibrationStartTime = 0;
      isCalibrating = false;
    }
    // reset to single note (no combos pressed)
    if (!indexPressed && !middlePressed && !ringPressed && currentMode != MODE_SINGLE_NOTE) {
      currentMode = MODE_SINGLE_NOTE;
      Serial.println("Mode: Single Note");
    }
  }
  
  // Store previous states
  for (int i = 0; i < NUM_RIGHT_BUTTONS; i++) {
    rightButtonPrevStates[i] = rightButtonStates[i];
  }

};

void handleLeftHand() {
  bool keySetMode = rightButtonStates[RIGHT_MIDDLE] && rightButtonStates[RIGHT_RING];

  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    leftButton[i].Debounce();

    bool pressed     = leftButton[i].Pressed();        // current physical state
    bool wasPressed  = leftButtonPrevStates[i];        // previous physical state
    bool rising      =  pressed && !wasPressed;        // just pressed
    bool falling     = !pressed &&  wasPressed;        // just released

    if (keySetMode) {
      // Change key on press only
      if (rising) {
        int newKey = (i * 2) % 12; // simple mapping, tweak as desired
        currentKey = newKey;
        updateScaleNotes();
        Serial.print("New Key: ");
        Serial.println(currentKey);
      }
    } else if (currentMode == MODE_MAJOR_CHORD || currentMode == MODE_MINOR_CHORD) {
      // Chord mode: press triggers chord
      if (rising) {
        leftButtonStates[i] = true;
        bool isMajor = (currentMode == MODE_MAJOR_CHORD);
        triggerChord(i, isMajor);
      }
      if (falling) {
        leftButtonStates[i] = false;
        releaseChord(i);
        Serial.print("Chord OFF - Button ");
        Serial.println(i + 1);
      }
    } else if (latchMode) {
      // Latch mode: press latches note ON, press again re-triggers
      if (rising) {
        if (!leftButtonStates[i]) {
          // Note was off, latch it on
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
          Serial.print("Note LATCHED - Button ");
          Serial.print(i + 1);
          Serial.print(", MIDI Note: ");
          Serial.print(note);
          Serial.print(" (");
          Serial.print(freq);
          Serial.println(" Hz)");
        } else {
          // Note already latched, re-trigger envelope
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
          oscSine[i].Reset();
          oscTri[i].Reset();
          triggerNote(i);  // Retrigger envelope from start
          Serial.print("Note RE-TRIGGERED - Button ");
          Serial.println(i + 1);
        }
      }
      // Ignore release in latch mode
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
      
      // Constrain to ±MAX_WINDOW_OFFSET_SEMITONES (consistent across all scales)
      int newWindowOffsetSemitones = (int)round(accelPositionOffset);
      newWindowOffsetSemitones = constrain(newWindowOffsetSemitones, -MAX_WINDOW_OFFSET_SEMITONES, MAX_WINDOW_OFFSET_SEMITONES);
      
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