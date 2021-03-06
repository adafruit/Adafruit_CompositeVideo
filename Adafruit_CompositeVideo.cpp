/*!
 * @file Adafruit_CompositeVideo.cpp
 *
 * @mainpage DMA-driven composite video library for M0 microcontrollers.
 *
 * @section intro_sec Introduction
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
 * @section dependencies Dependencies
 *
 * This library depends on <a
 * href="https://github.com/adafruit/Adafruit_ZeroDMA">Adafruit_ZeroDMA</a>
 * and <a
 * href="https://github.com/adafruit/Adafruit-GFX-Library">Adafruit_GFX</a>
 * being present on your system. Please make sure you have installed the
 * latest versions before using this library.
 *
 * @section author Author
 *
 * Written by Phil Burgess / Paint Your Dragon for Adafruit Industries.
 *
 * @section license License
 *
 * MIT license, all text above must be included in any redistribution
 *
 */

#include <Adafruit_CompositeVideo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ZeroDMA.h>
#include <malloc.h> // memalign() function

// The DAC has an option for a 1.0 Volt reference selection (exactly what's
// needed for composite video) -- but, BUT -- this is NOT used by default.
// The 'settling time' of the DAC is a function of the prior and new 10-bit
// values regardless of reference voltage.  Full rail-to-rail is too slow
// and produces blurry images and unrelaible sync.  THEREFORE, the default
// analog voltage reference (3.3V) is used, and a smaller window of values
// within the 10-bit range (0-310) corresponds to 0.0 to 1.0 Volts.
// Allowing space for the sync and blank voltages, there are ~220 available
// brightness levels (not 256).  GFX functions take care of brightness
// scaling, so use 0 for black and 255 for white as normal.
//#define DAC_MAX 1023           ///< Use 1.0 V DAC analog ref
#define DAC_MAX (1023 * 10 / 33) ///< Use subset of 3.3V DAC

// Currently only one video format & resolution is supported, and maybe
// that's all that will be implemented.  But if any others happen in the
// future, this is where they'll start, with a unique index here that
// points into videoSpec[] table below.
#define MODE_NTSC40x24 0 ///< NTSC 40x24 pixel mode

static const struct {
  uint16_t timerPeriod;    // CPU ticks per pixel clock (minus 1)
  uint8_t rowPixelClocks;  // # of pixel clocks (NOT visible pixels) per row
  uint8_t xOffset;         // Offset in pixel clocks of first visible pixel
  uint16_t numDescriptors; // Number of DMA descriptors for odd+even fields
} videoSpec[] = {
    60, 50, 9, 436, // MODE_NTSC40x24: F_CPU/61 = ~786,885 Hz, ~1.27 uS
};

// NTSC-SPECIFIC STUFF -----------------------------------------------------

// NTSC sync (NS), blank (N_), black (NK) and white (NW) levels
#if DAC_MAX == 1023
#define IRE(N) (((DAC_MAX * (40 + N)) + 70) / 140)
static const uint16_t NS = IRE(-40), N_ = IRE(0), NK = IRE(8), NW = IRE(100);
#else
// Better (?) sync & ref black voltages:
static const uint16_t NS = 0, N_ = 45, NK = 60, NW = 310;
#endif

// Adafruit_CompositeVideo class -------------------------------------------
//
// User code should not need to instantiate objects of this class.
// It's a parent class for the resolution-specific class(es) that appear
// later in the code.  It's done this way because the Adafruit_GFX
// constructor requires width & height values from the get-go.

// Constructor
Adafruit_CompositeVideo::Adafruit_CompositeVideo(uint8_t mode, int16_t width,
                                                 int16_t height)
    : Adafruit_GFX(width, height), mode(mode), descriptor(NULL) {}

