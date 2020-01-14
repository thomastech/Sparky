/*
   File: misc.cpp
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.1
   Creation: Sep-11-2019
   Revised: Dec-29-2019.
   Public Release: Jan-03-2020
   Revision History: See PulseWelder.cpp
   Project Leader: T. Black (thomastech)
   Contributors: thomastech, hogthrob

   (c) copyright T. Black 2019-2020, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
 */

#include <Arduino.h>
#include <Wire.h>
#include "digPot.h"
#include "PulseWelder.h"
#include "config.h"
#include "speaker.h"

// Global Wave Files.
extern XT_Wav_Class increaseMsg;
extern XT_Wav_Class currentOnMsg;
extern XT_Wav_Class decreaseMsg;
extern XT_Wav_Class overHeatMsg;
extern XT_Wav_Class promoMsg;

// Global System vars
extern int  Amps;            // Live Welding Current.
extern byte arcSwitch;       // Welding Arc On/Off Switch.
extern int  buttonClick;     // Bluetooth iTAG FOB Button click type, single or double click.
extern int  fobClick;        // Bluetooth FOB Button Click Value.
extern bool overTempAlert;   // Over Temperature (OC) Alert.
extern bool newFobClick;     // Bluetooth FOB Button, new click.
extern byte pulseAmpsPc;     // Arc modulation Background Current (%) for Pulse mode.
extern byte pulseFreqX10;    // Pulse Frequency times ten.
extern bool pulseState;      // Arc Pulse modulation state (on/off).
extern byte spkrVolSwitch;   // Audio Volume, five levels.
extern byte setAmps;         // Default Welding Amps *User Setting*.
extern byte pulseSwitch;     // Pulse Mode On/Off Flag. Pseudo Boolean; byte declared for EEPROM.

// *********************************************************************************************
// Check Welder's OC Led signal for alert condition. Could be over-heat or over-current state.
void checkForAlerts(void)
{
    overTempAlert = !digitalRead(OC_PIN); // Get OC Warning LED State.
    if(overTempAlert) {
        arcSwitch = ARC_OFF;
        disableArc(VERBOSE_OFF);         // Disable Arc current.
    }
}

// *********************************************************************************************
// Control Welding Arc Current.
// state = ARC_ON or ARC_OFF
// verbose = VERBOSE_ON (true) for expanded log messages, else VERBOSE_OFF (false) for less messages.
void controlArc(bool state, bool verbose)
{
  if (state == ARC_ON)
  {
    enableArc(verbose);
  }
  else
  {
    disableArc(verbose);
  }
}

// *********************************************************************************************
// Disable the Arc current.
// verbose = VERBOSE_ON (true) for expanded log messages, else VERBOSE_OFF (false) for less messages.
// PWM Shutdown control option requires hardware mod; Lift SG3525A pin 10, connect it
// to ESP32's SHDN_PIN (default is GPIO15)
void disableArc(bool verbose)
{
  arcSwitch = ARC_OFF;

  setPotAmps(ARC_OFF_AMPS, verbose); // Set Digital Pot to lowest welding current.

  #ifdef PWM_ARC_CTRL
   digitalWrite(SHDN_PIN, PWM_OFF);  // Disable PWM Controller.
   if (verbose == VERBOSE_ON) {
     Serial.println("Arc Current Turned Off (Disabled PWM Controller).");
   }
  #else
   digitalWrite(SHDN_PIN, PWM_ON); // PWM feature disabled by config.h; Don't shutdown!
   if (verbose == VERBOSE_ON) {
     Serial.println("Arc Current Suppressed (Reduced to " + String(ARC_OFF_AMPS) + " Amps).");
   }
  #endif
}

// *********************************************************************************************
// Enable the Arc current.
// verbose = VERBOSE_ON (true) for expanded log messages, else VERBOSE_OFF (false) for less messages.
// PWM Shutdown control option requires hardware mod; Lift SG3525A pin 10, connect ot
// to ESP32's SHDN_PIN (default is GPIO15)
void enableArc(bool verbose)
{
    if(overTempAlert) {
        if (verbose == VERBOSE_ON) {
            Serial.println("Arc Current Cannot be Turned On (Alarm State!)");
        }
    }
    else {
        arcSwitch = ARC_ON;
        setPotAmps(setAmps, verbose); // Refresh Digital Pot.
        digitalWrite(SHDN_PIN,PWM_ON);
        if (verbose == VERBOSE_ON) {
            Serial.println("Arc Current Turned On (" + String(setAmps) + " Amps).");
        }
    }
}

// *********************************************************************************************
// Get the iTAG FOB Button Click value.
// Set rst=true if this is the last (or only) call while retrieving the current Click state.
int getFobClick(bool rst)
{
  int fobKey;

  fobKey = buttonClick;

  if ((fobKey != CLICK_SINGLE) && (fobKey != CLICK_DOUBLE)) {
    fobKey = CLICK_NONE;
  }

  if (rst) {
    buttonClick = CLICK_NONE; // Clear click value, we're done with it now.
  }

  return fobKey;
}

