// -------------------------------------------------------------------------
// DMA-driven composite video library for M0 microcontrollers
// (Circuit Playground Express, Feather M0, Arduino Zero, etc.).
// Gator-clip composite video 'tip' to pin A0, 'ring' to GND.
// Handles grayscale NTSC video, 40x24 pixels, usable area may be smaller
// due to overscan.
//
// Written by Phil Burgess / Paint Your Dragon for Adafruit Industries.
// Adafruit invests time and resources providing this open source code,
// please support Adafruit and open-source hardware by purchasing
// products from Adafruit!
//
// MIT license, all text above must be included in any redistribution
// -------------------------------------------------------------------------

#ifndef _ADAFRUIT_COMPOSITEVIDEO_H_
#define _ADAFRUIT_COMPOSITEVIDEO_H_

#include <Adafruit_ZeroDMA.h>
#include <Adafruit_GFX.h>

class Adafruit_CompositeVideo : public Adafruit_GFX {
 public:
  Adafruit_CompositeVideo(uint8_t mode, int16_t width, int16_t height);
  boolean begin(void);
  void    drawPixel(int16_t x, int16_t y, uint16_t color);
 protected:
  const uint8_t     mode;
  Adafruit_ZeroDMA  dma;
  DmacDescriptor   *descriptor;
  uint16_t         *frameBuffer;
};

class Adafruit_NTSC40x24 : public Adafruit_CompositeVideo {
 public:
  Adafruit_NTSC40x24();
  boolean begin(void);
  void    clear(void);
  void    setBlank(uint8_t value);
  uint8_t getBlank(void);
};

#endif // _ADAFRUIT_COMPOSITEVIDEO_H_
