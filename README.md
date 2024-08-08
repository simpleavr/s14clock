# S14Clock

[Install v2.04 beta firmware, pick firmware beta](https://simpleavr.github.io/s14clock/install.html)

**2024-05-02** change 'alternate transition' token from '\~=' to '\~!'. '\~=' be used in new "exact date" macro, '\~=0501?HAPPY MAYDAY'. Implement alternate transition for "flip" to flip also spaces.  Implement alternate transition for "flash" to use alternate sequence.

**2023-12-10** Implements "alternate transition"" macro, `'\~='`. I.e. double-blink for **Blink** transition, left-to-right for **Shift** and **Drip** transitions, and alternate sequence for other transitions.

**2023-12-07** Implements "day counting" macro, `'\~<MMDD? DAYS TO ..'` and `'\~>MMDD? DAYS SINCE ..'`, apart from MMDD, YYMMDD and YYYYMMDD also accepted. Capable of counting up to 40+ years due to datatype constraint. When using MMDD, system assumes past, current, or next year as reference automatically.

[Install latest firmware, pick v2.03](https://simpleavr.github.io/s14clock/install.html)

**2023-11-27** NTP server and POSIX TZ configuration changed from using 8th content slot to a new storage space. The new fields are 40 characters wide and
should be able to hold the extra long TZ strings.

**2023-11-08** V2.03 Add optional POSIX TZ configuration support. This will allow daylight saving time configurations (eg. EST5EDT,M3.2.0,M11.1.0).  Enter POSIX TZ string as the 8th "Additional Content" text, prefixed with '@' character, and make sure the "Use" checkbox is off. Example enter "@EST5EDT" as the 8th display addn text. Configuration web page will now also detect geo-location and suggest a proper TZ value to use. Feature available in bin/firmware_203.bin

**2023-07-25** Implement new content tokens `'\~+dd'` and `'\~-dd'` to adjust current time by adding / subtracting hours, useful for multi-city display.

**2023-06-25** Implement configurable NTP server, to configure, enter overriding NTP server url as the 8th "Display Content" text, prefixed with '@' character, and make sure the "Use" checkbox is off. Example enter "@time2.google.com" as the 8th display content text.

**2023-06-12** Added to main branch are 2 startup options, press-n-hold button '0' during "0000..." test screen allow resetting WIFI credentials, pressing button '0' during "FW... HW.." version display enters Burn-In mode without setup, which excercise the time showing routines without WIFI.

**2023-06-07** V2.02 common firmware auto-detects and support all (V3, V2-24, V2-12) hardware designs. V3 hardware design eliminates 74HC154 4-to-16 line decoder. V2.02 software adds charliplexing to V2.01 led multiplexing scheme and achieves 14 x 24 (336 segments) direct driving with 26 IO pins.

S14Clock is a bar shaped, web synchronized word clock featuring 24 or 12 characters, 14 segment display.

[Project description](https://simpleavr.github.io/s14clock/index.html)

[Project wiki page, tips on building](https://github.com/simpleavr/s14clock/wiki.html)

[Project discussion page, questions and comments](https://github.com/simpleavr/s14clock/discussions.html)

[Install firmware](https://simpleavr.github.io/s14clock/install.html)

[Unit can be obtained from store139 in tindie](https://www.tindie.com/products/29601/)

Build using Arduino or PlatformIO.

S14Clock relies on Arduino AutoConnect library by Hieromon.
If using Arduino to build, make sure you install the library first.

[Download as **.zip**](https://github.com/simpleavr/s14clock/archive/refs/heads/main.zip) or clone the project with `git clone https://github.com/simpleavr/s14clock.git`

## Building using Arduino

- Open the folder `s14clock_firmware`, double click `s14clock_firmware.ino` and it will open in the Arduino IDE
- if you don't yet have support for ESP32 follow [the instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) to install the platform
- go to Library Manager, search for "AutoConnect" and install the resulting library by "Hieromon Ikasamo"
- select the board ESP32 > ESP32S2 Dev Module 
- select the port it is connected to
- press Verify
- if it verifies correctly press Upload

## Building with PlatformIO

- ~~edit src/main.cpp to include "ver2.h" (12 characters version) or "ver2l.h" (24 characters version)~~
- cd s14clock
- pio project init --board lolin_s2_mini
- pio lib install AutoConnect
- pio run --target upload

Once installed, follow the guide hosted at the [project description](https://simpleavr.github.io/s14clock/index.html)

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

V2.03 firmware (2023-11-27)

- implement new content tokens '\~+dd' and '\~-dd' to adjust current time, useful for multi-city display.
- allows NTP server override and POSIX TZ support, allowing standard and daylight saving time switching.