// *********************************************************************************************
// Remote control of the Amps settings via Bluetooth iTAG Button FOB.
// The remotely changed Amps settings are NOT saved to EEPROM.
void remoteControl(void)
{
  int click = CLICK_NONE;

  processFobClick();         // Update button detection status on BLE FOB.
  click = getFobClick(true); // Get Button Click value.

  if ((click == CLICK_SINGLE) || (click == CLICK_DOUBLE)) {
    if (spkrVolSwitch != VOL_OFF)
    {
      spkr.stopSounds();  // Override existing announcement.
      spkr.addSoundList({&beep});
    }

    if(overTempAlert){
        spkr.playToEnd(overHeatMsg);
        Serial.println("Announce: Alarm");
    }
    else if(arcSwitch != ARC_ON) {
        arcSwitch = ARC_ON;
        drawHomePage();
        spkr.addSoundList({&silence100ms, &ding, &beep, &silence100ms, &currentOnMsg});
        Serial.println("Announce: Arc Current Turned On.");
    }
    else {
        int changeVal = 0;
        if (click == CLICK_SINGLE) {
          changeVal = REMOTE_AMP_CHG;
        }
        else if (click == CLICK_DOUBLE) {  
          changeVal = -REMOTE_AMP_CHG;
        }

        byte newSetAmps = constrain((int)setAmps + changeVal, MIN_SET_AMPS, MAX_SET_AMPS);
        // explicitly cast setAmps to int in order to handle going into negative amps when subtracting changeVal
        // gracefully. 

        if (setAmps != newSetAmps) {
            setAmps = newSetAmps;
            spkr.addSoundList({(setAmps < newSetAmps) ? &increaseMsg : &decreaseMsg});
            Serial.print(setAmps < newSetAmps ? "Announce: Increase ": "Announce: Decrease ");
        }
        else {
            Serial.print("Announce <no change>:  ");
        }

        setPotAmps(setAmps, VERBOSE_ON);           // Refresh Digital Pot.
        spkr.addDigitSounds(setAmps);

        spkr.playSoundList();
    }
  }
}


// *********************************************************************************************
// Get PulseFreq in Hz as float value.
float PulseFreqHz(void)
{
  float freq;

  pulseFreqX10 = constrain(pulseFreqX10, MIN_PULSE_FRQ_X10, MAX_PULSE_FRQ_X10);
  freq         = (float)(pulseFreqX10);
  freq         = freq / 10.0;

  return freq;
}

// *********************************************************************************************
// Modulate the Welding Arc Current if Pulse Mode is Enabled.
// Modulation freq is provided by PulseFreqHz() function (user's pulse frequency setting).
// Pulse current is a percentage of Normal current (user setting pulseAmpsPc).
// If measured rod arc current is too low the modulation is postponed (normal current is used).
// On new rod strikes the pulse modulation is delayed to allow the arc to fully ignite.
void pulseModulation(void)
{
  int ampVal;
  static long previousMillis = 0;
  static long arcTimer = 0;
  float period               = 0; // Pulse period.

  if (arcSwitch == ARC_ON && pulseSwitch == PULSE_OFF) { // Pulse mode is disabled.
    if (millis() > previousMillis + 500) {               // Refresh Digital POT every 0.5Sec.
        previousMillis = millis();
        arcTimer = millis();
        setPotAmps(setAmps, VERBOSE_OFF);
    }
    pulseState = false;                      // Pulsed current is Off.
  }
  else if (arcSwitch == ARC_ON) {
    period = (1.0 / PulseFreqHz()) / 0.002;  // Convert freq to mS/2.
    // Serial.println("mS: " + String(period)); // Debug

    if (millis() > previousMillis + (long)(period)) {
        previousMillis = millis();
        pulseState = !pulseState;
        drawPulseLightning();                // Update the Pulse Arc icon.
        if(Amps < PULSE_AMPS_THRS) {         // Current too low, don't pulse modulate current.
            arcTimer = millis();
            setPotAmps(setAmps, VERBOSE_OFF);
        }
        else if(millis() > arcTimer+ ARC_STABLIZE_TM) { // Arc should be stabilized, OK to modulate.
           if(pulseState == PULSE_ON) {      // Pulsed welding current cycle.
                ampVal = (int)(setAmps) * pulseAmpsPc / 100; // Modulated with Pulse Current (%) Setting.
                ampVal = constrain(ampVal, MIN_SET_AMPS, MAX_SET_AMPS);
                setPotAmps((byte)(ampVal), VERBOSE_OFF);
            }
            else {                           // Normal welding current cycle.
                setPotAmps(setAmps, VERBOSE_OFF);
            }
        }
    }
  }
//  Serial.println("Amps: " + String(setAmps) + ", bg: " + String(ampVal));  // Debug.
}

// *********************************************************************************************
