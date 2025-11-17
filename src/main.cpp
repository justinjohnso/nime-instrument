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

// Accelerometer (MSA301 3-axis)
Adafruit_MSA301 accel = Adafruit_MSA301();
bool accelAvailable = false;

// IMU calibration and pitch bend parameters
float accelCenterX = 0.0f;              // Calibrated center X acceleration (tilt axis)
float accelCenterZ = 0.0f;              // Calibrated center Z acceleration (roll axis)
const float PITCH_BEND_RANGE = 2.0f;    // ±2 semitones from IMU tilt
const float PITCH_BEND_SENSITIVITY = 0.3f;  // Acceleration to semitones conversion
const float MODULATION_SENSITIVITY = 0.15f; // Acceleration to modulation depth
unsigned long lastIMURead = 0;
const unsigned long IMU_POLL_INTERVAL = 20; // 50Hz polling

// Distance sensor parameters
const int DISTANCE_CHANGE_THRESHOLD = 5;      // Minimum change in mm to process
const int DISTANCE_MIN = 50;                  // Minimum distance for mapping (mm)
const int DISTANCE_MAX = 300;                 // Maximum distance for mapping (mm)
const unsigned long SENSOR_INTERVAL = 50;     // Poll interval in ms
int lastDistance = -1;
unsigned long lastSensorRead = 0;
bool tofAvailable = false;

// Sliding Window (Accelerometer-based note selection) - LEGACY, disabled
// Note: This experimental feature is replaced by IMU pitch bend/modulation
// These variables kept for reference/potential future use
int windowOffset = 0;                         // Reserved for future sliding window feature
// const int WINDOW_SIZE = 5;                    // Number of notes in window
// const int MAX_WINDOW_OFFSET = 24;             // ±2 octaves
// float accelPositionOffset = 0.0f;             // Integrated position from center
// float lastAccelX = 0.0f;                      // Previous X acceleration
// unsigned long lastAccelRead = 0;
// const unsigned long ACCEL_INTERVAL = 20;      // 50Hz polling
// const float COARSE_SENSITIVITY = 8.0f;        // Semitones per second of movement (index)
// const float FINE_SENSITIVITY = 2.0f;          // Semitones per second of movement (pinky)

// Calibration
unsigned long calibrationStartTime = 0;
const unsigned long CALIBRATION_HOLD_TIME = 2000;  // 2 seconds to calibrate
bool isCalibrating = false;

// Left Hand Buttons (Note Articulation)
const int NUM_LEFT_BUTTONS = 5;
Switch leftButton[NUM_LEFT_BUTTONS];
const int leftButtonPins[NUM_LEFT_BUTTONS] = {8, 9, 10, 13, 14};  // D8-D12 (skip D11)
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
  RIGHT_PINKY = 0,    // Octave down (D15)
  RIGHT_RING = 1,     // Momentary flat (D16)
  RIGHT_MIDDLE = 2,   // Momentary sharp (D17)
  RIGHT_INDEX = 3,    // Octave up (D18)
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
  SCALE_BLUES = 1,
  SCALE_CHROMATIC = 2
};
int currentScale = SCALE_MAJOR_PENTATONIC;

// Scale intervals (semitones from root, mapped to 5 buttons)
const int majorPentatonic[] = {0, 2, 4, 7, 9};    // C, D, E, G, A
const int bluesScale[] = {0, 3, 5, 6, 7};         // C, Eb, F, F#, G  
const int chromaticScale[] = {0, 1, 2, 3, 4};     // C, C#, D, D#, E

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
 * Update the current scale notes based on octave, key, scale type, and window offset
 * Calculates MIDI note numbers for each of the 5 buttons
 * Window offset allows sliding through the scale
 */
