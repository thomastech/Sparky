/*
   File: misc.cpp
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.0
   Creation: Sep-11-2019
   Revised: Oct-24-2019
   Release: Oct-30-2019
   Author: T. Black
   (c) copyright T. Black 2019, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
 */

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "digPot.h"
#include "PulseWelder.h"
#include "XT_DAC_Audio.h"

// Global Audio Generation
extern XT_DAC_Audio_Class DacAudio;

// Global Audio Sequencer
extern XT_Sequence_Class Sequence;

// Global Wave Files.
extern XT_Wav_Class increaseMsg;
extern XT_Wav_Class decreaseMsg;
extern XT_Wav_Class silence50ms;
extern XT_Wav_Class beep;
extern XT_Wav_Class bleep;
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
      Sequence.AddPlayItem(&silence50ms); // Invalid arg! Say nothing.
      break;
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

    setPotAmps(setAmps, POT_I2C_ADDR, true); // Refresh Digital Pot.

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
// Modulation is based on Frequency setting. Background current is 50% of Amps Setting.
void pulseModulation(void)
{
  int ampVal;
  static long previousMillis = 0;
  float period               = 0; // Pulse period.

  if (pulseSwitch == PULSE_OFF) { // Pulse mode is disabled. Refresh Digital POT every 0.5Sec, exit.
    if (millis() > previousMillis + 500) {
      setPotAmps(setAmps, POT_I2C_ADDR, false);
      previousMillis = millis();
    }
    pulseState = true;                      // Arc current in On.
  }
  else if (arcSwitch == ARC_ON) {
    period = (1.0 / PulseFreqHz()) / 0.002; // Convert freq to mS/2.
    // Serial.println("mS: " + String(period));  // Debug

    if (millis() > previousMillis + (long)(period)) {
      previousMillis = millis();
      pulseState     = !pulseState;
      drawPulseLightning();                        // Update the Pulse Arc icon.

      if (pulseState || (Amps < MIN_PULSE_AMPS)) { // Disable modulation if Welding current too low.
        ampVal = setAmps;
      }
      else {
        ampVal = (int)(setAmps) * pulseAmpsPc / 100; // Modulated with Pulse Current (%) Setting.
      }

      ampVal = constrain(ampVal, MIN_SET_AMPS, MAX_SET_AMPS);

      //          Serial.println("Amps: " + String(setAmps) + ", bg: " + String(ampVal));  // Debug.
      setPotAmps((byte)(ampVal), POT_I2C_ADDR, false);
    }
  }
}

// *********************************************************************************************
