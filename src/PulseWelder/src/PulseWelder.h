/*
   File: PulseWelder.h
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.0
   Creation: Sep-11-2019
   Revised: Sep-24-2019
   Release: Oct-30-2019
   Author: T. Black
   (c) copyright T. Black 2019, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
 */

#include <BLEDevice.h>
#include "config.h"

// *********************************************************************************************
// VERSION STRING: Must be updated with each release!
#define VERSION_STR "V1.0"

// *********************************************************************************************
// Misc Pin Definitions
// GPIOs 34-39 do not support internal pullups.
#define DAC_PIN 25
#define LED_PIN LED_BUILTIN
#define OC_PIN 34
#define VP_PIN 36
#define VN_PIN 39
#define POT_CS 26 // if using SPI POT MCP41HV51, not I2C MCP45HV51
// #define DEBUG_PIN 2

// TFT Pin Definitions (Using LoLin D32 Pro defaults)
#define SDA_PIN 15
#define SCL_PIN 13
#define TFT_CS 14
#define TFT_DC 27
#define TFT_LED 32
#define TFT_RST 33
#define TS_CS 12

// Audio Player Defines.
#define VOL_OFF 0  // Do Not change VOL_OFF define!
#define VOL_LOW 15 // Feel free to change all other audio volume levels. 0 to 100 allowed.
#define VOL_MED 35
#define VOL_HI 65
#define XHI_VOL 100

// Amps & Volts Defines.
#define MAX_SET_AMPS MAX_AMPS     // Maximum amps setting. Typically set to the max measured welding current.
#define MIN_SET_AMPS MIN_AMPS     // Minimum amps setting. Use current measured at lowest possible POT setting.
#define MIN_VOLTS 18              // Minimum volts, else warn user.
#define MIN_DET_AMPS 2            // Minimum arc amps to detect current flow.
#define AMPS_MIN_FOR_PULSE 40     // Minimum realtime welding current for Pulse Modulation.
#define SET_AMPS_DISABLE MIN_AMPS // Amps Setting when Arc Switch is turned off. Must be >= MIN_SET_AMPS.

// Acr Mode Defines
#define ARC_OFF 0                 // Arc Current On.
#define ARC_ON 1                  // Arc Current Off (supressed).

// BlueTooth Defines
#define BLE_OFF 0                 // Bluetooth Low Energy On.
#define BLE_ON 1                  // Bluetooth Low Energy Off.
#define BLE_SCAN_TIME 10          // Bluetooth Scan time, in Secs.
#define BLE_RESCAN_TIME 4         // Bluetooth Re-Scan time, in Secs.
#define CLICK_NONE 0              // Bluetooth Button FOB, no clicks.
#define CLICK_SINGLE 1            // Bluetooth Button FOB, One click.
#define CLICK_DOUBLE 2            // Bluetooth Button FOB, Two clicks.
#define CLICK_BUSY 3              // Bluetooth Button FOB, busy processing click counter.
#define RECONNECT_TRIES 10        // Bluetooth max attempted auto reconnect count before giving up.

// EEPROM Defines.
#define INIT_BYTE 0xA5            // EEProm Initialization Stamping Byte.
#define INIT_ADDR 0               // E2Prom Address for Init byte.
#define AMP_SET_ADDR 1            // E2Prom Address for Amps setting.
#define VOL_SET_ADDR 2            // E2Prom Address for Audio Volume setting.
#define PULSE_SW_ADDR 3           // E2Prom Address for Pulse mode On/Off Switch.
#define PULSE_FRQ_ADDR 4          // E2Prom Address for Pulse Frequency (scaled 10X) setting.
#define PULSE_AMPS_ADDR 5         // E2Prom Address for Pulse Amps (%).
#define ARC_SW_ADDR 6             // E2Prom Address for Arc On/Off Switch.
#define BLE_SW_ADDR 7             // E2Prom Address for BlueTooth On/Off Switch.

// FOB Defines.
#define iTAG_FOB 1
#define TrackerPA_FOB 2

// INA219 Defines.
#define INA219_ADDR 0x40          // i2c address of Current sensor.
#define BUS_RANGE_16V 0           // We don't measure Bus voltage, but it will get set to 16V.
#define CONTINUOS_OP_NO_VOLTAGE 5 // Continuous Sampling of Shunt but not Bus voltage, no trigger required.
#define CONTINUOS_OP            7 // Continuous Sampling of Shunt and Bus voltage, no trigger required.
#define PGA_RANGE_160MV 2         // Set PGA gain to 160mv to allow our expected shunt voltage range.
#define PGA_RANGE_320MV 3         // Set PGA gain to 320mV to allow our expected shunt voltage range.
#define SAMPLE_AVG_128 15         // 128 Samples takes 69mS per acq.
#define SAMPLE_9BITS    0         // 9 bits takes 84uS per acq.
#define SAMPLE_12BITS   3         // 12 bits takes 532uS per acq.

