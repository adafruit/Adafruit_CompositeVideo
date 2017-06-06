# Adafruit_CompositeVideo
Composite video output from M0 microcontrollers: Circuit Playground Express (not 'classic'), Feather M0, Arduino Zero, etc. Requires latest Adafruit_GFX and Adafruit_ZeroDMA libraries.

Gator-clip composite video 'tip' to pin A0, 'ring' to GND. Handles grayscale NTSC video, 40x24 pixels, usable area may be smaller due to overscan. This is a hack and is NOT guaranteed to work on all composite displays!

Tutorial is located here:
https://learn.adafruit.com/circuit-playground-express-dac-hacks

Uses Timer/Counter 5 and DAC. Speaker output will be disabled. Video is entirely DMA-driven with zero CPU load. Interrupts, delay() and millis(), NeoPixels, etc. are all available. Uses about 11K of RAM.
