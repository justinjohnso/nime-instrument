#include "DaisyDuino.h"
#include <Adafruit_VL53L0X.h>

DaisyHardware hw;
Oscillator osc[8];
Overdrive distortion;

// Volume pot
const int VOLUME_PIN = A0;
const int VOLUME_CHANGE_THRESHOLD = 10; // keep volume reading from being too noisy
int lastVolumeRaw = -1;

// Buttons
const int NUM_BUTTONS = 7;
const int NUM_NOTE_BUTTONS = 5;
const int EFFECT_TOGGLE_BUTTON = 5;
Switch button[NUM_BUTTONS];
int buttonPins[NUM_BUTTONS] = {18, 20, 24, 26, 22, 16, 28};
bool buttonStates[NUM_BUTTONS] = {false};

// Distance sensor
Adafruit_VL53L0X sensor = Adafruit_VL53L0X();
const int DISTANCE_CHANGE_THRESHOLD = 5; // mm
int lastDistance = -1;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 50;

// Audio
float volume = 0.3f;
float frequencies[] = {261.63f, 293.66f, 329.63f, 392.00f, 440.00f}; // C major pentatonic: C, D, E, G, A
// float frequencies[] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f, 440.00f, 493.88f, 523.25f}; // C major scale
float distortionAmount = 0.0f;

void AudioCallback(float **in, float **out, size_t size) {
  for (size_t i = 0; i < size; i++) {
    float mixedSig = 0.0f;
    
    // mix oscillators - ONLY for the first 8 buttons
    for (int j = 0; j < NUM_NOTE_BUTTONS; j++) {
      if (buttonStates[j]) {
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

  // init Daisy hardware
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  float sample_rate = DAISY.get_samplerate();

  // init oscillators - only for the first 8 buttons
  for (int i = 0; i < NUM_NOTE_BUTTONS; i++) {
    osc[i].Init(sample_rate);
    osc[i].SetWaveform(Oscillator::WAVE_SIN);
    osc[i].SetFreq(frequencies[i]);
    osc[i].SetAmp(1.0f);
  }

  // init distortion
  distortion.Init();

  // Start audio processing
  DAISY.begin(AudioCallback);

  // volume pot
  pinMode(VOLUME_PIN, INPUT);

  // buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    button[i].Init(1000, true, buttonPins[i], INPUT_PULLUP);
  }

  // distance sensor
  Serial.println("Adafruit VL53L0X test.");
  if (!sensor.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while(1);
  }
  Serial.println(F("VL53L0X API Continuous Ranging example\n\n"));
  sensor.startRangeContinuous();
}

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

  // buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    button[i].Debounce();
    bool currentState = button[i].Pressed();

    if (currentState && !buttonStates[i]) {
      // button pressed
      buttonStates[i] = true;
      
      if (i < NUM_NOTE_BUTTONS) {
        // Only print note info for the first 8 buttons
        Serial.print("Button ");
        Serial.print(i + 1);
        Serial.print(" ON - Note: ");
        Serial.print(frequencies[i]);
        Serial.println(" Hz");
      } else {
        // Buttons 9 and 10 - you can use these for other functions
        Serial.print("Button ");
        Serial.print(i + 1);
        Serial.println(" ON - Control button");
      }
      
    } else if (!currentState && buttonStates[i]) {
      // button released
      buttonStates[i] = false;
      Serial.print("Button ");
      Serial.print(i + 1);
      Serial.println(" OFF");
    }
  }

  // distance sensor
  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    if (sensor.isRangeComplete()) {
      int distance = sensor.readRange();
      
      if (abs(distance - lastDistance) > DISTANCE_CHANGE_THRESHOLD) {
        distortionAmount = map(constrain(distance, 50, 300), 50, 300, 50, 0) / 100.0f;
        
        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.print(" mm - Distortion: ");
        Serial.print(distortionAmount * 100);
        Serial.println("%");
        lastDistance = distance;
      }
    }
    lastSensorRead = millis();
  }

  delay(1);
}