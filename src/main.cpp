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
#include <Wire.h>

DaisyHardware hw;
Oscillator oscSine[5];  // Sine oscillators for each button
Oscillator oscTri[5];   // Triangle oscillators for each button

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
const int DISTANCE_CHANGE_THRESHOLD = 5;      // Minimum change in mm to process
const int DISTANCE_MIN = 50;                  // Minimum distance for mapping (mm)
const int DISTANCE_MAX = 300;                 // Maximum distance for mapping (mm)
const unsigned long SENSOR_INTERVAL = 50;     // Poll interval in ms
int lastDistance = -1;
unsigned long lastSensorRead = 0;
bool tofAvailable = false;

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
  MODE_MAJOR_CHORD = 1,     // Reserved for future chord implementation
  MODE_MINOR_CHORD = 2      // Reserved for future chord implementation
};
int currentMode = MODE_SINGLE_NOTE;
bool latchMode = false;             // When true, buttons latch notes ON

void clearAllLatchedNotes() {
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    leftButtonStates[i] = false;
  }
  Serial.println("All latched notes cleared");
}

/**
 * Update the current scale notes based on octave, key, and scale type
 * Calculates MIDI note numbers for each of the 5 buttons
 */
void updateScaleNotes() {
  int baseNote = (currentOctave * 12) + currentKey;

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

void AudioCallback(float **in, float **out, size_t size) {
  for (size_t i = 0; i < size; i++) {
    float mixedSig = 0.0f;
    
    // mix oscillators with crossfade
    for (int j = 0; j < NUM_LEFT_BUTTONS; j++) {
      if (leftButtonStates[j]) {
        float sineSig = oscSine[j].Process() * sineAmp;
        float triSig = oscTri[j].Process() * triAmp;
        mixedSig += sineSig + triSig;
      }
    }
      
    // set volume
    mixedSig *= volume * 0.3f; // lower volume with multiple notes
    
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
    oscTri[i].SetAmp(0.0f);  // Start with triangle silent
  }

  DAISY.begin(AudioCallback); // start audio processing
  pinMode(VOLUME_PIN, INPUT); // volume pot

  // distance sensor
  // Wire.setSDA(13);  // I2C4 SDA on Daisy Seed
  // Wire.setSCL(14);  // I2C4 SCL on Daisy Seed
  Wire.begin();
  Wire.setClock(400000);
  
  // ALWAYS run I2C scan first to see what's connected
  Serial.println("=== Running I2C scan ===");
  i2cScan();
  Serial.println("=== Scan complete ===");
  
  Serial.println("Adafruit VL53L0X init...");
  if (sensor.begin()) {
      Serial.println("VL53L0X OK - starting continuous ranging");
      sensor.startRangeContinuous();
      tofAvailable = true;
    } else {
      Serial.println("Failed to boot VL53L0X - continuing without ToF");
      Serial.println("Tip: Verify sensor is wired to D11(SDA) and D12(SCL) for I2C1");
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
  // single button actions
  else {
    // octave up
    if (indexPressed && !rightButtonPrevStates[RIGHT_INDEX]) {
      currentOctave = constrain(currentOctave + 1, OCTAVE_MIN, OCTAVE_MAX);
      updateScaleNotes();
      Serial.print("Octave: ");
      Serial.println(currentOctave);
    }
    // octave down
    if (pinkyPressed && !rightButtonPrevStates[RIGHT_PINKY]) {
      currentOctave = constrain(currentOctave - 1, OCTAVE_MIN, OCTAVE_MAX);
      updateScaleNotes();
      Serial.print("Octave: ");
      Serial.println(currentOctave);
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
          Serial.print("Note LATCHED - Button ");
          Serial.print(i + 1);
          Serial.print(", MIDI Note: ");
          Serial.print(note);
          Serial.print(" (");
          Serial.print(freq);
          Serial.println(" Hz)");
        } else {
          // Note already latched, re-trigger it
          int note = currentScaleNotes[i];
          float freq = mtof(note);
          oscSine[i].SetFreq(freq);
          oscTri[i].SetFreq(freq);
          oscSine[i].Reset();
          oscTri[i].Reset();
          Serial.print("Note RE-TRIGGERED - Button ");
          Serial.println(i + 1);
        }
      }
      // Ignore release in latch mode
    } else {
      // Normal: press = ON, release = OFF
      if (rising) {
        leftButtonStates[i] = true;
        int note = currentScaleNotes[i];
        float freq = mtof(note);
        oscSine[i].SetFreq(freq);
        oscTri[i].SetFreq(freq);
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

  // distance sensor
  if (tofAvailable && (millis() - lastSensorRead >= SENSOR_INTERVAL)) {
    if (sensor.isRangeComplete()) {
      int distance = sensor.readRange();
      
      if (abs(distance - lastDistance) > DISTANCE_CHANGE_THRESHOLD) {
        switch (currentMode) {
          case MODE_SINGLE_NOTE: {
            // waveform crossfading: triangle when close, sine when far
            waveformBlend = map(constrain(distance, DISTANCE_MIN, DISTANCE_MAX), 
                               DISTANCE_MIN, DISTANCE_MAX, 100, 0) / 100.0f;
            
            // Equal-power crossfade to maintain constant perceived volume
            // Uses square root curves so that sine²(x) + cosine²(x) = 1
            float blendRadians = waveformBlend * (PI / 2.0f);  // 0 to π/2
            triAmp = sinf(blendRadians);      // 0.0 to 1.0 (curved)
            sineAmp = cosf(blendRadians);     // 1.0 to 0.0 (curved)
            
            // Update amplitudes for all oscillator pairs
            for (int j = 0; j < NUM_LEFT_BUTTONS; j++) {
              oscSine[j].SetAmp(sineAmp);
              oscTri[j].SetAmp(triAmp);
            }
            
            Serial.print("Distance: ");
            Serial.print(distance);
            Serial.print(" mm - Blend: Sine ");
            Serial.print(sineAmp * 100, 0);
            Serial.print("% / Tri ");
            Serial.print(triAmp * 100, 0);
            Serial.println("%");
            break;
          }
          case MODE_MAJOR_CHORD:
            // arpeggiator or strum 
            Serial.print("Distance: ");
            Serial.print(distance);
            Serial.println(" mm - chord effect");
            break;
          case MODE_MINOR_CHORD:
            // arpeggiator or strum 
            Serial.print("Distance: ");
            Serial.print(distance);
            Serial.println(" mm - chord effect");
            break;
        }
        lastDistance = distance;
      }
    }
    lastSensorRead = millis();
  }

  delay(1);
}