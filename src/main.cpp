// Title: Button
// Description: Turn an led on and off with a button
// Hardware: DaisyHardware
// Author: Ben Sergentanis
// Breadboard:
// https://raw.githubusercontent.com/electro-smith/DaisyExamples/master/seed/Button/resources/Button_bb.png
// Schematic:
// https://raw.githubusercontent.com/electro-smith/DaisyExamples/master/seed/Button/resources/Button_schem.png

#include "DaisyDuino.h"

const int VOLUME_PIN = A7;
int lastVolumeRaw = -1;

const int NUM_BUTTONS = 7;
Switch button[NUM_BUTTONS];
int buttonPins[NUM_BUTTONS] = {21, 20, 19, 18, 17, 16, 15};

void setup() {
  // setup the button
  // update at 1kHz, no invert
  for (int i = 0; i < NUM_BUTTONS; i++) {
    button[i].Init(1000, true, buttonPins[i], INPUT_PULLUP);
  }

  // volume pot
  pinMode(VOLUME_PIN, INPUT);

  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
}
void loop() {
  int volumeRaw = analogRead(VOLUME_PIN);
  
  if (volumeRaw != lastVolumeRaw); {
    float volumePercent = (volumeRaw / 1023.0) * 100; 
    Serial.print("Volume: ");
    Serial.print(volumePercent, 1);
    Serial.println("%");
    lastVolumeRaw = volumeRaw;
  }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    // Debounce the buttons
    button[i].Debounce();

    if (button[i].Pressed()) {
      Serial.print("Button ");
      Serial.print(i + 1);  // +1 so buttons are numbered 1-7 instead of 0-6
      Serial.println(" Pressed");
    }
  }

  // wait 1 ms
  delay(1);
}