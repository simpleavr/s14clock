//
// this package is built under windows 10 with platformio
//
// git clone https://github.com/simpleavr/s14clock.git
// cd s14clock
// (pio project init --board lolin_s2_mini) may need to
// (pio lib install AutoConnect) may need to
// pio run --target upload
// pio run -t nobuild -t upload
//
/*
 S14Clock is a bar shaped, web synchronized word clock featuring 24 or 12 characters, 14 segment display
 platformio is used to build
 relies on Arduino AutoConnect library by Hieromon
 */
// c2303 add support for vertical numerics
// c2303 change "glow" transition to "drip"
//
//#define USE_WIFI			// comment out to test display only


#include <WiFi.h>
#include <WebServer.h>
#include <uri/UriRegex.h>
#include <time.h>
#include <AutoConnect.h>

#ifdef USE_WIFI
WebServer 			server;
AutoConnect      	portal(server);
AutoConnectConfig   Config("esp32ap", "12345678");
#endif

#define NTPServer1  "pool.ntp.org"
#define NTPServer2  "time1.google.com"

#include "stdint.h"
// available hardware versions, headers containing IO mapping
//#include "ver1.h"
#include "ver2.h"
//#include "ver2l.h"

#define _LED		LED_BUILTIN
#define _BT2		0

//#include <type_traits>
//#include <map>
//#include <Preferences.h>
//#include <nvs.h>

hw_timer_t *_timer=0;

Preferences prefs;
uint8_t _brightness = 4;

#define DISP_SHUTTER		1
#define DISP_SHIFT   		2
#define DISP_DRIP    		3
#define DISP_FLIP    		4
#define DISP_SPIN    		5
#define DISP_CLEAR			(1<<3)
#define DISP_UPPERCASE		(1<<4)
#define DISP_REENTRANT		(1<<5)
#define DISP_FORCE			(1<<6)


#define OPT_ROTATED	(1<<3)
#define OPT_TOUPPER (1<<4)
#define OPT_XFONT   (1<<5)

static struct {
	char text[9][25];
	char addn[9][25];
	char use[9];
	int8_t timezone;
	int8_t brightness;			// lower 4 bit used
	uint8_t cycle;				// seconds to cycle to next content, 0 means no cycling
	uint8_t options;			// BIT0-2, transition effects, BIT3 rotated display, BIT4 toupper, BIT5 alternate font
	char reserved[7];
} _settings;


static uint8_t _next_transition = 0;
static uint8_t _segment_limit = 0;

static char     _chr_buf[50];
static uint8_t  _excess = 0, _excess_shift = 0, _excess_release = 0;
static unsigned _excess_cnt = 0;
// segments io a, b, c...m/2, dp, digits are io 1 to 12
//                              a   b   c   d   e   f   g   G   h   i   j   k   l   m   dp
/*
    ___ a
  f|\|/|b (hij)
    - - g G
  e|/|\|c (mlk)
    === d
*/
static const uint32_t *_font = asciiA;
static const uint8_t *_onOff = asciiOnOff;
const uint16_t map154[] = 
	{ 11<<5, 12<<5, 13<<5, 14<<5, 10<<5, 5<<5, 4<<5, 6<<5, 7<<5, 8<<5, 9<<5, 15<<5, 2<<5, 1<<5, 0, 3<<5 };

static uint8_t _input = 2;
static int8_t _mode = -1;
static uint16_t _countdown = 0;
static time_t _countfrom = 0;

//________________________________________________________________________________
void scan() {
	//for (uint8_t d=0;d<12;d++) pinMode(d+1, INPUT);
	//for (uint8_t s=0;s<14;s++) digitalWrite(seg_pins[s], LOW);
	//delay(5);
	// scan() is called at 40Khz, for 1/2 sec count to 20,000
	if (++_excess_cnt > 5000) {
		// 0..........b12345678901
		// THIS IS A LONGER STRING
		//
		_excess_cnt = 0;
		if (_excess_release) {				// done, we are just retaining last display content
			_excess_release--;
			if (!_excess_release)
				_excess = _excess_shift = 0;
		}//if
		else {
			if (_excess) {
				if (_excess_shift < _excess) {
					_excess_shift++;		// advance string position
				}//if
				else {
					_excess_release = 1;	// all done, fix last show for a bit before release
				}//else
			}//if
		}//else
	}//if

	static uint8_t d=0, on=0, off=0;

	if (on) {
		on--;
		return;
	}//if
#ifdef ENABLE_154
	if (_settings.options&OPT_ROTATED) {
		if (d > 15)
			pinMode(digit_map_r[d-12], INPUT);
		else
			digitalWrite(ENABLE_154, HIGH);
	}//if
	else {
		if (d > 7)
			digitalWrite(ENABLE_154, HIGH);
		else
			pinMode(digit_map[d], INPUT);
	}//else
#else
	if (_settings.options&OPT_ROTATED) 
		pinMode(digit_map_r[d], INPUT);		// turn off
	else
		pinMode(digit_map[d], INPUT);		// turn off
#endif
	if (off) {
		off--;
		return;
	}//if

	if (++d >= NUM_OF_DIGITS) d = 0;
	uint8_t digit = (_settings.options&OPT_ROTATED) ? NUM_OF_DIGITS - d - 1 : d;

#ifdef ENABLE_154
	if (digit > 7) {
		digitalWrite(ENABLE_154, HIGH);
	}//if
	else {
		digitalWrite(digit_map[digit], LOW);
		pinMode(digit_map[digit], INPUT);
	}//if
#else
	digitalWrite(digit_map[digit], LOW);
	pinMode(digit_map[digit], INPUT);
#endif
	//uint16_t cmp = 0x0001;
	//const uint8_t *sp = (_settings.options&OPT_ROTATED) ?  seg_pins_r : seg_pins;
	uint8_t num_segs = 0;
	REG_WRITE(GPIO_OUT_W1TC_REG, segment_maskA);//0b001001101000001011111110);
	REG_WRITE(GPIO_OUT1_W1TC_REG, segment_maskB);//0b00010110);
#ifdef ENABLE_154
	REG_WRITE(GPIO_OUT1_W1TC_REG, 0x1e0);
	if (digit > 7) {
		digitalWrite(ENABLE_154, LOW);
		REG_WRITE(GPIO_OUT1_W1TS_REG, map154[digit-8]);
	}//if
#endif
	uint32_t maskA = 0x00ffffff, maskB = 0x000003ff;
	if (_segment_limit) {
		maskA = spin_maskA[_segment_limit];
		maskB = spin_maskB[_segment_limit];
	}//if
	REG_WRITE(GPIO_OUT_W1TS_REG, _font[_chr_buf[d+_excess_shift]] & maskA);
	REG_WRITE(GPIO_OUT1_W1TS_REG, (_font[_chr_buf[d+_excess_shift]] >> 24) & maskB);
	//num_segs = _onOff[_chr_buf[d+_excess_shift]];
	on = _onOff[_chr_buf[d+_excess_shift]];
	/*
	for (uint8_t s=0;s<14;s++) {
		if ((segs&cmp) && (!_segment_limit || (s<_segment_limit))) {
			digitalWrite(sp[s], HIGH);
			num_segs++;
		}//if
		else {
			digitalWrite(sp[s], LOW);
		}//else
		cmp <<= 1;
	}//for
	*/
#ifdef ENABLE_154
	if (digit > 7) {
		on += (on >> 1);
	}//if
	else {
		pinMode(digit_map[digit], OUTPUT);
	}//else
#else
	pinMode(digit_map[digit], OUTPUT);
	on += _brightness;
#endif
	//digitalWrite(digit+1, LOW);
	//if (s) pinMode(digit_map[digit], INPUT);		// turn off
	/*
	if (num_segs > 1) on += 2;
	if (num_segs > 3) on += 2;
	if (num_segs > 5) on += 2;
	if (num_segs > 7) on += 2;
	on += _brightness;
	*/
	off = 8-_brightness;
}

