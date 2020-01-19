/*
   File: PulseWelder.h
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.2
   Creation: Sep-11-2019
   Revised: Jan-14-2020
   Public Release: Jan-15-2020
   Revision History: See PulseWelder.cpp
   Project Leader: T. Black (thomastech)
   Contributors: thomastech, hogthrob

   (c) copyright T. Black 2019-2020, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
 */
#ifndef __PULSE_WELDER_H__
#define __PULSE_WELDER_H__

#include "BLEDevice.h"

// *********************************************************************************************
// VERSION STRING: Must be updated with each public release! The version is shown on the boot screen.
#define VERSION_STR "V1.2"

// *********************************************************************************************
// GPIO Pin Definitions
// GPIOs 34-39 do not support internal pullups.
#define DAC_PIN 25          // DAC Audio, output.
#define LED_PIN LED_BUILTIN // ESP32 board LED, output.
#define OC_PIN 34           // Conntects to Front Panel OC Warning LED (LED removed), input.
#define POT_CS 26           // Chip Select for SPI Digital POT MCP41HV51, output.
#define SHDN_PIN 15         // Optional PWM Shutdown, output.
#define VP_PIN 36           // Attenuated live Welding Voltage, Input.
#define VN_PIN 39           // Not used.
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
#define MIN_VOLTS 18          // Minimum output volts. Change volts text color if VDC too low.
#define MIN_DET_AMPS 2        // Minimum output welding current before reporting measured Amps.
#define AMPS_MIN_FOR_PULSE 40 // Minimum output welding current for Pulse Modulation.

// Arc Mode Defines
#define ARC_OFF 0             // Arc Current Off, boolean flag.
#define ARC_ON 1              // Arc Current On, boolean flag.
#define PWM_ON LOW            // SG3525A PWM Controller chip is On, Arc current enabled.
#define PWM_OFF HIGH          // SG3525A PWM Controller chip is Off, Arc Current disabled.

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
#define INA219_ADDR 0x40          // i2c address of INA219 Current Sensor chip.
#define BUS_RANGE_16V   0         // We don't measure Bus voltage, but it will get set to 16V.
#define CONTINUOUS_OP   7         // Continuous Sampling of Shunt and Bus voltage, no trigger required.
#define CONTINUOUS_OP_NO_VDC 5    // Continuous Sampling of Shunt but not Bus voltage, no trigger required.
#define PGA_RANGE_40MV  0         // PGA gain 40mv.
#define PGA_RANGE_80MV  1         // PGA gain 80mv.
#define PGA_RANGE_160MV 2         // PGA gain 160mv.
#define PGA_RANGE_320MV 3         // PGA gain 320mV.
#define SAMPLE_AVG_2    9         // 12 bits, 2 Samples,  1.1mS conversion time.
#define SAMPLE_AVG_4   10         // 12 bits, 4 Samples,  2.1mS conversion time.
#define SAMPLE_AVG_8   11         // 12 bits, 8 Samples,  4.2mS conversion time.
#define SAMPLE_AVG_16  12         // 12 bits, 16 Samples, 8.5mS conversion time.
#define SAMPLE_AVG_32  13         // 12 bits, 32 Samples, 17mS conversion time.
#define SAMPLE_AVG_64  14         // 12 bits, 64 Samples, 34mS conversion time.
#define SAMPLE_AVG_128 15         // 12 bits, 128 Samples,68mS conversion time.
#define SAMPLE_9BITS    0         // 9 bits, 84uS per acq.
#define SAMPLE_10BITS   1         // 10 bits, 148uS per acq.
#define SAMPLE_11BITS   2         // 11 bits, 276uS per acq.
#define SAMPLE_12BITS   3         // 12 bits, 532uS per acq.
#define BUS_V_MAX      1.0        // Maximum Bus voltage (same as shunt voltage), in VDC.
#define MAX_I_EXPECTED 200.0      // Maximum Expected Current, in Amps.

// LED Defines
#define LED_OFF HIGH              // Built-In LED Off state.
#define LED_ON LOW                // Built-In LED On state.

// Misc Defines
#define VERBOSE_OFF 0              // Disable Verbose Messages.
#define VERBOSE_ON  1              // Enable Verbose Messages.

// Pulse Defines
#define ARC_STABLIZE_TM  1250     // Arc Stabilize time for pulse, in mS.
#define DECR 0                    // Decrement Pulse Freq.
#define INCR 1                    // Increment Pulse Freq.
#define MIN_PULSE_FRQ_X10 4       // Min Pulse Freq (times 10).
#define MAX_PULSE_FRQ_X10 50      // Max Pulse Freq (times 10).
#define PULSE_OFF 0               // Pulse Mode Off.
#define PULSE_ON 1                // Pulse Mode On.
#define PULSE_AMPS_STEP 5         // Amps Step size for Pulse Amps setting.
#define DEF_SET_PULSE_AMPS 50     // Default Pulse Amps (%).
#define MIN_PULSE_AMPS_PC 35      // Minimum Pulse Amps as percent of current setting.
#define MAX_PULSE_AMPS_PC 90      // Maximum Pulse Amps as percent of current setting.

// System Error defines
#define ERROR_NONE   0b00000000
#define ERROR_INA219 0b00000001  // INA219 Current Sensor Hardware failure, bit position D0.
#define ERROR_DIGPOT 0b00000010  // Digital POT Hardware failure, bit position D1.

// Timers
#define CHK_BLE_TIME 250         // Check Bluetooth Connection Timer, in mS.
#define DAC_ISR_TMR 0            // DAC Audio Interrupt Timer to Use.
#define DOUBLE_CLICK_TIME 750    // Bluetooth FOB Button Click Timer, in mS.
#define EEP_DELAY_TIME 3500      // Delay Time before writing Volume value to EEPROM.
#define HB_FLASH_TIME 500        // Heartbeat & LED FLASH Update Time, in mS.
#define MEAS_TIME 5              // Measurement Refresh Time, in mS.
#define RECONNECT_DLY_TIME 20000 // Delay time before attempting a Bluetooth re-connect.
#define SPLASH_TIME 2500         // Timespan for showing Splash Screen at boot.

// *********************************************************************************************
enum StartMode {
  SCRATCH_START,
  LIFT_START
};

extern StartMode startMode;

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
bool initDigitalPot(byte chipAddr,
                    int  spi_cs);
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
void drawOverTempAlert(void);
void drawPulseAmpsSettings(bool update_only);
void drawPulseHzSettings(bool update_only);
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
void  controlArc(bool state,
                 bool verbose);
void  disableArc(bool verbose);
void  enableArc(bool verbose);
bool  initCurrentSensor(void);
int   getFobClick(bool rst);
void  checkForAlerts(void);
float PulseFreqHz(void);
void  pulseModulation(void);
void  remoteControl(void);
void detectArcState(void);

// SPIFFS Prototypes
void  spiffsInit(void);

#endif
// EOF
