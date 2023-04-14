# S14Clock
S14Clock is a bar shaped, web synchronized word clock featuring 24 or 12 characters, 14 segment display.

[Detail project description](https://simpleavr.github.io/s14clock/index.html)

[Project wiki page, tips on building](https://simpleavr.github.io/s14clock/wiki.html)

[Project discussion page, questions and comments](https://simpleavr.github.io/s14clock/discussions.html)

[Unit can be obtained from store139 in tindie](https://www.tindie.com/products/29601/)

Platformio is used to build, Arduino should also work.

S14Clock relies on Arduino AutoConnect library by Hieromon.

Package is built under windows 10 with platformio
- git clone https://github.com/simpleavr/s14clock.git
- edit src/main.cpp to include "ver2.h" (12 characters version) or "ver2l.h" (24 characters version)
- cd s14clock
- pio project init --board lolin_s2_mini
- pio lib install AutoConnect
- pio run --target upload

V2.1 firmware (2020-03-20)
- replace "glow" transition with "drip" transition
- enchange shift transition