boolean Adafruit_CompositeVideo::begin(void) {
  if (descriptor)
    return true; // Don't double-init!

  // DMA init --------------------------------------------------------------

  dma.setTrigger(TC5_DMAC_ID_OVF);
  dma.setAction(DMA_TRIGGER_ACTON_BEAT);
  if (dma.allocate() != DMA_STATUS_OK)
    return false;

  // Big allocation --------------------------------------------------------

  // DMA descriptor list MUST be 128-bit (16 byte) aligned!
  if (!(descriptor = (DmacDescriptor *)memalign(
            16,
            sizeof(DmacDescriptor) * videoSpec[mode].numDescriptors +
                sizeof(uint16_t) * videoSpec[mode].rowPixelClocks * HEIGHT)))
    return false;

  // Frame buffer follows descriptor list.
  frameBuffer = (uint16_t *)&descriptor[videoSpec[mode].numDescriptors];

  // Timer init ------------------------------------------------------------
  // TC5 is used; this will knock out the Tone library

  GCLK->CLKCTRL.reg = (uint16_t)(GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 |
                                 GCLK_CLKCTRL_ID(GCM_TC4_TC5));
  while (GCLK->STATUS.bit.SYNCBUSY == 1)
    ;

  TC5->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE; // Disable TCx to config it
  while (TC5->COUNT16.STATUS.bit.SYNCBUSY)
    ;

  TC5->COUNT16.CTRLA.reg =     // Configure timer counter
      TC_CTRLA_MODE_COUNT16 |  // 16-bit counter mode
      TC_CTRLA_WAVEGEN_MFRQ |  // Match Frequency mode
      TC_CTRLA_PRESCALER_DIV1; // 1:1 Prescale
  while (TC5->COUNT16.STATUS.bit.SYNCBUSY)
    ;

  TC5->COUNT16.CC[0].reg = videoSpec[mode].timerPeriod;
  while (TC5->COUNT16.STATUS.bit.SYNCBUSY)
    ;

  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE; // Re-enable TCx
  while (TC5->COUNT16.STATUS.bit.SYNCBUSY)
    ;

    // DAC INIT --------------------------------------------------------------

#ifdef ADAFRUIT_CIRCUITPLAYGROUND_M0
  pinMode(11, OUTPUT);
  digitalWrite(11, LOW); // Switch off speaker (DAC to A0 pin only)
#endif
  analogWriteResolution(10); // Let Arduino core initialize the DAC,
  analogWrite(A0, 512);      // ain't nobody got time for that!
#if DAC_MAX == 1023
  DAC->CTRLB.bit.REFSEL = 0; // VMAX = 1.0V
  while (DAC->STATUS.bit.SYNCBUSY)
    ;
#endif

  // DMA transfer job is NOT started here!  That's done in subclass
  // begin() function, after descriptor table is filled.

  return true;
}

// ALL-PURPOSE GFX PIXEL DRAWING FUNCTION ----------------------------------

void Adafruit_CompositeVideo::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (y < 0) || (x >= _width) || (y >= _height))
    return;

  int16_t t;
  switch (rotation) {
  case 1:
    t = x;
    x = WIDTH - 1 - y;
    y = t;
    break;
  case 2:
    x = WIDTH - 1 - x;
    y = HEIGHT - 1 - y;
    break;
  case 3:
    t = x;
    x = y;
    y = HEIGHT - 1 - t;
    break;
  }

  frameBuffer[y * videoSpec[mode].rowPixelClocks + x +
              videoSpec[mode].xOffset] = NK + (color & 0xFF) * (NW - NK) / 255;
}

// NTSC 40x24-SPECIFIC STUFF -----------------------------------------------

// Some pixel clock arrangements for the vertical sync and overscan lines:
// 25 and 50 in this case refer to the total number of pixel clocks per
// line, which includes horizontal sync and overscan.  The available drawable
// raster size is narrower than this (40 pixels).
#define NTSC_EQ_HALFLINE25                                                     \
  NS, NS, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,  \
      N_, N_, N_, N_, N_, N_ ///< One-half vsync scanline
#define NTSC_SERRATION_HALFLINE25                                              \
  NS, NS, NS, NS, NS, NS, NS, NS, NS, NS, NS, NS, NS, NS, NS, NS, NS, NS, NS,  \
      NS, NS, NS, N_, N_, N_ ///< Different one-half vsync scanline
#define NTSC_BLANK_LINE50                                                      \
  NS, NS, NS, NS, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,  \
      N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,  \
      N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_ ///< Full line blank
#define NTSC_EMPTY_LINE50                                                      \
  NS, NS, NS, NS, N_, N_, N_, N_, N_, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK,  \
      NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK,  \
      NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, NK, N_ ///< Full line black

// NTSC sync (NS), blank (N_), black (NK) and white (NW) levels

