/*
   File: config.h
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.1
   Creation: Sep-11-2019
   Revised: Dec-29-2019.
   Public Release: Jan-03-2020
   Revision History: See PulseWelder.cpp
   Project Leader: T. Black (thomastech)
   Contributors: thomastech

   (c) copyright T. Black 2019-2020, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
 */

// ************************************************************************************************************************
// Special Defines
// #define DEMO_MODE    // Use this to Disable hardware tests. Allows welder demo without functional MCP4XHV51 or INA219.

// ************************************************************************************************************************
// Serial Baud Rate for Log Messages.
#define BAUD_RATE 115200        // Serial Log Baud rate.

// ************************************************************************************************************************
// BLE Button FOB Defines (choose only one!)
#define FOB_TYPE  iTAG_FOB      // iTAG BLE Button FOB (52mm long Teardrop shaped button).
//#define FOB_TYPE  TrackerPA_FOB // TrackerPA BLE Button FOB (38mm Square button).

// ************************************************************************************************************************
// BLE Remote Control defines
#define REMOTE_AMP_CHG 5        // Number of Amps to inc/dec on each Bluetooth FOB Button press.

// ************************************************************************************************************************
// INA219 Defines
#define SHUNT_OHMS 0.000525     // ZX7-200's internal Shunt ohms. Reduce value to increase displayed realtime current.
//#define SHUNT_OHMS 0.000375   // Shunt value used by user @hogthrob. See https://github.com/thomastech/Sparky/issues/2
#define SHUNT_V_MAX 0.125       // Maximum voltage across shunt, in VDC.
#define INA219_AVG_ON           // Use 32 samples per Shunt Acquistion (hardware avg). Comment this line to disable.

// ************************************************************************************************************************
// Welding Amps & Volts Defines
#define ARC_OFF_AMPS MIN_AMPS   // Welder's output Amps when Arc is turned off. Must be >= MIN_AMPS.
#define MAX_AMPS 105            // Enter actual Maximum welder output Amps (100A typical). Use clamp-on ammeter to cal.
#define MIN_AMPS 45             // Enter actual Minimum welder output Amps (45A typical). Use clamp-on ammeter to cal.
#define MAX_SET_AMPS MAX_AMPS   // Maximum permitted welder output Amps. Must be <= MAX_AMPS.
#define MIN_SET_AMPS MIN_AMPS   // Minimum permitted welder output Amps. Must be >= MIN_AMPS.
#define PULSE_AMPS_THRS 50      // Pulse Amps Threshold. Current must meet or exceed this value to permit pulse modulation.

// ************************************************************************************************************************
// Optional PWM Arc current control (via PWM IC Shutdown). Requires modification to Welder's main control board.
// Hardware mod instructions: Lift SG3525A Pin-10 and connect lifted leg to ESP32's SHDN_PIN (default is GPIO-15).
// By using the shutdown pin of the SG3525A IC it is possible to fully turn off output current (as opposed
// to just setting it to the minimum Amps via the digital pot). If this software feature is not enabled the
// ESP32's PWM contoller is never turned off (Digital Pot will be set to the ARC_OFF_AMPS during the Arc Off state).
//
//#define PWM_ARC_CTRL          // Uncomment this line when using PWM ARC On/Off control.

// ************************************************************************************************************************
// Default Touch Screen Settings. Used on maiden boot only.
// Flash memory must be erased before code upload to force-load these settings on virgin boot.
// Platformio flash erase instructions: In IDE enter CTRL-ALT-T, select [Platformio:Erase Flash].
#define DEF_SET_AMPS 70         // Default Welding Current. Any int value between MIN_AMPS and MAX_AMPS, inclusive.
#define DEF_SET_ARC ARC_ON      // Default Arc Current On/Off; ARC_ON, ARC_OFF are allowed.
#define DEF_SET_BLE BLE_ON      // Default Bluetooth (Key FOB) On/Off; BLE_ON, BLE_OFF are allowed.
#define DEF_SET_FRQ_X10 10      // Default Pulse Freq Hz, times 10; Allowed int values: 4-9 (0.4-0.9) and 10-50 (1.0-5.0).
#define DEF_SET_PULSE PULSE_OFF // Default Pulse Mode; PULSE_ON, PULSE_OFF are allowed.
#define DEF_SET_VOL VOL_MED     // Default DAC Audio Volume; VOL_OFF, VOL_LOW, VOL_MED, VOL_HI, VOL_XHI are allowed.

// -----------------------------------------------------------------------------------------------------------------------
// Error Checks. Will force compiler error if suspect settings are entered.
// Fatherly Advice: Do not change the code shown below.

#if DEF_SET_AMPS > MAX_AMPS
 #error "DEF_SET_AMPS conflicts with MAX_AMPS. Correction in config.h is required."
#endif

#if DEF_SET_FRQ_X10 > MAX_PULSE_FRQ_X10
 #error "DEF_SET_FRQ_X10 value out of range (too high). Correction in config.h is required."
#endif

#if DEF_SET_FRQ_X10 < MIN_PULSE_FRQ_X10
 #error "DEF_SET_FRQ_X10 value out of range (too low). Correction in config.h is required."
#endif

#if DEF_SET_VOL > XHI_VOL
 #error "DEF_SET_VOL value out of range (too high). Correction in config.h is required."
#endif

#if DEF_SET_VOL < VOL_OFF
 #error "DEF_SET_VOL value out of range (too low). Correction in config.h is required."
#endif

#if MIN_AMPS > MAX_AMPS
 #error "MIN_AMPS conflicts with MAX_AMPS. Correction in config.h is required."
#endif

#if MIN_AMPS > ARC_OFF_AMPS
 #error "MIN_AMPS conflicts with ARC_OFF_AMPS. Correction in config.h is required."
#endif

#if MAX_SET_AMPS < MIN_SET_AMPS
 #error "MAX_SET_AMPS conflicts with MIN_SET_AMPS. Correction in config.h is required."
#endif

#if MAX_SET_AMPS > MAX_AMPS
 #error "MAX_SET_AMPS conflicts with MAX_AMPS. Correction in config.h is required."
#endif

#if MIN_SET_AMPS < MIN_AMPS
 #error "MIN_SET_AMPS conflicts with MIN_AMPS. Correction in config.h is required."
#endif
// -----------------------------------------------------------------------------------------------------------------------
// EOF