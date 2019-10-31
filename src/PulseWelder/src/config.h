/*
   File: PulseWelder.cpp
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.0
   Creation: Sep-11-2019
   Revised: Oct-24-2019
   Release: Oct-30-2019
   Author: T. Black
   (c) copyright T. Black 2019, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
 */

// Misc Defines
// #define DEMO_MODE            // Uncomment to Disable hardware tests. Useful for screen demo without I2C hardware installed.

// Serial Baud Rate for Log Messages
#define BAUD_RATE 115200

// BLE Button FOB Defines (choose only one!)
#define FOB_TYPE  iTAG_FOB      // iTAG BLE Button FOB (52mm long Teardrop shaped button).
//#define FOB_TYPE  TrackerPA_FOB // TrackerPA BLE Button FOB (38mm Square button).

// BLE Remote Control defines
#define REMOTE_AMP_CHG 5        // Number of Amps to inc/dec on each Bluetooth iTAG FOB Button press.

// INA219 Defines
#define SHUNT_OHMS 0.000525     // (0.00055) ZX7-200 internal Shunt ohms. Reduce value to increase measured current.
#define SHUNT_V_MAX 0.125       // Maximum voltage across shunt, in VDC.

// Welding Amps Defines
#define MAX_AMPS 105            // Used in Digital Pot Calcs. Actual Maximum Amps provided by Welder is 100A.
#define MIN_AMPS 45             // Used in Digital Pot Calcs. Actual Minimum Amps provided by Welder is 47A.

// Default Settings Defines. Used on maiden boot only. Perform full Flash erase if needed on chip re-flash.
#define DEF_SET_AMPS 70         // Default Welding Current on maiden Boot.
#define DEF_SET_ARC ARC_ON      // ARC_ON, ARC_OFF
#define DEF_SET_BLE BLE_ON      // BLE_ON, BLE_OFF
#define DEF_SET_FRQ_X10 10      // Value times 10! 0.5 - 0.9 (0.1 incr) and 1.0 - 5.0 (1.0 incr) allowed.
#define DEF_SET_PULSE PULSE_OFF // PULSE_ON, PULSE_OFF
#define DEF_SET_VOL VOL_MED     // VOL_OFF,F VOL_LOW, VOL_MED, VOL_HI, VOL_XHI

// EOF