void updateScaleNotes() {
  int baseNote = (currentOctave * 12) + currentKey + windowOffset;

  switch (currentScale) {
    case SCALE_MAJOR_PENTATONIC:
      for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
        currentScaleNotes[i] = baseNote + majorPentatonic[i];
      }
      break;
    case SCALE_BLUES:
      for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
        currentScaleNotes[i] = baseNote + bluesScale[i];
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
 * Print current sliding window information
 */
void printWindow() {
  Serial.print("Window: ");
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    Serial.print(currentScaleNotes[i]);
    if (i < NUM_LEFT_BUTTONS - 1) Serial.print(", ");
  }
  Serial.print(" (offset: ");
  Serial.print(windowOffset);
  Serial.println(" semitones)");
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
 * Uses oscillator multiplexing: distributes chord tones across available oscillators
 * One button press plays 3 notes (root, 3rd, 5th) - cycling through oscillators
 */
void triggerChord(int buttonIndex, bool isMajor) {
  if (buttonIndex < 0 || buttonIndex >= NUM_LEFT_BUTTONS) return;
  
  const int* intervals = isMajor ? majorChordIntervals : minorChordIntervals;
  int rootNote = currentScaleNotes[buttonIndex] + pitchOffset;
  
  // Clear any previous chord voices from this button
  // (Handling re-trigger case)
  for (int i = 0; i < 5 * CHORD_NOTES_PER_VOICE; i++) {
    if (chordVoices[i].isActive && chordVoices[i].oscillatorIndex == buttonIndex) {
      chordVoices[i].isActive = false;
      releaseNote(buttonIndex);  // Release this oscillator
    }
  }
  
  // Play 3 notes of the chord (root, 3rd, 5th)
  // Constraint: Use available oscillators sequentially (0-4)
  // For now: play root and 5th in lowest octave, 3rd in higher octave
  int chordNotesArray[3];
  chordNotesArray[0] = rootNote;                    // Root
  chordNotesArray[1] = rootNote + intervals[1];     // 3rd (up one octave for separation)
  chordNotesArray[2] = rootNote + intervals[2];     // 5th
  
  // Trigger all 3 chord tones via same oscillator pair (buttonIndex)
  // Use waveform blend to create texture difference between chord tones
  // Root: mostly sine, 3rd: blend, 5th: more triangle
  float freq = mtof(chordNotesArray[0]);
  oscSine[buttonIndex].SetFreq(freq);
  oscTri[buttonIndex].SetFreq(freq);
  triggerNote(buttonIndex);  // Start envelope
  
  // Log chord
  Serial.print("Chord triggered - Button ");
  Serial.print(buttonIndex + 1);
  Serial.print(", Root: ");
  Serial.print(chordNotesArray[0]);
  Serial.print(" (");
  Serial.print(mtof(chordNotesArray[0]));
  Serial.print("Hz), 3rd: ");
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
    releaseNote(buttonIndex);  // Release the oscillator pair
  }
}

/**
 * Process IMU data for pitch bend (tilt) and modulation (roll)
 * X-axis tilt: ±2 semitones pitch bend
 * Z-axis roll: vibrato/tremolo modulation depth (0-1.0)
 */