// Pixel clocking data for the whole odd & even vertical sync periods...
static const uint16_t
    NTSC40x24vsyncOdd[] =
        {
            // These 16 blank lines (510-525) are the bottom of the *prior*
            // (even)
            // field, merged here to save on DMA descriptors:
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50,
            // The vertical blank for odd fields then actually starts here:
            NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25, // Line 1
            NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25,
            NTSC_EQ_HALFLINE25, NTSC_SERRATION_HALFLINE25,
            NTSC_SERRATION_HALFLINE25, NTSC_SERRATION_HALFLINE25,
            NTSC_SERRATION_HALFLINE25, NTSC_SERRATION_HALFLINE25,
            NTSC_SERRATION_HALFLINE25, NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25,
            NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25,
            NTSC_EQ_HALFLINE25, // Line 9
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50, // Lines 10-20
                               // The 11 lines above (10-20) are part of the
                               // vertical blank. Video at top of field could
                               // then start...but the next 10 lines here are
                               // also blank to center the 24-row pixel data
                               // vertically:
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
            NTSC_BLANK_LINE50 // Lines 21-30
                              // Pixel data then occupies lines 31-246 (216
                              // lines; 24*9)
},
    NTSC40x24vsyncEven[] = {
        // These 16 blank lines (247-262) are the bottom of the *prior* (odd)
        // field, merged here to save on DMA descriptors:
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50,
        // Line 263 before vblank is an odd half-line of image and half EQ:
        NS, NS, NS, NS, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_, N_,
        N_, N_, N_, N_, N_, N_, N_, NTSC_EQ_HALFLINE25,
        // Vertical blank for even fields then starts here:
        NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25, // Line 264
        NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25,
        NTSC_SERRATION_HALFLINE25, NTSC_SERRATION_HALFLINE25,
        NTSC_SERRATION_HALFLINE25, NTSC_SERRATION_HALFLINE25,
        NTSC_SERRATION_HALFLINE25, NTSC_SERRATION_HALFLINE25,
        NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25,
        NTSC_EQ_HALFLINE25, NTSC_EQ_HALFLINE25, // Line 271
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, // 272-282
        // Line 283 is another weird half-line at the top of the new field,
        // but since we have no pixel data up this high, a blank line works:
        NTSC_BLANK_LINE50,
        // Next 10 lines (284-293) are blank to v-center the 24-row pixel data:
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50, NTSC_BLANK_LINE50, NTSC_BLANK_LINE50,
        NTSC_BLANK_LINE50
        // Pixel data then occupies lines 294-509 (216 lines; 24*9)
};

// Until if/when proper double-buffering is implemented, this is a
// Kludgey McKludgeface thing for polling for odd/even vertical sync
// events.  Don't get comfortable with it, this is going away.
volatile uint8_t vBlank = 0; ///< Current field index
static const uint8_t vBlank1 = 1, vBlank2 = 2;

// Adafruit_NTSC40x24 class ------------------------------------------------
//
// THIS is what user code instantiates.

// Constructor
Adafruit_NTSC40x24::Adafruit_NTSC40x24()
    : Adafruit_CompositeVideo(MODE_NTSC40x24, 40, 24) {}