//________________________________________________________________________________
void clearAll() {
	for (uint8_t i=0;i<50;i++) _chr_buf[i] = ' ';
}

//________________________________________________________________________________
void writeAscii(uint8_t d, uint8_t v, uint16_t opt=0) {

	d %= 24;
	if (d >= (NUM_OF_DIGITS*2)) return;

	uint16_t cmp = 0x0001, segs = 0x0000;

	if ((opt&DISP_UPPERCASE) && v >= 0x61 && v <= 0x7a) v -= 0x20;
	// opt>>8 to use numeric set, 1-chinese, 2-flower, 3-vertical 4-aurebesh
	// c2303 add support for vertical numerics
	//if ((opt&0x0300) && v >= 0x30 && v <= 0x3f) v = (v&0x0f) | ((opt>>8)-1)<<4;
	opt >>= 8;
	if ((opt&0x03) && v >= 0x30 && v <= 0x3f) {
		v &= 0x0f;
		switch (opt) {
			case 2: v += 1<<4; break;
			case 3: v += 9<<4;
			default: break;
		}//switch
	}//if
	if (opt&0x04) v += 0x80;		// aurebesh, shift full character set
	_chr_buf[d] = v;
}

//________________________________________________________________________________
uint8_t writeString(const char *s, uint16_t opt=0) {
	static char _last_string[64]="";
	static uint16_t _last_opt=0;

	if (_next_transition) {
		opt &= 0xfff8;
		opt |= _next_transition;
		_next_transition = 0;
	}//if

	if (opt&DISP_CLEAR) clearAll();
	if (opt&DISP_REENTRANT) {
		opt &= 0xfff0;		// clear effects
	}//if
	else {
		if (!strcmp(s, _last_string) && opt==_last_opt && !(opt&DISP_FORCE)) return 0;
		//strcpy(_last_string, s);
		//_last_opt = opt;
		_excess = _excess_shift = _excess_release = 0;
	}//else
	switch (opt&0x0f) {
		case DISP_SHUTTER:
			{
				const char *p=s;
				uint8_t i=0;
				clearAll();
				delay(30);
				writeString(s, opt|DISP_REENTRANT);
				delay(60);
				while (*p) {
					writeAscii(i, *p == ' ' ? ' ' : '-', opt);
					i++; p++;
				}//while
				delay(60);
			}
			break;
		case DISP_SHIFT:
			{
				clearAll();
				uint8_t j=(NUM_OF_DIGITS-1);
				while (j--) {
					const char *p = s;
					uint8_t done=0;
					for (uint8_t i=j;i<24;i++) {
						if (!*p) done = 1;
						writeAscii(i, done ? ' ' : *p, opt);
						p++;
					}//for
					delay(20);
				}//while
			}
			break;
		case DISP_DRIP:
			/*
			{
				_brightness = 7;
				while (_brightness--) delay(80);
				delay(60);
				writeString(s, opt|DISP_REENTRANT);
				while (_brightness++<=7) delay(80);
				_brightness = _settings.brightness&0x07;
			}
			*/
			{
				// [...........0] _12345
				// [..........0.] _12345
				// [.0..........] _12345
				// [0...........] _12345
				//  next
				// [01..........] __2345
				clearAll();
				const char *p = s;
				uint8_t j=0;
				while (*p) {
					if (*p != ' ') {
						for (uint8_t i=(NUM_OF_DIGITS-1);i>j;i--) {
							_chr_buf[i] = ' ';
							delay(5);
							writeAscii(i-1, *p, opt);
							delay(10);
						}//for
					}//if
					j++;
					p++;
				}//while
			}
			break;
		case DISP_FLIP:
			{
				for (char c='0';c<'Z';c++) {
					for (uint8_t i=0;i<NUM_OF_DIGITS;i++) {
						if (_last_string[i] > ' ') 
							writeAscii(i, _last_string[i] > c ? _last_string[i] : c, opt);
					}//for
					delay(20);
				}//for
				clearAll();
				for (char c='0';c<'Z';c++) {
					const char *p = s;
					for (uint8_t i=0;i<strlen(s);i++) {
						if (*p > ' ') writeAscii(i, *p > c ? c : *p, opt);
						p++;
					}//for
					delay(20);
				}//for
			}
			break;
		case DISP_SPIN:
			{
				for (uint8_t i=1;i<14;i++) {
					_segment_limit = 14 - i;
					delay(30);
				}//for
				writeString(s, opt|DISP_REENTRANT);
				for (uint8_t i=1;i<14;i++) {
					_segment_limit = i;
					delay(30);
				}//for
				_segment_limit = 0;
			}
			break;
		default:
			break;
	}//switch
	//
	if (!(opt&DISP_REENTRANT)) {
		strcpy(_last_string, s);
		_last_opt = opt;
		//________ for excessive characters, we shift them out
		if (strlen(s) > NUM_OF_DIGITS) {
			//_excess = (strlen(s) - NUM_OF_DIGITS);
			_excess = strlen(s);
			_excess_shift = _excess_release = 0;
			_excess_cnt = 0;
		}//if
	}//if

	uint8_t i=0;
	while (*s) writeAscii(i++, *s++, opt);
	return (i);
}

