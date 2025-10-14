#include "DaisyDuino.h"
#include <Adafruit_VL53L0X.h>
#include <Wire.h>

DaisyHardware hw;
Oscillator osc[8];
Overdrive distortion;

// Volume pot
const int VOLUME_PIN = A5;
const int VOLUME_CHANGE_THRESHOLD = 10; // keep volume reading from being too noisy
int lastVolumeRaw = -1;

//////////////
// Left hand
//////////////

// Distance sensor
Adafruit_VL53L0X sensor = Adafruit_VL53L0X();
const int DISTANCE_CHANGE_THRESHOLD = 5; // mm
int lastDistance = -1;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 50;
bool tofAvailable = false;

//Buttons
const int NUM_LEFT_BUTTONS = 5;
Switch leftButton[NUM_LEFT_BUTTONS];
int leftButtonPins[NUM_LEFT_BUTTONS] = {8, 9, 10, 13, 14};
bool leftButtonStates[NUM_LEFT_BUTTONS] = {false};
bool leftButtonPrevStates[NUM_LEFT_BUTTONS] = {false};

///////////////
// Right hand
///////////////

// Buttons
// Vars
const int NUM_RIGHT_BUTTONS = 5;
Switch rightButton[NUM_RIGHT_BUTTONS];
int rightButtonPins[NUM_RIGHT_BUTTONS] = {15, 16, 17, 18, 19};
bool rightButtonStates[NUM_RIGHT_BUTTONS] = {false};
bool rightButtonPrevStates[NUM_RIGHT_BUTTONS] = {false};
// Mapping
enum RightHandButtons {
  RIGHT_THUMB = 4,    // SHIFT key
  RIGHT_INDEX = 3,    // Ocatave up
  RIGHT_MIDDLE = 2,   // Momentary flat
  RIGHT_RING = 1,     // Used in combos
  RIGHT_PINKY = 0     // Octave down
};

//////////////////////
// Musical structure
/////////////////////
// Audio
float volume = 0.3f;
float distortionAmount = 0.0f;

// Scale definitions
int currentOctave = 4; // Start in the middle of the octave range
int currentKey = 0; // C major
enum ScaleType {
  SCALE_MAJOR_PENTATONIC = 0,
  SCALE_BLUES = 1,
  SCALE_CHROMATIC = 2
};

int currentScale = SCALE_MAJOR_PENTATONIC;

// Scale intervals (as semitones from root)
int majorPentatonic[] = {0, 2, 4, 7, 9}; // C, D, E, G, A
int bluesScale[] = {0, 3, 5, 6, 7}; // C, Eb, F, F#, G
int chromaticScale[] = {0, 1, 2, 3, 4}; // C, C#, D, D#, E

int currentScaleNotes[5]; // midi note numbers

/////////////////////
// Additional setup
////////////////////
void i2cScan() {
  Serial.println("I2c scan starting...");
  byte count = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("I2C device found at 0x");
      Serial.println(addr, HEX);
      count++;
      delay(1);
    }
  }
  if (count == 0) {
    Serial.println("No I2C devices found.");
  } else {
    Serial.print("Total I2C devices found: ");
    Serial.println(count);
  }
};

// Playing modes
enum PlayMode {
  MODE_SINGLE_NOTE = 0,
  MODE_MAJOR_CHORD = 1,
  MODE_MINOR_CHORD = 2
};

int currentMode = MODE_SINGLE_NOTE;
bool latchMode = false;

void updateScaleNotes() { // midi values
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

void AudioCallback(float **in, float **out, size_t size) {
  for (size_t i = 0; i < size; i++) {
    float mixedSig = 0.0f;
    
    // mix oscillators
    for (int j = 0; j < NUM_LEFT_BUTTONS; j++) {
      if (leftButtonStates[j]) {
        mixedSig += osc[j].Process();
      }
    }

    // add distortion
    if (distortionAmount > 0.0f && mixedSig != 0.0f) {
      distortion.SetDrive(distortionAmount);
      mixedSig = distortion.Process(mixedSig);
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

  // init oscillator
  for (int i = 0; i < NUM_LEFT_BUTTONS; i++) {
    osc[i].Init(sample_rate);
    osc[i].SetWaveform(Oscillator::WAVE_SIN);
    osc[i].SetAmp(1.0f);
  }

  distortion.Init();  // init distortion
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
      currentOctave = constrain(currentOctave + 1, 1, 8);
      updateScaleNotes();
      Serial.print("Octave: ");
      Serial.println(currentOctave);
    }
    // octave down
    if (pinkyPressed && !rightButtonPrevStates[RIGHT_PINKY]) {
      currentOctave = constrain(currentOctave - 1, 1, 8);
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
      // Toggle note on each press; ignore release
      if (rising) {
        leftButtonStates[i] = !leftButtonStates[i];
        if (leftButtonStates[i]) {
          int note = currentScaleNotes[i];
          osc[i].SetFreq(mtof(note));
          Serial.print("Note ON - Button ");
          Serial.print(i + 1);
          Serial.print(", MIDI Note: ");
          Serial.print(note);
          Serial.print(" (");
          Serial.print(mtof(note));
          Serial.println(" Hz)");
        } else {
          Serial.print("Note OFF - Button ");
          Serial.println(i + 1);
        }
      }
    } else {
      // Normal: press = ON, release = OFF
      if (rising) {
        leftButtonStates[i] = true;
        int note = currentScaleNotes[i];
        osc[i].SetFreq(mtof(note));
        Serial.print("Note ON - Button ");
        Serial.print(i + 1);
        Serial.print(", MIDI Note: ");
        Serial.print(note);
        Serial.print(" (");
        Serial.print(mtof(note));
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
    volume = (volumeRaw / 1023.0) * 0.5f; // Scale to 0-0.5 for safe volume
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
          case MODE_SINGLE_NOTE:
            // distortion
            distortionAmount = map(constrain(distance, 50, 300), 50, 300, 50, 0) / 100.0f;
            Serial.print("Distance: ");
            Serial.print(distance);
            Serial.print(" mm - Distortion: ");
            Serial.print(distortionAmount * 100);
            Serial.println("%");
            break;
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