// begin() sets up DMA descriptor table
boolean Adafruit_NTSC40x24::begin(void) {
  if (!Adafruit_CompositeVideo::begin())
    return false;

  // FYI, the DMA descriptor table is what uses all the memory here.
  // 436 entries * 20 bytes each = 8720 bytes (+ 1 for word alignment).
  // Framebuffer for NTSC40x24 mode (actually 50 pixel clocks wide)
  // is 50 words (2 bytes ea) * 24 lines = 2400 bytes.
  // 8720 + 1 + 2400 = 11,121 bytes!

  DmacDescriptor *desc;

  for (uint16_t i = 0; i < videoSpec[mode].numDescriptors; i++) {
    desc = &descriptor[i];
    desc->BTCTRL.bit.VALID = true;
    desc->BTCTRL.bit.EVOSEL = DMA_EVENT_OUTPUT_DISABLE;
    desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_NOACT;
    desc->BTCTRL.bit.BEATSIZE = DMA_BEAT_SIZE_HWORD;
    desc->BTCTRL.bit.SRCINC = true;
    desc->BTCTRL.bit.DSTINC = false;
    desc->BTCTRL.bit.STEPSEL = DMA_STEPSEL_DST;
    desc->BTCTRL.bit.STEPSIZE = DMA_ADDRESS_INCREMENT_STEP_SIZE_1;
    desc->DSTADDR.reg = (uint32_t)&DAC->DATA.reg;
    desc->DESCADDR.reg = (uint32_t)&descriptor[i + 1];

    if (i == 0) {
      // Descriptor 0 is the odd field vertical sync
      desc->SRCADDR.reg = (uint32_t)NTSC40x24vsyncOdd;
      desc->BTCNT.reg =
          sizeof(NTSC40x24vsyncOdd) / sizeof(NTSC40x24vsyncOdd[0]);
    } else if (i <= 216) {
      // Descriptors 1-216 are pixel data
      uint8_t row = (i - 1) / 9; // 0 to 23
      desc->SRCADDR.reg =
          (uint32_t)&frameBuffer[row * videoSpec[mode].rowPixelClocks];
      desc->BTCNT.reg = videoSpec[mode].rowPixelClocks;
    } else if (i == 217) {
      // Descriptor 217 is vsync hack (will go away later)
      desc->BTCTRL.bit.BEATSIZE = DMA_BEAT_SIZE_BYTE;
      desc->BTCTRL.bit.SRCINC = false;
      desc->SRCADDR.reg = (uint32_t)&vBlank1;
      desc->BTCNT.reg = 1;
      desc->DSTADDR.reg = (uint32_t)&vBlank;
    } else if (i == 218) {
      // Descriptor 218 is the even field vertical sync
      desc->SRCADDR.reg = (uint32_t)NTSC40x24vsyncEven;
      desc->BTCNT.reg =
          sizeof(NTSC40x24vsyncEven) / sizeof(NTSC40x24vsyncEven[0]);
    } else if (i <= 434) {
      // Descriptors 219-434 are pixel data
      uint8_t row = (i - 219) / 9; // 0 to 23
      desc->SRCADDR.reg =
          (uint32_t)&frameBuffer[row * videoSpec[mode].rowPixelClocks];
      desc->BTCNT.reg = videoSpec[mode].rowPixelClocks;
    } else {
      // Last descriptor (435) is vsync hack (also going away later)
      desc->BTCTRL.bit.BEATSIZE = DMA_BEAT_SIZE_BYTE;
      desc->BTCTRL.bit.SRCINC = false;
      desc->SRCADDR.reg = (uint32_t)&vBlank2;
      desc->BTCNT.reg = 1;
      desc->DSTADDR.reg = (uint32_t)&vBlank;
    }
    if (desc->BTCTRL.bit.SRCINC)
      desc->SRCADDR.reg += 2 * desc->BTCNT.reg;
  }

  // Link last DMA descriptor back to first.  Once the transfer job is
  // started, video generation runs entirely on its own with *zero* CPU
  // intervention!  Interrupts, NeoPixels, all of that runs without harm.
  descriptor[videoSpec[mode].numDescriptors - 1].DESCADDR.reg =
      (uint32_t)&descriptor[0];

  // The DMA library needs to think it's allocated at least one
  // valid descriptor, so we do that here (though it's never used)
  (void)dma.addDescriptor(NULL, NULL, 42, DMA_BEAT_SIZE_BYTE, false, false);

  // Point DMA descriptor base address to our descriptor list
  __disable_irq();
  __DMB();
  DMAC->CTRL.reg = 0; // Disable DMA controller
  DMAC->BASEADDR.bit.BASEADDR = (uint32_t)descriptor;
  DMAC->CTRL.reg = DMAC_CTRL_DMAENABLE | DMAC_CTRL_LVLEN(0xF);
  __DMB();
  __enable_irq();

  clear(); // Initialize frame buffer

  return (dma.startJob() == DMA_STATUS_OK);
}

void Adafruit_NTSC40x24::clear(void) {
  const uint16_t emptyLine[] = {NTSC_EMPTY_LINE50};
  for (uint8_t i = 0; i < 24; i++) {
    memcpy(&frameBuffer[i * videoSpec[mode].rowPixelClocks], &emptyLine,
           sizeof(emptyLine));
  }
}

// Hacky stuff, don't use this
void Adafruit_NTSC40x24::setBlank(uint8_t value) { vBlank = value; }

// Ditto
uint8_t Adafruit_NTSC40x24::getBlank(void) { return vBlank; }
