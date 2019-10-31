/*
   File: screen.h
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.0
   Creation: Sep-11-2019
   Revised: Oct-24-2019
   Release: Oct-30-2019
   Author: T. Black
   (c) copyright T. Black 2019, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
 */

#include <XPT2046_Touchscreen.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Fonts/FreeSans9pt7b.h"
#include "Fonts/FreeSans12pt7b.h"
#include "Fonts/FreeSansBold12pt7b.h"
#include "Fonts/FreeMonoBold18pt7b.h"
#include "Fonts/FreeMonoBold24pt7b.h"
#include "icons.h"

// Menu page Timers
#define DATA_REFRESH_TIME 250 // Amps data Refresh time, in mS.
#define REPEAT_CNT_THRS 5     // Keypress repeat counter threshold to enable fast speed during amps setting.
#define REPEAT_FAST_MS 50     // Fast repeat speed for amps setting, in mS. 20Hz.
#define REPEAT_SLOW_MS 200    // Slow repeat speed for amps setting, in mS. 5Hz.
#define SET_AMPS_TIME 1500    // Timer value for how long to show Welding Amps if user changes amps setting.

// Menu page Definitions
#define PG_HOME 0             // Home Page.
#define PG_VOL 10             // Volume Page.
#define PG_INFO 20            // Main Rod Information Page, Menu.
#define PG_INFO_6011 21       // E-6011 Rod Info Page.
#define PG_INFO_6013 22       // E-6013 Rod Info Page.
#define PG_INFO_7018 23       // E-7018 Rod Info Page.
#define PG_SET 30             // Settings Page.
#define PG_ERROR 40           // Error (Caution) Page.
#define PG_RD_TIME_MS 45000   // Timeout time (mS) for reading a rod information page automatic before exit.
#define MENU_RD_TIME_MS 10000 // Timeout time (mS) for chosing a menu item before automatic exit.

// BlueTooth message defines
#define BLE_MSG_FAIL  0
#define BLE_MSG_AUTO  1
#define BLE_MSG_FOUND 2
#define BLE_MSG_SCAN  3

// Graphics Defines for X=320 : Y=240 landscape Formatted display.
#define ARCBOX_X 10 // Arc current On/Off Button Box Area X
#define ARCBOX_Y 11
#define ARCBOX_W 46
#define ARCBOX_H 51
#define ARCBOX_R 3

#define AUPBOX_X 77 // Arrow Up Button Box Area X
#define AUPBOX_Y 11
#define AUPBOX_W 46
#define AUPBOX_H 78
#define AUPBOX_R 3

#define ADNBOX_X 77 // Arrow Down Button Box Area X
#define ADNBOX_Y 97
#define ADNBOX_W 46
#define ADNBOX_H 78
#define ADNBOX_R 3

#define AMPBAR_X 150 // Amp Moving Bar area X
#define AMPBAR_Y 95  // Amp Moving Bar area Y
#define AMPBAR_W 120 // Amp Moving Bar Width
#define AMPBAR_H 15  // Amp Moving Bar Height

#define AMPBOX_X 140 // Amp Button Box area X
#define AMPBOX_Y 0   // Amp Button Box area Y
#define AMPBOX_H 123 // Amp Button Box Height
#define AMPBOX_W 165 // Width of Amp Box
#define AMPBOX_R 10  // Amp Box corner Radius
#define AMPVAL_H 70  // Height of Amp Value Font

#define FBBOX_X 20   // Scan Bluetooth Button Box area X
#define FBBOX_Y 170
#define FBBOX_W 220
#define FBBOX_H 40
#define FBBOX_R 3

#define BOBOX_X 20 // Bluetooth On/Off Button Box area X
#define BOBOX_Y 112
#define BOBOX_W 50
#define BOBOX_H 40
#define BOBOX_R 3

#define INFOBOX_X 10 // Info Button Box area X
#define INFOBOX_Y 123
#define INFOBOX_W 46
#define INFOBOX_H 51
#define INFOBOX_R 3

#define PULSEBOX_X 77 // Pulse On/Off Button Box area X
#define PULSEBOX_Y 179
#define PULSEBOX_W 46
#define PULSEBOX_H 51
#define PULSEBOX_R 3

#define PCBOX_X 20 // Pulse Current Settings Button Box area X
#define PCBOX_Y 112
#define PCBOX_W 40
#define PCBOX_H 40
#define PCBOX_R 3

#define PSBOX_X 20 // Pulse Settings Left/Right Button Box area X
#define PSBOX_Y 55
#define PSBOX_W 40
#define PSBOX_H 40
#define PSBOX_R 3

#define SETBOX_X 10 // Settings Button Box area X
#define SETBOX_Y 179
#define SETBOX_W 46
#define SETBOX_H 51
#define SETBOX_R 3

#define SNDBOX_X 10 // Sound Button Box area X
#define SNDBOX_Y 67
#define SNDBOX_W 46
#define SNDBOX_H 51
#define SNDBOX_R 3

#define VOLTBOX_X 155 // Volt Box area X
#define VOLTBOX_Y 125 // Volt Box area Y
#define VOLTBOX_H 70  // Volt Box Height
#define VOLTBOX_W 110 // Volt Box Width
#define VOLTVAL_H 70  // Height of Volt Value Font

#define BATTERY_X 260 // Battery Icon X Location
#define BATTERY_Y 220 // Battery Icon Y Location
#define BATTERY_W 22  // Battery Icon Width
#define BATTERY_H 10  // Battery Icon Height
#define CAUTION_X 270 // Caution Icon X Location
#define CAUTION_Y 190 // Caution Icon Y Location
#define HEART_X 295   // Heart Icon X Location
#define HEART_Y 218   // Heart Icon Y Location
#define SCREEN_W (tft.width() - 1)
#define SCREEN_H (tft.height() - 1)

// Touchscreen Definitions
#define TS_MINX 3800 // Calibration points for touchscreen
#define TS_MAXX 250
#define TS_MINY 200
#define TS_MAXY 3750
#define TOUCH_DBNC 150 // Touch Screen Debounce time, in mS.

// Special Color Definitions, RGB565 format.
// Online color picker: http://www.barth-dev.de/online/rgb565-color-picker/
#define BLACK 0x0000
#define BLUE 0x001F
#define LIGHT_BLUE 0x95BA
#define MED_BLUE 0x5C57
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF
#define DOSEBACKGROUND 0x0455
#define BUTTONBACKGROUND 0x6269
#define ARC_BG_COLOR ILI9341_ORANGE

// Misc defines
#define DEG2RAD 0.0174532925

// EOF
