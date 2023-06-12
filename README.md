# S14Clock

**2013-06-07** V2.02 common firmware auto-detects and support all (V3, V2-24, V2-12) hardware designs. V3 hardware design eliminates 74HC154 4-to-16 line decoder. V2.02 software adds charliplexing to V2.01 led multiplexing scheme and achieves 14 x 24 (336 segments) direct driving with 26 IO pins.

S14Clock is a bar shaped, web synchronized word clock featuring 24 or 12 characters, 14 segment display.

[Project description](https://simpleavr.github.io/s14clock/index.html)

[Project wiki page, tips on building](https://github.com/simpleavr/s14clock/wiki.html)

[Project discussion page, questions and comments](https://github.com/simpleavr/s14clock/discussions.html)

[Install firmware](https://simpleavr.github.io/s14clock/install.html)

[Unit can be obtained from store139 in tindie](https://www.tindie.com/products/29601/)

Build using Arduino or PlatformIO.

S14Clock relies on Arduino AutoConnect library by Hieromon.
If using Arduino to build, make sure you install the library first.

Package is built under windows 10 with platformio

- git clone https://github.com/simpleavr/s14clock.git
- edit src/main.cpp to include "ver2.h" (12 characters version) or "ver2l.h" (24 characters version)
- cd s14clock
- pio project init --board lolin_s2_mini
- pio lib install AutoConnect
- pio run --target upload

V2.01 firmware (2023-03-20)

- replace "glow" transition with "drip" transition
- enchange shift transition

V2.02 firmware (2023-05-29)

- introduce "flash" transition
- add option to cycle through all transaction modes one after another
- config time with new time zone immediately after change
- autodetect v2, v2l, v3 hardware
- substitute underscore with space for ad-hoc messages
- ad-hoc messages now follows transition in
- implements %-[dmHIMSjuW] tokens in strftime() to suppress leading zeros and spaces
