#include "DaisyDuino.h"
#include <Adafruit_VL53L0X.h>

DaisyHardware hw;
Oscillator osc;

// Volume pot
const int VOLUME_PIN = A7;
const int VOLUME_CHANGE_THRESHOLD = 10; // keep volume reading from being too noisy
int lastVolumeRaw = -1;

// Buttons
const int NUM_BUTTONS = 9;
Switch button[NUM_BUTTONS];
int buttonPins[NUM_BUTTONS] = {21, 20, 19, 18, 17, 16, 15, 0, 1};

// Distance sensor
Adafruit_VL53L0X sensor = Adafruit_VL53L0X();
const int DISTANCE_CHANGE_THRESHOLD = 5; // mm
int lastDistance = -1;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 50;

// Audio
float volume = 0.3f;
bool isPlaying = false;
float frequencies[] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f, 440.00f, 493.88f, 523.25f}; // C major scale

void AudioCallback(float **in, float **out, size_t size) {
  for (size_t i = 0; i < size; i++) {
    float sig = 0.0f;
    
    if (isPlaying) {
      sig = osc.Process() * volume;
    }
    
    out[0][i] = sig; // Left channel
    out[1][i] = sig; // Right channel
  }
}


void setup() {
  Serial.begin(115200);

  // Initialize Daisy hardware for audio
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  float sample_rate = DAISY.get_samplerate();

  // Initialize oscillator
  osc.Init(sample_rate);
  osc.SetWaveform(Oscillator::WAVE_SIN);
  osc.SetFreq(440.0f);
  osc.SetAmp(1.0f);

  // Start audio processing
  DAISY.begin(AudioCallback);

  // volume pot
  pinMode(VOLUME_PIN, INPUT);

  // buttons
  // update at 1kHz, no invert
  for (int i = 0; i < NUM_BUTTONS; i++) {
    button[i].Init(1000, true, buttonPins[i], INPUT_PULLUP);
  }

  // distance sensor
  Serial.println("Adafruit VL53L0X test.");
  if (!sensor.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while(1);
  }
  // power
  Serial.println(F("VL53L0X API Continuous Ranging example\n\n"));

  // start continuous ranging
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
  isPlaying = false;

  for (int i = 0; i < NUM_BUTTONS; i++) {
    // Debounce the buttons
    button[i].Debounce();

    if (button[i].Pressed()) {
      // Set frequency and start playing
      osc.SetFreq(frequencies[i]);
      isPlaying = true;

      Serial.print("Button ");
      Serial.print(i + 1);
      Serial.print(" Pressed - Note: ");
      Serial.print(frequencies[i]);
      Serial.println(" Hz");
    }
  }

  // distance sensor
    if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    if (sensor.isRangeComplete()) {
      int distance = sensor.readRange();
      
      if (abs(distance - lastDistance) > DISTANCE_CHANGE_THRESHOLD) {
        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.println(" mm");
        lastDistance = distance;
      }
    }
    lastSensorRead = millis();
  }

  // wait 1 ms
  delay(1);
}