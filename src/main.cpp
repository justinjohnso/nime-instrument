#include "DaisyDuino.h"
#include <Adafruit_VL53L0X.h>

// Volume pot
const int VOLUME_PIN = A7;
const int VOLUME_CHANGE_THRESHOLD = 10; // keep volume reading from being too noisy
int lastVolumeRaw = -1;

// Buttons
const int NUM_BUTTONS = 7;
Switch button[NUM_BUTTONS];
int buttonPins[NUM_BUTTONS] = {21, 20, 19, 18, 17, 16, 15};

// Distance sensor
Adafruit_VL53L0X sensor = Adafruit_VL53L0X();
const int DISTANCE_CHANGE_THRESHOLD = 5; // mm
int lastDistance = -1;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 50;

void setup() {
  Serial.begin(115200);

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
    float volumePercent = (volumeRaw / 1023.0) * 100; 
    Serial.print("Volume: ");
    Serial.print(volumePercent, 1);
    Serial.println("%");
    lastVolumeRaw = volumeRaw;
  }

  // buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    // Debounce the buttons
    button[i].Debounce();

    if (button[i].Pressed()) {
      Serial.print("Button ");
      Serial.print(i + 1);  // +1 so buttons are numbered 1-7 instead of 0-6
      Serial.println(" Pressed");
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