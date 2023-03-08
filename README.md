# S14Clock
S14Clock is a bar shaped, web synchronized word clock featuring 24 or 12 characters, 14 segment display.

Platformio is used to build, Arduino should also work.

S14Clock relies on Arduino AutoConnect library by Hieromon.

Package is built under windows 10 with platformio
- git clone https://github.com/simpleavr/s14clock.git
- cd s14clock
- pio project init --board lolin_s2_mini
- pio lib install AutoConnect
- pio run --target upload