void IRAM_ATTR onTimer() {
	scan();
}

//________________________________________________________________________________
void setupDisplay() {
    pinMode(_BT2, INPUT_PULLUP);
    pinMode(_LED, OUTPUT);

	//_settings.brightness = 4;
	/*______ direct multiplexing
	for (uint8_t i=1;i<=18;i++) pinMode(i, OUTPUT);
    pinMode(21, OUTPUT);
	for (uint8_t i=33;i<=40;i++) pinMode(i, OUTPUT);
	*/
	REG_WRITE(GPIO_ENABLE_REG, all_maskA);//0b00000000001001111111111111111110);
	REG_WRITE(GPIO_ENABLE1_REG, all_maskB);//0b0000000111111110);
	pinMode(39, OUTPUT);
	pinMode(40, OUTPUT);
#ifdef ENABLE_154
	pinMode(ENABLE_154, OUTPUT);
	digitalWrite(ENABLE_154, HIGH);
#endif
	_timer = timerBegin(0, 80, true);
	timerAttachInterrupt(_timer, onTimer, true);
	timerAlarmWrite(_timer, 25, true);			// 25us...40Khz
	timerAlarmEnable(_timer);
	clearAll();
}
//________________________________________________________________________________
void resetConfig() {
	strcpy(_settings.text[0], "%b %d %a");
	strcpy(_settings.addn[0], "%R");
	strcpy(_settings.text[1], "~W");
	strcpy(_settings.addn[1], "");
	strcpy(_settings.text[2], "~w");
	strcpy(_settings.addn[2], "");
	strcpy(_settings.text[3], "~R");
	strcpy(_settings.addn[3], "");
	strcpy(_settings.text[4], "NEW YORK");
	strcpy(_settings.addn[4], "%T");
	strcpy(_settings.text[5], "STORE CLOSED");
	strcpy(_settings.addn[5], "");
	strcpy(_settings.text[6], "HOTDOG $4.50");
	strcpy(_settings.addn[6], "%R");
	strcpy(_settings.text[7], "PLS WAIT TO BE SEATED");
	strcpy(_settings.addn[7], "");
	strcpy(_settings.text[8], "BE BACK IN");
	strcpy(_settings.addn[8], "~D");
	_settings.use[0] = _settings.use[1] = _settings.use[2] = _settings.use[3] = 'o';
	_settings.use[4] = _settings.use[5] = _settings.use[6] = _settings.use[7] = ' ';
	_settings.use[8] = (char) 1;
	_settings.cycle = 0;
	_settings.brightness = 4;
	_settings.options = OPT_TOUPPER;
	//_settings.options &= ~OPT_ROTATED;
	_settings.timezone = -4;
}
//________________________________________________________________________________
void loadConfig() {
	_brightness = _settings.brightness;
	/*
	if (_settings.options&OPT_XFONT) {
		_font  = (_settings.options&OPT_ROTATED) ? aurebeshB : aurebeshA;
		_onOff = aurebeshOnOff;
	}//if
	else {
		_font  = (_settings.options&OPT_ROTATED) ? asciiB : asciiA;
		_onOff = asciiOnOff;
	}//else
	*/
	_font  = (_settings.options&OPT_ROTATED) ? asciiB : asciiA;
	if (_settings.options&OPT_XFONT) _font += 0x80;
	_onOff = asciiOnOff;

}
//________________________________________________________________________________
void saveConfig() {
	_brightness = _settings.brightness;
	prefs.end();
	prefs.begin("S14Clock", false);
	prefs.putBytes((const char*) "_settings", (void *) &_settings, sizeof(_settings));
	prefs.end();
	prefs.begin("S14Clock", true);
	loadConfig();
	//configTime(_settings.timezone * 3600, 0, NTPServer1, NTPServer2);
}
//________________________________________________________________________________
void setupClock() {
	prefs.begin("S14Clock", true);
	if (!prefs.isKey("_settings")) {
		resetConfig();
		saveConfig();
	}//if
	prefs.getBytes((const char*) "_settings", (void *) &_settings, sizeof(_settings));
	configTime(_settings.timezone * 3600, 0, NTPServer1, NTPServer2);
	loadConfig();
}


