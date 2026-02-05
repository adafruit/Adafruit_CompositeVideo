/*!
 * @file Adafruit_CompositeVideo.h
 *
 * DMA-driven composite video library for M0 microcontrollers
 * (Circuit Playground Express, Feather M0, Arduino Zero, etc.).
 * Gator-clip composite video 'tip' to pin A0, 'ring' to GND.
 * Handles grayscale NTSC video, 40x24 pixels, usable area may be smaller
 * due to overscan.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * Written by Phil Burgess / Paint Your Dragon for Adafruit Industries.
 *
 * MIT license, all text above must be included in any redistribution
 *
 */

#ifndef _ADAFRUIT_COMPOSITEVIDEO_H_
#define _ADAFRUIT_COMPOSITEVIDEO_H_

#include <Adafruit_GFX.h>
#include <Adafruit_ZeroDMA.h>

/**
 * @brief  Class for generating composite video from a M0 microcontroller,
 *         providing bitmapped low-resolution grayscale graphics.
 */
class Adafruit_CompositeVideo : public Adafruit_GFX {
 public:
  /**
   * @brief  Construct a new Adafruit_CompositeVideo object.
   * @param  mode    Video mode, must be 0 (NTSC, others might be added
   *                 in the future.
   * @param  width   Framebuffer width in pixels, must be 40 (ditto).
   * @param  height  Framebuffer height in pixels, must be 24 (same).
   */
  Adafruit_CompositeVideo(uint8_t mode, int16_t width, int16_t height);

  /**
   * @brief  Call to begin composite video output.
   * @return bool  true on success, false on failure (allocation error).
   */
  boolean begin(void);

  /**
   * @brief  Pixel-drawing function for Adafruit_GFX.
   * @param  x      Pixel column (0 = left edge, unless rotation used).
   * @param  y      Pixel row (0 = top edge, unless rotation used).
   * @param  color  Pixel brightness, 0 (black) to 255 (white).
   */
  void drawPixel(int16_t x, int16_t y, uint16_t color);

 protected:
  const uint8_t mode;         ///< Video mode, only MODE_NTSC40x24 right now
  Adafruit_ZeroDMA dma;       ///< SAMD DMA object
  DmacDescriptor* descriptor; ///< DMA descriptor list
  uint16_t* frameBuffer;      ///< Pixel data
};

/**
 * @brief  Class for generating 40x24 pixel grayscale NTSC video, using
           Adafruit_CompositeVideo.
 */
class Adafruit_NTSC40x24 : public Adafruit_CompositeVideo {
 public:
  /**
   * @brief Construct a new Adafruit_NTSC40x24 object.
   */
  Adafruit_NTSC40x24();

  /**
   * @brief  Call to begin NTSC video output.
   * @return bool  true on success, false on failure (allocation error).
   */
  boolean begin(void);

  /**
   * @brief  Clear framebuffer; set all pixels to 0 (black).
   */
  void clear(void);

  /**
   * @brief  Set current field number, used in kludgey vertical blank
   *         synchronization. Will be deprecated in future when double-
   *         buffering is handled.
   * @param  value  Typically 0 to indicate nonsense field number,
   *                then getBlank() is used to poll for a specific field.
   */
  void setBlank(uint8_t value);

  /**
   * @brief   Get current NTSC field number. Will be deprecated in future
   *          when double-buffering is handled.
   * @return  uint8_t  1 if odd-numbered field, 2 if even-numbered.
   */
  uint8_t getBlank(void);
};

#endif // _ADAFRUIT_COMPOSITEVIDEO_H_
