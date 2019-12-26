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

   Notes:
   1. This "Arduino" project must be compiled with VSCode / Platformio. Do not use the Arduino IDE.
   2. The INA219, XT_DAC_VOL and BLE libraries are custom patched; They were modified for use with this project.
   3. The INA219 "High-Side" current sensor is being used in a Low-side configuration. Therefore Bus voltage and
      power measurements are not available.
   4. MUST remove the existing shunt resistor on the Adafruit/clone INA219 PCB. See project documentation.
   5. Future Feature Wishlist Ideas (not implemented):
      Hot Start (anti-stick): Auto-Increase current during arc starts.
      Arc Force (Dig): Auto-Increase current during voltage sags.
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include "INA219.h"
#include "PulseWelder.h"
#include "XT_DAC_Audio.h"
#include "dacAudio.h"
#include "screen.h"
#include "digPot.h"
#include "config.h"

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
byte spkrVolSwitch    = DEF_SET_VOL;        // Audio Volume, five levels.
byte systemError      = ERROR_NONE;         // General hardware error state (bad current sensor or bad Digital Pot).
unsigned int Volts    = 0;                  // Measured Welding Volts.

// *********************************************************************************************

void setup()
{
  static long currentMillis = 0;

  delay(500);               // Allow power to stabilize.
  WiFi.mode(WIFI_OFF);      // Disable WiFi, Not Used. Bluetooth not affected.
  Serial.begin(BAUD_RATE);  // Use User Config baud rate for Serial Log Messages.

  pinMode( TFT_CS, OUTPUT); // TFT Select Output
  pinMode(LED_PIN, OUTPUT); // LED Output
  pinMode( OC_PIN, INPUT);  // Over Current Input. This pin does not support internal pullups.
  digitalWrite(LED_PIN, LED_ON);

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
    pulseAmpsPc = constrain(pulseAmpsPc, MIN_PULSE_AMPS, MAX_PULSE_AMPS);

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

#ifdef DEMO_MODE
  systemError = ERROR_NONE; // Clear hardware error codes (if any) when in Demo Mode.
#endif // ifdef DEMO_MODE

  // Set Welding Current (Digital Pot).
  if (arcSwitch == ARC_ON) {
    setPotAmps(setAmps, true);
  }
  else {
    setPotAmps(SET_AMPS_DISABLE, true);
  }

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
  promoMsg.Speed     = 1.0;           // Normal Playback Speed.
  promoMsg.Volume    = 127;           // Maximum Sub-Volume (0-127 allowed).
  DacAudio.DacVolume = spkrVolSwitch; // Set Master-Volume (0-100 allowed). This is a Menu setting.
  DacAudio.Play(&beep, false);        // Init audio, Beep user.

  while (beep.TimeLeft)               // Wait until beep tone has finished playing.
  {
    DacAudio.FillBuffer();
  }
  Serial.println("Initialized Audio Playback System.");

  if (spkrVolSwitch != VOL_OFF) {
    DacAudio.Play(&promoMsg, true); // Welcome the user with a short promotional message.
  }

  // Done with initialization. Show home Page and announce "promo" message.
  if (systemError == ERROR_NONE) {
    drawHomePage();
    Serial.println("System Initialization Complete.");
  }
  else {
    drawErrorPage();
    Serial.println("System Initialization Complete, Hardware Fails.");
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
  overTempAlert = !digitalRead(OC_PIN); // Get Over-Temperature Signal.

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
  DacAudio.FillBuffer(); // Fill the sound buffer with data.
  showHeartbeat();       // Display Flashing Heartbeat icon.
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