#ifdef USE_WIFI
//________________________________________________________________________________
void notFound() {
  server.send(404, "text/plain", "Not found");
}

char htmlResponse[6000];

void handleRoot() {
  snprintf(htmlResponse, 6000, "\
<!DOCTYPE html>\
  <style>\
  body { font-family:Arial; }\
  fieldset p { clear:both; padding:0px; }\
  fieldset { background-color:gainsboro; }\
  </style>\
<html lang='en'>\
  <head>\
    <meta charset='utf-8'>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
  </head>\
  <body>\
  <fieldset>\
  <legend style='color:dimgrey'><b>Seg14 Clock</b></legend>\
  <a href=\"/button1\"><button>Count Down</button></a>\
  <a href=\"/button2\"><button>Advance Display</button></a>\
  <a href=\"/reset\"><button>Reset Configuration</button></a><br><br>\
  <a href=\"/_ac\"><button>Reset WIFI</button></a>\
  </fieldset><br>\
  <form action='/fix'>\
	  <fieldset>\
	  <legend style='color:dimgrey'><b>One-time Message</b></legend>\
		  <input type='text' name='message' value='~x' size='24' maxlength='24' autofocus>\
		  <input type='submit' value='Send'>\
	  </fieldset>\
  </form><br>\
  <form action='/action_page'>\
	  <fieldset>\
	  <legend style='color:dimgrey'><b>Display Contents</b></legend>\
	  Content accepts ~? (custom) and %%? (strtime) formatting tokens<br>\
	  <table cellspacing='2px' cellpadding='2px'>\
	  <tr>\
		  <td>Content</td>\
		  <td>End With</td>\
		  <td>Use</td>\
	  </tr>\
	  <tr>\
		  <td><input type='text' name='textA' value='%-.24s' size='24' maxlength='24' autofocus>  </td>\
		  <td><input type='text' name='addnA' value='%-.24s' size='8' maxlength='12'>  </td>\
		  <td><input type='checkbox' name='useA' %s></td>\
	  </tr>\
	  <tr>\
		  <td><input type='text' name='textB' value='%-.24s' size='24' maxlength='24' autofocus>  </td>\
		  <td><input type='text' name='addnB' value='%-.24s' size='8' maxlength='12'>  </td>\
		  <td><input type='checkbox' name='useB' %s></td>\
	  </tr>\
	  <tr>\
		  <td><input type='text' name='textC' value='%-.24s' size='24' maxlength='24' autofocus>  </td>\
		  <td><input type='text' name='addnC' value='%-.24s' size='8' maxlength='12'>  </td>\
		  <td><input type='checkbox' name='useC' %s></td>\
	  </tr>\
	  <tr>\
		  <td><input type='text' name='textD' value='%-.24s' size='24' maxlength='24' autofocus>  </td>\
		  <td><input type='text' name='addnD' value='%-.24s' size='8' maxlength='12'>  </td>\
		  <td><input type='checkbox' name='useD' %s></td>\
	  </tr>\
	  <tr>\
		  <td><input type='text' name='textE' value='%-.24s' size='24' maxlength='24' autofocus>  </td>\
		  <td><input type='text' name='addnE' value='%-.24s' size='8' maxlength='12'>  </td>\
		  <td><input type='checkbox' name='useE' %s></td>\
	  </tr>\
	  <tr>\
		  <td><input type='text' name='textF' value='%-.24s' size='24' maxlength='24' autofocus>  </td>\
		  <td><input type='text' name='addnF' value='%-.24s' size='8' maxlength='12'>  </td>\
		  <td><input type='checkbox' name='useF' %s></td>\
	  </tr>\
	  <tr>\
		  <td><input type='text' name='textG' value='%-.24s' size='24' maxlength='24' autofocus>  </td>\
		  <td><input type='text' name='addnG' value='%-.24s' size='8' maxlength='12'>  </td>\
		  <td><input type='checkbox' name='useG' %s></td>\
	  </tr>\
	  <tr>\
		  <td><input type='text' name='textH' value='%-.24s' size='24' maxlength='24' autofocus>  </td>\
		  <td><input type='text' name='addnH' value='%-.24s' size='8' maxlength='12'>  </td>\
		  <td><input type='checkbox' name='useH' %s></td>\
	  </tr>\
	  <tr>\
	  <td>Content (Count Down)</td>\
	  <td>#Dd #Uu</td>\
	  <tr>\
		  <td><input type='text' name='textX' value='%s' size='24' maxlength='24' autofocus>  </td>\
		  <td><input type='text' name='addnX' value='%s' size='8' maxlength='12'>  </td>\
	  </tr>\
	  </table>\
	  <p>Countdown Increment:<br>\
		  <input type='radio' name='useX' value=1 %s> 1 min\
		  <input type='radio' name='useX' value=5 %s> 5 min\
		  <input type='radio' name='useX' value=10 %s> 10 min\
		  <input type='radio' name='useX' value=15 %s> 15 min\
		  <input type='radio' name='useX' value=30 %s> 30 min\
	  </p>\
	  <p>Transition Effect:<br>\
		  <input type='radio' name='transition' value=0 %s> NA\
		  <input type='radio' name='transition' value=1 %s> Blink\
		  <input type='radio' name='transition' value=2 %s> Shift\
		  <input type='radio' name='transition' value=3 %s> Drip\
		  <input type='radio' name='transition' value=4 %s> Flip\
		  <input type='radio' name='transition' value=5 %s> Spin\
	  </p>\
	  <p>Cycles Contents Every <input type='number ' name='cycle' size=3 min=0 max=255 value=%d>  Seconds</p>\
		  <p>Bright: <input type='range' name='brightness' min=0 max=7 value=%d>\
		    Rotate: <input type='checkbox' name='optRotate' %s></p>\
		   All Caps: <input type='checkbox' name='optToUpper' %s>\
		   Aurebesh: <input type='checkbox' name='optXfont' %s></p>\
		  <p>Time Zone (-11 to 14): <input type='number' name='timezone' size=3 min=-11 max=14 value=%d></p>\
		  <p id='suggest_tz'></p>\
		  <input type='submit' value='Save Configuration'>\
	  </fieldset>\
	  <br>\
  </form>\
  </fieldset>\
  <fieldset>\
  <legend style='color:dimgrey'><b>Format Control</b></legend>\
  Formatting tokens can be used in content / message<br><br>\
  <table border='1px' border-collapse='collapse' >\
  <tr>\
    <th>Token</th>\
    <th>Effect on Content / Message</th>\
  </tr>\
  <tr> <td>~1</td> <td>Chinese Numerics</td> </tr>\
  <tr> <td>~2</td> <td>Flower Code Numerics</td> </tr>\
  <tr> <td>~3</td> <td>Vertical Numerics</td> </tr>\
  <tr> <td>~4</td> <td>Aurebesh Character Set</td> </tr>\
  <tr> <td>~X</td> <td>Message expires in 60 seconds</td> </tr>\
  <tr> <td>~x</td> <td>Message expires in 10 seconds</td> </tr>\
  <tr> <td>~W</td> <td>Worded Time Format 1</td> </tr>\
  <tr> <td>~w</td> <td>Worded Time Format 2</td> </tr>\
  <tr> <td>~R</td> <td>Roman Numeric Hour-Min</td> </tr>\
  <tr> <td>~r</td> <td>Roman Numeric Hour-Min-Sec</td> </tr>\
  <tr> <td>~D</td> <td>Countdown HH:MM:SS</td> </tr>\
  <tr> <td>~d</td> <td>Countdown HH:MM</td> </tr>\
  <tr> <td>~U</td> <td>Countup HH:MM:SS</td> </tr>\
  <tr> <td>~u</td> <td>Countup HH:MM</td> </tr>\
  </table>\
  <br>Search web for <a href=\"https://www.google.com/search?q=strftime\">strtime()</a> formatting tokens, common ones includes % + HMSmdw for hour, min, sec, month, day, weekday, etc.<br>\
  </fieldset><br>\
  <script>document.getElementById('suggest_tz').innerHTML = 'Suggest Time Zone -' + (new Date().getTimezoneOffset() / 60);</script>\
  </body>\
</html>\
",
_settings.text[0], _settings.addn[0], _settings.use[0]=='o' ? "checked" : "",
_settings.text[1], _settings.addn[1], _settings.use[1]=='o' ? "checked" : "",
_settings.text[2], _settings.addn[2], _settings.use[2]=='o' ? "checked" : "",
_settings.text[3], _settings.addn[3], _settings.use[3]=='o' ? "checked" : "",
_settings.text[4], _settings.addn[4], _settings.use[4]=='o' ? "checked" : "",
_settings.text[5], _settings.addn[5], _settings.use[5]=='o' ? "checked" : "",
_settings.text[6], _settings.addn[6], _settings.use[6]=='o' ? "checked" : "",
_settings.text[7], _settings.addn[7], _settings.use[7]=='o' ? "checked" : "",
_settings.text[8], _settings.addn[8],
_settings.use[8]<=1 ? "checked" : "",
_settings.use[8]==5 ? "checked" : "",
_settings.use[8]==10 ? "checked" : "",
_settings.use[8]==15 ? "checked" : "",
_settings.use[8]==30 ? "checked" : "",
(_settings.options&0x07) == 0 ? "checked" : "",
(_settings.options&0x07) == 1 ? "checked" : "",
(_settings.options&0x07) == 2 ? "checked" : "",
(_settings.options&0x07) == 3 ? "checked" : "",
(_settings.options&0x07) == 4 ? "checked" : "",
(_settings.options&0x07) == 5 ? "checked" : "",
_settings.cycle,
_settings.brightness, 
_settings.options & OPT_ROTATED ? "checked" : "", 
_settings.options & OPT_TOUPPER ? "checked" : "", 
_settings.options & OPT_XFONT   ? "checked" : "", 
_settings.timezone
);

	server.send(200, "text/html", htmlResponse);
}

