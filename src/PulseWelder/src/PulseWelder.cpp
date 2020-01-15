/*
   File: PulseWelder.cpp
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.2
   Creation: Sep-11-2019
   Revised: Dec-29-2019.
   Public Release: Jan-03-2020
   Project Leader: T. Black (thomastech)
   Contributors: thomastech, hogthrob

   (c) copyright T. Black 2019-2020, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.

   Revision History:
   V1.0, Oct-30-2019: Initial public release by thomastech.
   v1.1, Jan-03-2020:
    - Moved customized ESP32_BLE and INA219 libraries to /lib folder (removed from /.pio/libdeps/lolin_d32_pro folder).
    - Revised platformio.ini, lib_deps section now has current stable release snapshots:
        Adafruit GFX Library = V1.7.3
        Adafruit ILI9341 = V1.5.3
        XPT2046_Touchscreen = build 26b691b2c8
    - Updated config.h (User Preference Settings):
        Added more build settings to further customize Sparky.
        Added pre-processor error testing to detect unusual config entries.
    - Added new Remote Key FOB feature:
        If Arc current is turned off the Remote FOB can now turn it back on with FOB button press.
    - Updated Over-Heat alarm (sensed front panel's OC LED):
        Improved screen handling of alert.
        Prevent user from changing some touchscreen entries during the alert.
        Prevent Key FOB from altering settings during alert state. Voice announce alert message instead.
    - Updated harware failure boot actions (bad current sensor or digital pot):
        Prevent Key FOB from changing current.
        Halt all operation after posting Hardware Failure screen.
    - Updated Arc Pulse Mode:
        Pulse modulated current is momentarily delayed at each new rod strike to allow arc stabilization.
        Added PULSE_AMPS_THRS to config.h. This sets the minimum rod current before pulse modulation is allowed.
    - Added hogthrob's PR #1, https://github.com/thomastech/Sparky/pull/1
        MCP41HV51 chip support. Code will autodetect if there is a MCP45HV51 connected via I2C or a MCP41HV51 via SPI.
        MCP41HV51 uses GPIO26 for CS and the normal MISO/MOSI/SCLK lines (same as touchscreen and display).
    - Added hogthrob's PR #2, https://github.com/thomastech/Sparky/pull/2
        Correction to ina219.configure() calling parameters.
        New user option for enabling 32-sample INA219 shunt averaging (hardware avg) in config.h. Enabled by Default.
    - Added hogthrob's PWM Controller Shutdown function (Optional Feature).
        Lift PIN-10 on SG3525A PWM Controller IC. Connect lifted pin to ESP32's SHDN_PIN (default is ESP32 GPIO-15).
        PWM Shutdown feature must be enabled in config.h (via PWM_ARC_CTRL define).
    - Added hogthrob's checkAndUpdateEEPROM() function & IS_IN_BOX() macro to streamline screen.cpp code.
    V1.2, Jan-14-2020:
     - Incorporated hogthrob's PR #5:
       No functional changes, maintanence only.
       Updated INA219 library, improved response time.
       Removed monitor port directive from platformio.ini.
       Sound and Screen Handling refactoring.

   Notes:
   1. This "Arduino" project must be compiled with VSCode / Platformio. Do not use the Arduino IDE.
   2. The INA219, XT_DAC_VOL and BLE libraries are custom patched; They were modified for use with this project.
   3. The INA219 "High-Side" current sensor is being used in a Low-side configuration. Therefore Bus voltage and
      power measurements are not available.
   4. MUST remove the existing shunt resistor on the Adafruit/clone INA219 PCB. See project documentation.
   5. Future Feature Wishlist Ideas (not implemented):
      Anti-Stick: Severely reduce output current if low weld voltage occurs for a prolonged period.
      Arc Force (Dig): Proportionly increase output current during voltage sags.
      Hot Start: Momemtarily increase output current during arc starts. Typical period is 0.5S.
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include "INA219.h"
#include "PulseWelder.h"
#include "screen.h"
#include "digPot.h"
#include "config.h"
#include "speaker.h"

// INA219 Current Sensor
INA219 ina219;

// LCD Touchscreen Setup
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TS_CS);

// Global System Vars
int  Amps             = 0;                  // Measured Welding Amps (allow +/- range).
byte arcSwitch        = DEF_SET_ARC;        // Welder Arc Current On/Off state. Pseudo Boolean, byte for EEPROM.
bool bleConnected     = false;              // Flag, Bluetooth is connected to FOB.
byte bleSwitch        = DEF_SET_BLE;        // Bluetooth Low Energy On/Off Switch. Pseudo boolean, byte used for EEPROM.
int  buttonClick      = CLICK_NONE;         // Bluetooth FOB Button click type, single or double click.
bool i2cInitComplete  = false;              // Flag, Shows that the i2c port has been configured.
bool overTempAlert    = false;              // Flag, Over-Temperature Alarm.
bool pulseState       = true;               // Arc Pulse modulation state (on/off).
byte pulseSwitch      = DEF_SET_PULSE;      // Pulse Mode On/Off state. Pseudo Boolean; Byte used for EEPROM.
byte pulseFreqX10     = DEF_SET_FRQ_X10;    // Arc modulation frequency for Pulse mode.
byte pulseAmpsPc      = DEF_SET_PULSE_AMPS; // Arc modulation Background Current (%) for Pulse mode.
byte setAmps          = DEF_SET_AMPS;       // Default Welding Amps *User Setting*.
bool setAmpsTimerFlag = false;              // Flag, User has Changed Amps Setting.
bool spiInitComplete  = false;              // SPI Port Initialization is Complete flag.
byte spkrVolSwitch    = DEF_SET_VOL;        // Audio Volume, five levels.
byte systemError      = ERROR_NONE;         // General hardware error state (bad current sensor or bad Digital Pot).
unsigned int Volts    = 0;                  // Measured Welding Volts.

// *********************************************************************************************

void setup()
{
  static long currentMillis = 0;

  delay(500);                   // Allow power to stabilize.
  WiFi.mode(WIFI_OFF);          // Disable WiFi, Not Used. Bluetooth not affected.
  Serial.begin(BAUD_RATE);      // Use User Config baud rate for Serial Log Messages.

  pinMode(  TFT_CS, OUTPUT);    // TFT Select, Output
  pinMode( LED_PIN, OUTPUT);    // LED Drive, Output
  pinMode(  OC_PIN, INPUT);     // Front Panel OC Warning LED, Input. This pin does not support internal pullups.
  pinMode(SHDN_PIN, OUTPUT);    // PWM Shutdown, Output

  digitalWrite( LED_PIN, LED_ON);
  digitalWrite(SHDN_PIN, HIGH); // Disable the PWM Controller.

  //  pinMode(DEBUG_PIN, OUTPUT); // Debug Test Output
  //  digitalWrite(DEBUG_PIN, 0);

  Serial.flush();
  Serial.println(                                "\n\n");
  Serial.println("Pulse Welder Controller Starting ...");

  digitalWrite(TFT_RST, LOW);
  delay(250);
  digitalWrite(TFT_RST, HIGH);
  delay(250);

  // Setup the TFT and Touch Array.
  ts.begin();                    // Initialize Touch Sensor Array.
  ts.setRotation(3);             // Home is upper left. Reversed x,y.
  tft.begin();                   // Initialize TFT Display.
  tft.setRotation(1);            // Home is upper left.
  tft.fillScreen(ILI9341_BLACK); // CLS.
  Serial.println("Initialized TFT Display & Touch Sensor.");

  // Initialize EEPROM emulation.
  EEPROM.begin(512);

  if (EEPROM.read(INIT_ADDR) != INIT_BYTE)
  {
    // The default values were already initialized in the vars' declarations, as follows:
    //        arcSwitch = DEF_SET_ARC;
    //        bleSwitch = DEF_SET_BLE;
    //        pulseSwitch = DEF_SET_PULSE;
    //        setAmps = DEF_SET_AMPS;
    //        pulseAmpsPc = DEF_SET_AMPS;
    //        spkrVolSwitch = DEF_SET_VOL;
    //        pulseFreqX10 = DEF_SET_FRQ_X10;

    EEPROM.write(      INIT_ADDR, INIT_BYTE);
    EEPROM.write(   AMP_SET_ADDR, setAmps);
    EEPROM.write(   VOL_SET_ADDR, spkrVolSwitch);
    EEPROM.write( PULSE_FRQ_ADDR, pulseFreqX10);
    EEPROM.write(  PULSE_SW_ADDR, pulseSwitch);
    EEPROM.write(    ARC_SW_ADDR, arcSwitch);
    EEPROM.write(    BLE_SW_ADDR, bleSwitch);
    EEPROM.write(PULSE_AMPS_ADDR, pulseAmpsPc);
    EEPROM.commit();
    Serial.println("Initialized Virgin EEPROM (detected first use).");
  }
  else
  {
    setAmps = EEPROM.read(AMP_SET_ADDR);
    setAmps = constrain(setAmps, MIN_SET_AMPS, MAX_SET_AMPS);

    spkrVolSwitch = EEPROM.read(VOL_SET_ADDR);
    spkrVolSwitch = constrain(spkrVolSwitch, VOL_OFF, XHI_VOL);

    pulseFreqX10 = EEPROM.read(PULSE_FRQ_ADDR);
    pulseFreqX10 = constrain(pulseFreqX10, MIN_PULSE_FRQ_X10, MAX_PULSE_FRQ_X10);

    pulseSwitch = EEPROM.read(PULSE_SW_ADDR);
    pulseSwitch = constrain(pulseSwitch, PULSE_OFF, PULSE_ON);

    arcSwitch = EEPROM.read(ARC_SW_ADDR);
    arcSwitch = constrain(arcSwitch, ARC_OFF, ARC_ON);

    bleSwitch = EEPROM.read(BLE_SW_ADDR);
    bleSwitch = constrain(bleSwitch, BLE_OFF, BLE_ON);

    pulseAmpsPc = EEPROM.read(PULSE_AMPS_ADDR);
    pulseAmpsPc = constrain(pulseAmpsPc, MIN_PULSE_AMPS_PC, MAX_PULSE_AMPS_PC);

    Serial.println("Restored settings from EEPROM.");
    Serial.println(            " -> Welding Amps: " + String(setAmps) + "A");
    Serial.println(            " -> Volume Level: " + String(spkrVolSwitch) + "%");
    Serial.println(            " -> Pulse Switch: " + String((pulseSwitch == PULSE_ON ? "On" : "Off")));
    Serial.println(            " -> Pulse Freq  : " + String(PulseFreqHz(), 1) + "Hz");
    Serial.println(            " -> Pulse Amps  : " + String(pulseAmpsPc) + "%");
    Serial.println(            " -> Arc Switch  : " + String((arcSwitch == ARC_ON ? "On" : "Off")));
    Serial.println(            " -> Bluetooth Sw: " + String((bleSwitch == BLE_ON ? "On" : "Off")));
  }

  // Setup ADC
  initVdcAdc(); // Initialize the Welding Voltage ADC.
  Serial.println("Initialized ADC.");

  // Setup the INA219 Current Sensor.
  if (initCurrentSensor() == false) {
    systemError |= ERROR_INA219;
  }

  // Setup Digital Pot. Must setup INA219 before the Digital Pot due to the shared i2c.
  if (initDigitalPot(POT_I2C_ADDR, POT_CS) == false) {
    systemError |= ERROR_DIGPOT;
  }

  // Set Arc Weld Current (Update Digital Pot and PWM Control pin).
  controlArc(arcSwitch, VERBOSE_ON);

  // Init SPIFFS (SPIFFS is not used in this project).
  // spiffsInit();
  // Serial.println("SPIFFS: Initialization Complete");

  // Post splash screen before Bluetooth init.
  currentMillis = millis();
  displaySplash(); // Show Splash Image.
  scanBlueTooth(); // Find the BLE handheld iTag Button FOB. Will take a few seconds.

  while (millis() < currentMillis + SPLASH_TIME) {} // Give user time to see Splash Screen.

  // Misc House Keeping, data initialization
  resetCurrentBuffer();
  resetVdcBuffer();

  // Setup Hardware Interrupts (Not used).
  // attachInterrupt(interruptPin, isr, FALLING);

  // Initialize Audio Voice and tones.
  spkr.volume(spkrVolSwitch); // Set Master-Volume (0-100 allowed). This is a Menu setting.
  spkr.playToEnd(beep);        // Init audio, Beep user.

  Serial.println("Initialized Audio Playback System.");

 // Welcome the user with a promotional voice message.
  spkr.play(promoMsg);

  // Done with initialization. Show Home Page or Hardware Error Page.
  if (systemError == ERROR_NONE) {  // Hardware is OK.
    drawHomePage();
    Serial.println("System Initialization Complete: Success!");
  }
  else {                              // Hardware Problem!
    pulseSwitch = PULSE_OFF;
    disableArc(VERBOSE_ON);           // Turn Off PWM controller IC.
    setPotAmps(MIN_AMPS, VERBOSE_ON); // Minimize Amps even if Digital POT is non-functional.
    #ifdef DEMO_MODE
     Serial.println("System Warning: Operating in Demo Mode; Do NOT attempt to weld.");
     drawHomePage();
    #else
     Serial.println("System Hardware Fails! Repair needed; Do NOT attempt to weld.");
     drawErrorPage();                 // Post Hardware Failure Screen.
     Serial.flush();
     while(true) {                    // HALT the welder using infinite loop.
        showHeartbeat();              // Flash Red Caution Icon.
     }
    #endif
  }
  Serial.flush();
}

// *********************************************************************************************
// Main Loop.
void loop()
{
  static long currentMillis      = 0; // Led Flash Timer.
  static long previousBleMillis  = 0; // Timer for Bluetooth BLE scan.
  static long previousMeasMillis = 0; // Timer for Measurement.

  // Housekeeping.
  currentMillis = millis();

  // System Tick Timers Tasks
  if (currentMillis - previousMeasMillis >= MEAS_TIME) {
    previousMeasMillis = currentMillis;
    measureCurrent();
    measureVoltage();
  }

  if (currentMillis - previousBleMillis > CHK_BLE_TIME) {
    previousBleMillis = millis();
    checkBleConnection(); // Check the Bluetooth iTAG FOB Button server connection.
  }

  // Background tasks
  spkr.fillBuffer();     // Fill the sound buffer with data.
  showHeartbeat();       // Display Flashing Heartbeat icon.
  checkForAlerts();      // Check for alert conditions.
  processScreen();       // Process Menu System (touch screen).
  pulseModulation();     // Update the Arc Pulse Current if pulse mode is enabled.
  remoteControl();       // Check the BLE FOB remote control for button presses.
}

// *********************************************************************************************

/*
   // Hardware interrupts are not used in this project. This isr is a placeholder for future use.
   //  Warning: It is not possible to execute delay() or yield() from an ISR, or do blocking operations,
   // or operations that disable the interrupts. Code MUST be short & sweet, such as set a flag then exit.
   // Function protype must use ICACHE_RAM_ATTR type.
   ICACHE_RAM_ATTR void isr() // interrupt service routine
   {
   }
 */

// EOF