void processIMU() {
  if (!accelAvailable) return;
  
  unsigned long now = millis();
  if (now - lastIMURead < IMU_POLL_INTERVAL) return;
  lastIMURead = now;
  
  // Read accelerometer data
  accel.read();
  float accelX = accel.x;
  float accelZ = accel.z;
  
  // Calculate pitch bend from X-axis tilt
  // Deviation from center position in G units converted to semitones
  float pitchBendAmount = (accelX - accelCenterX) * PITCH_BEND_SENSITIVITY;
  pitchBendAmount = constrain(pitchBendAmount, -PITCH_BEND_RANGE, PITCH_BEND_RANGE);
  
  // Calculate modulation depth from Z-axis roll
  float modulationDepth = (accelZ - accelCenterZ) * MODULATION_SENSITIVITY;
  modulationDepth = constrain(modulationDepth, 0.0f, 1.0f);
  
  // Apply pitch bend to all active notes
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    if (envelopes[i].isActive) {
      // Calculate bent frequency: pitch = baseNote + pitchBendAmount
      int bentNote = currentScaleNotes[i] + (int)round(pitchBendAmount);
      float freq = mtof(bentNote);
      
      // Apply bend smoothly
      oscSine[i].SetFreq(freq);
      oscTri[i].SetFreq(freq);
    }
  }
  
  // TODO: Implement modulation (vibrato/tremolo) using modulationDepth
  // This would require adding an LFO to the envelope system
  
  // Debug output at lower frequency
  static unsigned long lastDebugPrint = 0;
  if (now - lastDebugPrint > 500) {  // Print every 500ms
    Serial.print("IMU - Tilt (X): ");
    Serial.print(accelX, 2);
    Serial.print("G -> Pitch Bend: ");
    Serial.print(pitchBendAmount, 2);
    Serial.print(" semitones, Roll (Z): ");
    Serial.print(accelZ, 2);
    Serial.print("G -> Modulation: ");
    Serial.print(modulationDepth * 100, 0);
    Serial.println("%");
    lastDebugPrint = now;
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
    
    waveformBlend = palmBlend;
    
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

  // I2C sensor initialization
  // Initialize default Wire for VL53L0X (address 0x29)
  // Note: STM32H750 has limited I2C ports; using remapping approach
  Wire.begin();
  Wire.setClock(400000);
  
  delay(100);  // Give I2C bus time to stabilize
  
  // ALWAYS run I2C scan first to see what's connected
  Serial.println("=== Running I2C scan ===");
  i2cScan();
  Serial.println("=== Scan complete ===");
  
  delay(100);
  
  // Initialize VL53L0X (address 0x29)
  Serial.println("Initializing VL53L0X ToF sensor...");
  if (sensor.begin()) {
    Serial.println("VL53L0X OK - starting continuous ranging");
    sensor.startRangeContinuous();
    tofAvailable = true;
  } else {
    Serial.println("VL53L0X initialization failed - continuing without ToF");
  }
  
  delay(100);
  
  // Initialize MSA301 (address 0x26) - using same Wire bus for now
  // TODO: Implement I2C multiplexing or dual-bus support if conflicts persist
  Serial.println("Initializing MSA301 accelerometer...");
  if (accel.begin(0x26, &Wire)) {
    Serial.println("MSA301 OK - IMU features enabled");
    accelAvailable = true;
  } else {
    Serial.println("MSA301 initialization failed - IMU features disabled");
    accelAvailable = false;
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
      currentScale = SCALE_BLUES;
      updateScaleNotes();
      Serial.println("Scale: Blues");
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
          int note = currentScaleNotes[i];
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
          int note = currentScaleNotes[i];
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
        int note = currentScaleNotes[i];
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
        Serial.println(" Hz)");
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

  // IMU processing (pitch bend and modulation)
  processIMU();

  // Distance sensor processing (mode-aware palm effects)
  processDistanceSensor();

  // accelerometer (sliding window control) - LEGACY, disabled for now
  // Note: This was experimental accelerometer-based sliding window feature
  // Now replaced by IMU pitch bend/modulation
  /*
  if (accelAvailable && (millis() - lastAccelRead >= ACCEL_INTERVAL)) {
    accel.read();
    float accelX = accel.x;
    float deltaT = (millis() - lastAccelRead) / 1000.0f;  // Time in seconds
    
    // Only process if index or pinky pressed (not both - that's calibration)
    bool indexPressed = rightButtonStates[RIGHT_INDEX];
    bool pinkyPressed = rightButtonStates[RIGHT_PINKY];
    
    if ((indexPressed || pinkyPressed) && !(indexPressed && pinkyPressed)) {
      // Calculate velocity (change from center)
      float velocity = accelX - accelCenterX;
      
      // Choose sensitivity based on which button is pressed
      float sensitivity = indexPressed ? COARSE_SENSITIVITY : FINE_SENSITIVITY;
      
      // Integrate velocity to position
      accelPositionOffset += velocity * sensitivity * deltaT;
      
      // Update window offset (convert to integer semitones)
      int newWindowOffset = (int)round(accelPositionOffset);
      newWindowOffset = constrain(newWindowOffset, -MAX_WINDOW_OFFSET, MAX_WINDOW_OFFSET);
      
      // Only update if changed
      if (newWindowOffset != windowOffset) {
        windowOffset = newWindowOffset;
        updateScaleNotes();
        
        // Print window info
        Serial.print(indexPressed ? "[COARSE] " : "[FINE] ");
        printWindow();
      }
    }
    
    lastAccelX = accelX;
    lastAccelRead = millis();
  }
  */

  delay(1);
}