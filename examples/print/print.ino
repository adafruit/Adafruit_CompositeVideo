// Minimal example for the Adafruit_CompositeVideo library.
// Continually displays value from analog input.
// Written for Adafruit Circuit Playground Express (not 'classic'),
// but can also work on Feather M0, Arduino Zero or similar boards.
// Requires latest Adafruit_GFX and Adafruit_ZeroDMA libraries.
// Gator-clip composite video 'tip' to pin A0, 'ring' to GND.

#include <Adafruit_GFX.h>
#include <Adafruit_CompositeVideo.h>

#define PIN A8 // Light sensor on Circuit Playground Express

Adafruit_NTSC40x24 display; // NTSC 40x24 currently the only supported type

void setup() {
  if(!display.begin()) for(;;);   // Initialize display; halt on failure
}

void loop() {
  display.fillScreen(0);          // Clear display
  display.setCursor(4, 4);        // Inset slightly to avoid overscan area
  display.print(analogRead(PIN)); // Print analog reading
  delay(100);                     // Wait 1/10 sec
}