void handleButton1() {
	_input = 0x01;
	server.sendHeader("Location", "/");
	server.send(302, "text/plain", "Updated - Press Back Button");
}

void handleButton2() {
	_input = 0x02;
	server.sendHeader("Location", "/");
	server.send(302, "text/plain", "Updated - Press Back Button");
}

void showMessage(const char *sp);

void handleFix() {
	showMessage(server.arg("message").c_str());
	server.sendHeader("Location", "/");
	server.send(302, "text/plain", "Updated - Press Back Button\n");
}

void handleMsg() {
	showMessage(server.pathArg(0).c_str());
	server.sendHeader("Location", "/");
	server.send(302, "text/plain", "Updated - Press Back Button\n");
}

void handleReset() {
	resetConfig();
	server.sendHeader("Location", "/");
	server.send(302, "text/plain", "Updated - Press Back Button");
	saveConfig();
}

void handleForm() {
	strcpy(_settings.text[0], server.arg("textA").c_str());
	strcpy(_settings.text[1], server.arg("textB").c_str());
	strcpy(_settings.text[2], server.arg("textC").c_str());
	strcpy(_settings.text[3], server.arg("textD").c_str());
	strcpy(_settings.text[4], server.arg("textE").c_str());
	strcpy(_settings.text[5], server.arg("textF").c_str());
	strcpy(_settings.text[6], server.arg("textG").c_str());
	strcpy(_settings.text[7], server.arg("textH").c_str());
	strcpy(_settings.text[8], server.arg("textX").c_str());
	strcpy(_settings.addn[0], server.arg("addnA").c_str());
	strcpy(_settings.addn[1], server.arg("addnB").c_str());
	strcpy(_settings.addn[2], server.arg("addnC").c_str());
	strcpy(_settings.addn[3], server.arg("addnD").c_str());
	strcpy(_settings.addn[4], server.arg("addnE").c_str());
	strcpy(_settings.addn[5], server.arg("addnF").c_str());
	strcpy(_settings.addn[6], server.arg("addnG").c_str());
	strcpy(_settings.addn[7], server.arg("addnH").c_str());
	strcpy(_settings.addn[8], server.arg("addnX").c_str());
	_settings.use[0] = server.arg("useA") != "" ? server.arg("useA")[0] : ' ';
	_settings.use[1] = server.arg("useB") != "" ? server.arg("useB")[0] : ' ';
	_settings.use[2] = server.arg("useC") != "" ? server.arg("useC")[0] : ' ';
	_settings.use[3] = server.arg("useD") != "" ? server.arg("useD")[0] : ' ';
	_settings.use[4] = server.arg("useE") != "" ? server.arg("useE")[0] : ' ';
	_settings.use[5] = server.arg("useF") != "" ? server.arg("useF")[0] : ' ';
	_settings.use[6] = server.arg("useG") != "" ? server.arg("useG")[0] : ' ';
	_settings.use[7] = server.arg("useH") != "" ? server.arg("useH")[0] : ' ';
	_settings.use[8] = (char) (server.arg("useX") != "" ? atoi(server.arg("useX").c_str()) : 1);
	_settings.options &= ~0x07;
	_settings.options |= (char) (server.arg("transition") != "" ? atoi(server.arg("transition").c_str()) : 0);
	if (server.arg("cycle")!= "") _settings.cycle = atoi(server.arg("cycle").c_str());
	if (server.arg("brightness")!= "") _settings.brightness = atoi(server.arg("brightness").c_str());
	if (server.arg("optRotate")!= "") _settings.options |= OPT_ROTATED;
	else _settings.options &= ~OPT_ROTATED;
	if (server.arg("optToUpper")!= "") _settings.options |= OPT_TOUPPER;
	else _settings.options &= ~OPT_TOUPPER;
	if (server.arg("optXfont")!= "") _settings.options |= OPT_XFONT;
	else _settings.options &= ~OPT_XFONT;
	if (server.arg("timezone")!= "") _settings.timezone = atoi(server.arg("timezone").c_str());

	server.sendHeader("Location", "/");
	server.send(302, "text/plain", "Updated - Press Back Button");
	saveConfig();
	//snprintf(htmlResponse, 2048, "<a ref='/'> Go Back </a>");
	//server.send(200, "text/html", htmlResponse);
}