#define BUS_V_MAX 1.0             // Maximum Bus voltage (same as shunt voltage), in VDC.
#define MAX_I_EXPECTED 200.0      // Maximum Expected Current, in Amps.

// LED Defines
#define LED_OFF HIGH              // Built-In LED Off state.
#define LED_ON LOW                // Built-In LED On state.

// Pulse Defines
#define DECR 0                    // Decrement Pulse Freq.
#define INCR 1                    // Increment Pulse Freq.
#define MIN_PULSE_FRQ_X10 4       // Min Pulse Freq (times 10).
#define MAX_PULSE_FRQ_X10 50      // Max Pulse Freq (times 10).
#define PULSE_OFF 0               // Pulse Mode Off.
#define PULSE_ON 1                // Pulse Mode On.
#define PULSE_AMPS_STEP 5         // Amps Step size for Pulse Amps setting.
#define DEF_SET_PULSE_AMPS 50     // Default Pulse Amps (%).
#define MIN_PULSE_AMPS 35         // Minimum Pulse Amps (%).
#define MAX_PULSE_AMPS 90         // Maximum Pulse Amps (%).

// System Error defines
#define ERROR_NONE   0b00000000
#define ERROR_INA219 0b00000001  // INA219 Current Sensor Hardware failure, bit position D0.
#define ERROR_DIGPOT 0b00000010  // MCP45HV51 Digital POT Hardware failure, bit position D1.

// Timers
#define CHK_BLE_TIME 250         // Check Bluetooth Connection Timer, in mS.
#define DAC_ISR_TMR 0            // DAC Audio Interrupt Timer to Use.
#define DOUBLE_CLICK_TIME 750    // Bluetooth FOB Button Click Timer, in mS.
#define EEP_DELAY_TIME 3500      // Delay Time before writing Volume value to EEPROM.
#define HB_FLASH_TIME 500        // Heartbeat & LED FLASH Update Time, in mS.
#define MEAS_TIME 65             // Measurement Update Time, in mS. Set to INA219 Conversion Time.
#define RECONNECT_DLY_TIME 20000 // Delay time before attempting a Bluetooth re-connect.
#define SPLASH_TIME 2500         // Timespan for showing Splash Screen at boot.

// *********************************************************************************************

// Amps & Volts Measurement Prototypes
void initVdcAdc(void);
void measureCurrent(void);
void measureVoltage(void);
void resetCurrentBuffer(void);
void resetVdcBuffer(void);

// Bluetooth Prototypes
bool connectToServer(BLEAddress pAddress);
bool isBleServerConnected(void);
bool isBleDoScan(void);
void onResult(BLEAdvertisedDevice advertisedDevice);
void checkBleConnection(void);
void processFobClick(void);
void reconnectBlueTooth(int secs);
bool reconnectTimer(bool rst);
void scanBlueTooth(void);
void setupBle(unsigned int scanSeconds);
void stopBle(void);

// Digital Pot Protoypes
bool digitalPotWrite(byte dataValue,
                     byte memAddr);
byte digitalPotRead(byte memAddr);
bool initDigitalPot(byte chipAddr, int spi_cs);
void setPotAmps(byte ampVal,
                bool verbose);

// Display Prototypes
bool adjustPulseFreq(bool direction);
void displayAmps(bool forceRefresh);
void displayOverTempAlert(void);
void displaySplash(void);
void displayVolts(bool forceRefresh);
void drawAmpsBox(void);
void drawAmpBar(int  x,
                int  y,
                bool forceRefresh);
void drawBattery(int x,
                 int y);
void drawCaution(int  x,
                 int  y,
                 bool state);
void drawHeart(int  x,
               int  y,
               bool state);
void drawErrorPage(void);
void drawHomePage(void);
void drawInfoPage(void);
void drawInfoPage6011(void);
void drawInfoPage6013(void);
void drawInfoPage7018(void);
void drawPulseAmpsSettings(void);
void drawPulseHzSettings(void);
void drawPulseIcon(void);
void drawPulseLightning(void);
void drawSettingsPage(void);
void getTouchPoints(void);
void processScreen(void);
void showBleStatus(int command);
void showHeartbeat(void);
void updateVolumeIcon(void);

// Graphics Prototypes
void fillArc(int          x,
             int          y,
             int          start_angle,
             int          seg_count,
             int          rx,
             int          ry,
             int          w,
             unsigned int color);

// Misc Prototypes
void  AddNumberToSequence(int theNumber);
bool  initCurrentSensor(void);
int   getFobClick(bool rst);
void  remoteControl(void);
float PulseFreqHz(void);
void  pulseModulation(void);

// SPIFFS Prototypes
void  spiffsInit(void);

// EOF
