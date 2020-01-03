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
#include "XT_DAC_Audio.h"
#include "config.h"

// Global Audio Generation
extern XT_DAC_Audio_Class DacAudio;

// Global Audio Sequencer
extern XT_Sequence_Class Sequence;

// Global Wave Files.
extern XT_Wav_Class increaseMsg;
extern XT_Wav_Class currentOnMsg;
extern XT_Wav_Class decreaseMsg;
extern XT_Wav_Class overHeatMsg;
extern XT_Wav_Class silence100ms;
extern XT_Wav_Class beep;
extern XT_Wav_Class bleep;
extern XT_Wav_Class ding;
extern XT_Wav_Class n000;
extern XT_Wav_Class n001;
extern XT_Wav_Class n002;
extern XT_Wav_Class n003;
extern XT_Wav_Class n004;
extern XT_Wav_Class n005;
extern XT_Wav_Class n006;
extern XT_Wav_Class n007;
extern XT_Wav_Class n008;
extern XT_Wav_Class n009;
extern XT_Wav_Class n010;
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
// Add the wav file item for the 0-9 number passed by calller
void AddNumberToSequence(int theNumber)
{
  switch (theNumber)
  {
    case 0:
      Sequence.AddPlayItem(&n000);
      break;
    case 1:
      Sequence.AddPlayItem(&n001);
      break;
    case 2:
      Sequence.AddPlayItem(&n002);
      break;
    case 3:
      Sequence.AddPlayItem(&n003);
      break;
    case 4:
      Sequence.AddPlayItem(&n004);
      break;
    case 5:
      Sequence.AddPlayItem(&n005);
      break;
    case 6:
      Sequence.AddPlayItem(&n006);
      break;
    case 7:
      Sequence.AddPlayItem(&n007);
      break;
    case 8:
      Sequence.AddPlayItem(&n008);
      break;
    case 9:
      Sequence.AddPlayItem(&n009);
      break;
    default:
      Sequence.AddPlayItem(&silence100ms); // Invalid arg! Say nothing.
      break;
  }
}

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
  int amps1, amps10, amps100;

  processFobClick();         // Update button detection status on BLE FOB.
  click = getFobClick(true); // Get Button Click value.

  if ((click == CLICK_SINGLE) || (click == CLICK_DOUBLE)) {
    if (spkrVolSwitch != VOL_OFF)
    {
      DacAudio.StopAllSounds();      // Override existing announcement.
      // Sequence.ClearAfterPlay = true;  // New feature (this is a test).
      Sequence.RemoveAllPlayItems(); // Clear previously sequenced items.
      Sequence.Volume = 127;         // Maximum sub-volume.
      Sequence.Repeat = 0;           // Don't Repeat.
      Sequence.AddPlayItem(&beep);
    }

    if(overTempAlert){
        Sequence.AddPlayItem(&currentOnMsg);
        DacAudio.Play(&overHeatMsg, true);
        Serial.println("Announce: Alarm");
    }
    else if(arcSwitch != ARC_ON) {
        arcSwitch = ARC_ON;
        drawHomePage();
        if (spkrVolSwitch != VOL_OFF) {
          Sequence.AddPlayItem(&silence100ms);
          Sequence.AddPlayItem(&ding);
          Sequence.AddPlayItem(&beep);
          Sequence.AddPlayItem(&silence100ms);
          Sequence.AddPlayItem(&currentOnMsg);
          DacAudio.Play(&Sequence, true);
        }
        Serial.println("Announce: Arc Current Turned On.");
    }
    else {
        if (click == CLICK_SINGLE) {
            if (setAmps != MAX_SET_AMPS) {
                if (setAmps <= MAX_SET_AMPS - REMOTE_AMP_CHG) {
                    setAmps += REMOTE_AMP_CHG;
                }
                else {
                    setAmps = MAX_SET_AMPS;
                }

                if (spkrVolSwitch != VOL_OFF) {
                    Sequence.AddPlayItem(&increaseMsg);
                }
                Serial.print("Announce: Increase ");
            }
            else {
                Serial.print("Announce <no change>:  ");
            }
        }
        else if (click == CLICK_DOUBLE) {
            if (setAmps != MIN_SET_AMPS) {
                if (setAmps >= MIN_SET_AMPS + REMOTE_AMP_CHG) {
                    setAmps -= REMOTE_AMP_CHG;
                }
                else {
                    setAmps = MIN_SET_AMPS;
                }

                if (spkrVolSwitch != VOL_OFF) {
                    Sequence.AddPlayItem(&decreaseMsg);
                }
                Serial.print("Announce: Decrease ");
            }
            else {
                Serial.print("Announce <no change>: ");
            }
        }

        setAmps = constrain(setAmps, MIN_SET_AMPS, MAX_SET_AMPS);
        amps100 = setAmps / 100;
        amps10  = (setAmps - amps100 * 100) / 10;
        amps1   = setAmps - (amps100 * 100 + amps10 * 10);

        Serial.println(String(amps100) + "-" + String(amps10) + "-" + String(amps1));
        setPotAmps(setAmps, VERBOSE_ON);           // Refresh Digital Pot.

        if (spkrVolSwitch != VOL_OFF) {
            if (amps100 > 0) {                     // Suppress extraneous leading zero.
                AddNumberToSequence(amps100);
            }
            AddNumberToSequence(amps10);
            AddNumberToSequence(amps1);
            DacAudio.Play(&Sequence, true);
        }
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