uint8_t setupWebServer() {
	server.on("/", handleRoot);
	server.on("/fix", handleFix);			// example 192.168.2.xx/fix?message=hello world
	server.on("/reset", handleReset);
	server.on("/action_page", handleForm);
	server.on("/button1", handleButton1);
	server.on("/button2", handleButton2);
	server.on("/favicon.ico", notFound);
	server.on(UriRegex("/msg/(.+)"), HTTP_GET, handleMsg);		// example 192.168.2.xx/msg/hello%20world
	//server.onNotFound(notFound);
	//portal.onNotFound(handleFix1);
	//server.begin();
	portal.config(Config);
	return portal.begin();
}

#endif

void macroSub(uint16_t *pOpt, char *dp, const char *sp);
char* wordTime(char *p, char format);

// 012345678901234567890123
// TWELVE TWENTY-SEVEN   34
// SEPTEMBER TWENTY-SEVEN
// 2022-10-28 FRIDAY
// 10:28:32 PM TUESDAY

// TWELVE O-CLOCK, O-0NE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE
// TWELVE TEN, ELEVEN, TWELVE, THIRTEEN, FOURTEEN, FIFTEEN, SIXTEEN, SEVENTEEN, EIGHTEEN, NINETEEN
// TWELVE TWENTY, TWENTY-ONE, ....., TWENTY-NINE
// TWELVE THIRTY, .., FORTY, .., FIFTY
//________________________________________________________________________________
const char minInc[][12] = { "FIVE", "TEN", "QUARTER", "TWENTY", "TWENTY-FIVE", "HALF", };
const char units[][10] = {
	"ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE",
	"TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN", "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN",
};
const char tens[][7] = { "TWENTY", "THIRTY", "FORTY", "FIFTY", };
const char unitsR[][10] = { "", "\x11", "\x12", "\x13", "\x11V", "V", "V\x11", "V\x12", "V\x13", "\x11X", };
const char tensR[][7] = { "", "X", "XX", "XXX", "XL", "L", };

