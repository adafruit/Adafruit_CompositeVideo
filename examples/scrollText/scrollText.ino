// A fancier graphics example for the Adafruit_CompositeVideo library.
// Shows grayscale graphics and scrolls "Hello World" across screen.
// Written for Adafruit Circuit Playground Express (not 'classic'),
// but can also work on Feather M0, Arduino Zero or similar boards.
// Requires latest Adafruit_GFX and Adafruit_ZeroDMA libraries.
// Gator-clip composite video 'tip' to pin A0, 'ring' to GND.

#include <Adafruit_GFX.h>
#include <Adafruit_CompositeVideo.h>
#include <fonts/FreeSerifItalic18pt7b.h>

Adafruit_NTSC40x24 display; // NTSC 40x24 currently the only supported type

// To prevent video "tearing," an offscreen canvas is used for drawing:
GFXcanvas8 canvas(display.width(), display.height());

void setup() {
  if(!display.begin()) for(;;); // Initialize display; halt on failure
  canvas.setTextWrap(false);    // Allow text to flow off right edge
  canvas.setFont(&FreeSerifItalic18pt7b);
}

int x = display.width(); // Text horizontal position

void loop() {
  // DO NOT get too comfortable with this, it's a hack and will be replaced
  // with a better method in the future!  For now, this polls (waits) until
  // an odd-numbered field vertical blank is occurring...
  while(display.getBlank() != 1);
  display.setBlank(0);

  // During (or near) vertical blanking, copy offscreen canvas to display:
  display.drawGrayscaleBitmap(0, 0,
    canvas.getBuffer(), display.width(), display.height());

  // Then draw next frame in the offscreen canvas...
  for(uint8_t y=0; y<canvas.height(); y++)
    canvas.drawLine(0, y, canvas.width() - 1, y, y * 5); // Gradient

  // Draw text shadow 1 pixel down/right from text
  canvas.setCursor(x + 1, canvas.height());
  canvas.setTextColor(0);
  canvas.print("Hello World");

  // Draw text on top of shadow
  canvas.setCursor(x, canvas.height() - 1);
  canvas.setTextColor(255);
  canvas.print("Hello World");

  // Move text position one pixel to left.
  // If it's gone entirely off the left side, reset to the far right.
  if(--x < -170) x = display.width();
}