static time_t _epoch = 0;
static struct tm *_tm = 0;
//________________________________________________________________________________
char* wordTime(char *p, char format) {
	uint8_t pos=0;
	switch (format) {
		case 'W': 
		{
			// #W worded time format 1
			// 012345678901234567890123
			// TWENTY-FIVE PAST TWELVE
			// FIVE,QUARTER,TWENTY-FIVE PAST,TO ...
			uint8_t to=_tm->tm_min>=58 ? 1 : 0;
			_tm->tm_min /= 5;
			// 10:01 past==0, min==0, TEN O'CLOCK
			// 10:31 past==1, min==0, HALF PAST TEN
			if (_tm->tm_min > 6) {
				_tm->tm_hour++;
				to++;
				_tm->tm_min = 12 - _tm->tm_min;
			}//if
			if (_tm->tm_min) strcpy(p, minInc[_tm->tm_min-1]);
			if (to) {
				if (to > 1)
					strcpy(p, "ALMOST ");
				else
					strcat(p, " TO ");
			}//if
			else {
				if (_tm->tm_min) strcat(p, " PAST ");
			}//else
			_tm->tm_hour %= 12;
			if (!_tm->tm_hour) _tm->tm_hour = 12;
			strcat(p, units[_tm->tm_hour]);
			if (!_tm->tm_min) strcat(p, " O'CLOCK ");
			break;
		}
		case 'w': {
			// #w worded time format 2
			// 012345678901234567890123
			// TWELVE FORTY-THREE
			_tm->tm_hour %= 12;
			if (_tm->tm_hour == 0) _tm->tm_hour = 12;
			strcat(p, units[_tm->tm_hour]);
			strcat(p, " ");
			if (_tm->tm_min) {
				if (_tm->tm_min >= 20) {
					_tm->tm_min -= 20;
					strcat(p, tens[_tm->tm_min/10]);
					if (_tm->tm_min%10) {
						strcat(p, "'"); pos++;
						strcat(p, units[_tm->tm_min%10]);
					}//if
				}//if
				else {
					if (_tm->tm_min < 10) {
						strcat(p, "O'");
					}//if
					strcat(p, units[_tm->tm_min]);
				}//else
			}//if
			else {
				strcat(p, "O'CLOCK ");
			}//else
			break;
		}
		case 'R':		// roman numerics
		case 'r':
			// 12:01, XII.I
			_tm->tm_hour %= 12;
			if (_tm->tm_hour == 0) _tm->tm_hour = 12;
			strcat(p, tensR[_tm->tm_hour/10]);
			strcat(p, unitsR[_tm->tm_hour%10]);
			strcat(p, ",");
			strcat(p, tensR[_tm->tm_min/10]);
			strcat(p, unitsR[_tm->tm_min%10]);
			if (format == 'R') break;
			strcat(p, ",");
			strcat(p, tensR[_tm->tm_sec/10]);
			strcat(p, unitsR[_tm->tm_sec%10]);
			break;
		case 'U':
		case 'u':
			_countdown = _epoch - _countfrom;
			sprintf(p, "%2d:%02d:%02d", _countdown / 3600, (_countdown / 60) % 60, _countdown % 60);
			if (format == 'U') *(p+5) = '\0';
			break;
		case 'D':
		case 'd':
			sprintf(p, "%2d:%02d:%02d", _countdown / 3600, (_countdown / 60) % 60, _countdown % 60);
			if (format == 'U') *(p+5) = '\0';
			break;
		case 'X':	// expired in 60 seconds
			_countdown += 50;
		case 'x':	// expired in 10 seconds
			_countdown += 10;
			_input |= 0x08;
			break;
		default: break;
	}//switch
	while (*p) p++;
	return p;
}

//________________________________________________________________________________
void showMessage(const char *sp) {
	char out1[48] = "";
	char out2[48] = "";
	uint16_t opt = DISP_CLEAR|DISP_FORCE;
	_input = 0x04;
	_next_transition = _settings.options&0x07;
	macroSub(&opt, out1, sp);
	strftime(out2, sizeof(out2), out1, _tm);
	writeString(out2, opt);
}

//________________________________________________________________________________
void macroSub(uint16_t *pOpt, char *dp, const char *sp) {
	while (*sp) {		// process internal # tokens 1st
		if (*sp == '~' && (strchr("~123WwRrUuDdXx", *(sp+1)))) {
			sp++;
			if (*sp == '~') *dp++ = *sp;
			// c2303 add support for vertical numerics
			//if (strchr("123", *sp)) *pOpt |= 1 << ((*sp - '1') + 8);
			if (strchr("1234", *sp)) *pOpt |= (*sp - '0') << 8;
			if (strchr("WwRrUuDdXx", *sp)) dp = wordTime(dp, *sp);
		}//if
		else {
			*dp++ = *sp;
		}//else
		sp++;
	}//while
	*dp = '\0';
}

//________________________________________________________________________________
void showTime(uint8_t format) {
	char content[48] = "";
	char buf[48] = "";
	uint8_t pos = 0;
	_epoch = time(NULL);
	_tm = localtime(&_epoch);

	if (_countdown > 0) {		// countdown
		_countdown--;
		format = 8;
	}//if
	uint16_t use_opt = 0;
	char *cp = _settings.text[format];

	if (_settings.options&OPT_TOUPPER) use_opt = DISP_UPPERCASE;

	macroSub(&use_opt, content, cp);
	strftime(buf, sizeof(buf), content, _tm);

	pos = strlen(buf);
	if (pos < NUM_OF_DIGITS) {
		char rbuf[48] = "";
		cp = _settings.addn[format];
		*content = '\0';

		macroSub(&use_opt, content, cp);
		strftime(rbuf, sizeof(rbuf), content, _tm);
		while (pos < (NUM_OF_DIGITS-strlen(rbuf))) buf[pos++] = ' ';
		if ((pos + strlen(rbuf)) > NUM_OF_DIGITS) pos = NUM_OF_DIGITS - strlen(rbuf);
		buf[pos] = '\0';
		strcat(buf, rbuf);
		//buf[NUM_OF_DIGITS] = '\0';
	}//if

	writeString(buf, use_opt);
}
//

/* experiment w/ class inheritance

class LEDBar : public Stream {
	private:
		int pos=0;
		int advance=0;
	public:
		int read() { return 0; }
		int available() { return 0; }
		int peek() { return 0; }
		virtual size_t write(uint8_t val) { 
			if (val == 0x0d || val == 0x0a) {
				advance = 1;
			}//if
			else {
				if (advance) {
					advance = 0;
					pos = 0;
					clearAll();
				}//if
				writeAscii(pos, val, 1); 
				if (++pos>23) pos = 0;
			}//else
			return 1; 
		}
		virtual size_t write(const uint8_t *buf, uint8_t val) { writeString((char*) buf); return val; }
		using Print::write;
		virtual void flush() { }
};


LEDBar _timeBar;
*/



//________________________________________________________________________________
void setup() {
	setupDisplay();
	writeString("OOOOOOOOOOOOOOOOOOOOOOOO");
	delay(400);
	writeString("************************");
	delay(400);
	clearAll();
#ifdef USE_WIFI
	writeString("CONNECTING");
	if (setupWebServer()) {
		//configTime(TIMEZONE, 0, NTPServer1, NTPServer2);
		char buf[24];
		clearAll();
		sprintf(buf, "%s", WiFi.localIP().toString().c_str());
		writeString(buf);
		delay(2000);
	}//if
#endif
	/*
	if (setup)
		writeString("CONNECTING", DISP_SHUTTER);
	else
		writeString("CONNECTED", DISP_SHUTTER);
	delay(1000);
	*/
	//setESPAutoWiFiConfigDebugOut(_timeBar);
	//if (ESPAutoWiFiConfigSetup(_LED, false, sizeof(_settings))) return;
	setupClock();
	/*
	strcpy(_settings.addn[8], "#d");
	_settings.options = 0;
	strcpy(_settings.text[0], "0123456789AB");
	*_settings.addn[0] = '\0';
	strcpy(_settings.text[1], "CDEFGHIJKLMN");
	*_settings.addn[1] = '\0';
	strcpy(_settings.text[2], "OPQRSTUVWXYZ");
	*_settings.addn[2] = '\0';
	_settings.cycle = 0;
	_brightness = 4;
	*/
	//resetConfig();
	//saveConfig();
	//_input = 0x04;
	//writeString("#x#1012345ABCXYZ", DISP_CLEAR);
}
static int _count = 0;
//________________________________________________________________________________
void loop() {
	static uint8_t cycle = _settings.cycle;
	static uint32_t save_time = 0;
	static uint32_t stay_in_3_until = 0;
	uint32_t current_time = millis();

	if (digitalRead(_BT2) == LOW) {
		while (digitalRead(_BT2) == LOW) {			// wait for key release
			if ((millis() - current_time) > 500) {
				//__________ release after 1/2 second, considered as long pressed
				_input = 3;
				writeString("-", DISP_CLEAR);		// indicate press-n-hold achieved
				while (digitalRead(_BT2) == LOW);	// wait for final release
			}//if
		}//while
		if (stay_in_3_until) _input = 3;			// single key press following a press-n-hold consider as press-n-hold
		if (_input != 3) {		// not press-n-hold
			// ____    __________    _____
			//     |__|          |__|
			//        <--400ms-->
			//key_last_pressed = millis();	// wait for next key or time registers
			_input = 2;
			delay(150);		// hold 250ms, if comeback
			for (uint8_t i=0;i<10 && (digitalRead(_BT2) != LOW);i++) {
				delay(10);
			}//for
			if (digitalRead(_BT2) == LOW) {
				while (digitalRead(_BT2) == LOW);
				_input = 1;
			}//if
		}//if
	}//if
	else {
		if (stay_in_3_until) {
			if (current_time > stay_in_3_until) {
				stay_in_3_until = 0;
				saveConfig();
				_next_transition = _settings.options&0x07;		// force transition
				cycle = _settings.cycle;
			}//if
			else {
				return;		// wait for press-n-hold succession to expire
			}//else
		}//if
	}//else

	if (_input == 3) stay_in_3_until = millis() + 1000;
	if (_input & 0x03) current_time += 1000;		// force immediate action

	const char _transit_label[][10] = { "-NONE", "-SHUTTER", "-SHIFT", "-DRIP", "-FLIP", "-SPIN", };
	switch (_input) {
		case 1: // increment countdown minutes
			if (!_settings.use[8]) _settings.use[8] = 1;
			_countdown += _settings.use[8] * 60;
			_countfrom = time(NULL) - 1;
			break;
		case 2: // cycle display content
			{
			if (_countdown) {
				_countdown = 0;
			}//if
			else { 		// not long pressed
				++_mode %= 8;		
				uint8_t check=_mode;
				while (_settings.use[_mode] != 'o') {
					++_mode %= 8;
					//_________ use slot 0 if none is setup
					if (check==_mode) _settings.use[0] = 'o';
				}//while
				cycle = _settings.cycle;
				_next_transition = _settings.options&0x07;
			}//else
			}
			break;
		case 3: // long pressed button 2
			_countdown = 0;
			_settings.options++;
			if ((_settings.options&0x07) > DISP_SPIN) _settings.options &= ~0x07;
			//saveConfig();
			writeString(_transit_label[_settings.options&0x07], DISP_CLEAR);
			_input = 0;
			return;
			/*
			delay(1000);
			_next_transition = _settings.options&0x07;		// force transition
			cycle = _settings.cycle;
			*/
			break;
		//case 4:	// showing one time message
		default:
			break;
	}//switch
	_input &= ~0x03;		// clear button actions
	//_____ update display
	if ((current_time - save_time) > 1000) {			// second passed
		// every second we do this
		save_time = current_time;
		if (!_input) {	// normal mode, check to automatic cycling of contents
			showTime(_mode);
			if (cycle) {
				cycle--;
			}//if
			else {
				if (_settings.cycle && !_countdown) {		// advance content, set timer for next advance
					_input = 2;
					cycle = _settings.cycle;
					_next_transition = _settings.options&0x07;
				}//if
			}//else
		}//if
		else {							// in fixed message mode
			if (_input & 0x08) {		// with expiry
				if (_countdown) _countdown--;
				else _input = 2;		// advance
			}//if
		}//else
	}//if
#ifdef USE_WIFI
	portal.handleClient();
#endif
	delay(1);			// suppose to save a lot of power
	//
}
