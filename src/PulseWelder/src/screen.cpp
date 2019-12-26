/*
   File: screen.cpp
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
#include <EEPROM.h>
#include "PulseWelder.h"
#include "screen.h"
#include "XT_DAC_Audio.h"
#include "digPot.h"
#include "config.h"

// Touch Screen
extern XPT2046_Touchscreen ts;
extern Adafruit_ILI9341    tft;

// Global Vars

extern int  Amps;             // Live Welding Current.
extern byte arcSwitch;        // Welder Arc Current On/Off Flag. Pseudo Boolean.
extern byte bleSwitch;        // Bluetooth On/Off Switch.
extern byte pulseSwitch;      // Pulse Mode On/Off Switch.
extern bool overTempAlert;    // High Heat Detected.
extern byte pulseAmpsPc;      // Arc modulation Background Current (%) for Pulse mode.
extern byte pulseFreqX10;     // Pulse mode modulation frequency.
extern bool pulseState;       // Arc Pulse modulation state (on/off).
extern byte setAmps;          // Welding Amps Setting.
extern bool setAmpsTimerFlag; // User has Changed Amps Setting when true.
extern byte spkrVolSwitch;    // Audio Volume, five levels.
extern byte systemError;      // Captures General hardware errors (bad current sensor or bad Digital Pot).
extern unsigned int Volts;    // Live Welding Volts.

// Global Audio Generation
extern XT_DAC_Audio_Class DacAudio;

// Global Wave Files.
extern XT_Wav_Class hello_voice;
extern XT_Wav_Class ding;
extern XT_Wav_Class beep;
extern XT_Wav_Class bloop;
extern XT_Wav_Class blip;
extern XT_Wav_Class bleep;
extern XT_Wav_Class overheat;
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

// Global Music Audio Generation
extern XT_MusicScore_Class highBeep;
extern XT_MusicScore_Class lowBeep;

// Global Audio Sequencer
extern XT_Sequence_Class Sequence;

// local Scope Vars
static char StringBuff[32];          // General purpose Char Buffer
static bool eepromActive = false;    // EEProm Write Requested.
static int  page = PG_HOME;          // Current Menu Page.
static int  x, y;                    // Screen's Touch coordinates.
static long infoAbortMillis     = 0; // Info Page Abort Timer, in mS.
static long SettingsAbortMillis = 0; // Settings Page abort time.
static long previousEepMillis   = 0; // Previous Home Page time.

// *********************************************************************************************
// Change Welder's Pulse amps, Increase or decrement from 10% to 90%.
// Used by processScreen().
// On exit, true returned if end of travel was reached.
bool adjustPulseAmps(bool direction)
{
  bool limitHit = false;

  // Increase or decrease the value. Range is MIN_PULSE_Amps to MAX_PULSE_Amps.
  if ((direction == INCR) && (pulseAmpsPc < MAX_PULSE_AMPS)) {
    pulseAmpsPc += PULSE_AMPS_STEP;
  }
  else if ((direction == DECR) && (pulseAmpsPc > MIN_PULSE_AMPS)) {
    pulseAmpsPc -= PULSE_AMPS_STEP;
  }

  // Check for out of bounds values. Constrain in necessary.
  if (pulseAmpsPc >= MAX_PULSE_AMPS) {
    pulseAmpsPc = MAX_PULSE_AMPS;
    limitHit    = true;
  }
  else if (pulseAmpsPc <= MIN_PULSE_AMPS) {
    pulseAmpsPc = MIN_PULSE_AMPS;
    limitHit    = true;
  }

  // Refresh the Pulse Entry display (if on page PG_SET)
  drawPulseAmpsSettings();

  // Let the caller know that we would like to update the EEPROM if the value has changed.
  eepromActive      = true;     // Request EEPROM save for new settings.
  previousEepMillis = millis(); // Set EEPROM write delay timer.

  return limitHit;
}

// *********************************************************************************************
// Change Welder's Pulse Frequency, Increase or decrement from 0.5 to 5.0 Hz.
// Used by processScreen().
// On exit, true returned if end of travel was reached.
bool adjustPulseFreq(bool direction)
{
  bool limitHit = false;
  int  nextFreq;

  nextFreq = direction == INCR ? 1 : -1;

  // Increase or decrease the value. Range is MIN_PULSE_FRQ (0.4Hz) to MAX_PULSE_FRQ (5.0Hz).
  if ((pulseFreqX10 >= MIN_PULSE_FRQ_X10) && (pulseFreqX10 < 10)) {       // 0.5Hz to 0.9Hz
    pulseFreqX10 += nextFreq;
  }
  else if ((pulseFreqX10 == 10) && (direction == DECR)) {                 // 1.0Hz being decremented
    pulseFreqX10 += nextFreq;
  }
  else if ((pulseFreqX10 >= 10) && (pulseFreqX10 <= MAX_PULSE_FRQ_X10)) { // 1.0Hz to 5.0Hz
    pulseFreqX10 += nextFreq * 10;
  }

  // Check for out of bounds values. Constrain in necessary.
  if (pulseFreqX10 > MAX_PULSE_FRQ_X10) {
    pulseFreqX10 = MAX_PULSE_FRQ_X10;
    limitHit     = true;
  }
  else if (pulseFreqX10 < MIN_PULSE_FRQ_X10) {
    pulseFreqX10 = MIN_PULSE_FRQ_X10;
    limitHit     = true;
  }
  else if ((pulseFreqX10 < MIN_PULSE_FRQ_X10) || (pulseFreqX10 > MAX_PULSE_FRQ_X10)) {
    pulseFreqX10 = DEF_SET_FRQ_X10;
    limitHit     = true;
  }

  // Refresh the Pulse Entry display (if on page PG_SET)
  drawPulseHzSettings();

  // Let the caller know that we would like to update the EEPROM if the value has changed.
  eepromActive      = true;     // Request EEPROM save for new settings.
  previousEepMillis = millis(); // Set EEPROM write delay timer.

  return limitHit;
}

// *********************************************************************************************
// Draw an arc with a defined thickness (modified to aid drawing spirals)
// Source: https://github.com/m5stack/M5Stack/blob/master/examples/Advanced/Display/TFT_FillArcSpiral/TFT_FillArcSpiral.ino
// x,y = coords of centre of arc
// start_angle = 0 - 359
// seg_count = number of 7 degree segments to draw (120 => 360 degree arc)
// rx = x axis radius
// yx = y axis radius
// w  = width (thickness) of arc in pixels
// color = 16 bit colour value
// Note if rx and ry are the same then a circle is drawn

void fillArc(int x, int y, int start_angle, int seg_count, int rx, int ry, int w, unsigned int color)
{
  // Make the segment size 7 degrees to prevent gaps when drawing spirals
  byte seg = 7; // Angle a single segment subtends (made more than 6 deg. for spiral drawing)
  byte inc = 6; // Draw segments every 6 degrees

  // Draw color blocks every inc degrees
  for (int i = start_angle; i < start_angle + seg * seg_count; i += inc)
  {
    // Calculate pair of coordinates for segment start
    float sx    = cos((i - 90) * DEG2RAD);
    float sy    = sin((i - 90) * DEG2RAD);
    uint16_t x0 = sx * (rx - w) + x;
    uint16_t y0 = sy * (ry - w) + y;
    uint16_t x1 = sx * rx + x;
    uint16_t y1 = sy * ry + y;

    // Calculate pair of coordinates for segment end
    float sx2 = cos((i + seg - 90) * DEG2RAD);
    float sy2 = sin((i + seg - 90) * DEG2RAD);
    int   x2  = sx2 * (rx - w) + x;
    int   y2  = sy2 * (ry - w) + y;
    int   x3  = sx2 * rx + x;
    int   y3  = sy2 * ry + y;

    tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
    tft.fillTriangle(x1, y1, x2, y2, x3, y3, color);
  }
}

// *********************************************************************************************
// Process the TouchScreen actions.
// Display screen pages, get touch inputs, perform actions.
// All screens and touch events are performed here.
void processScreen(void)
{
  bool limitHit                  = false;          // Control reached end of travel.
  static bool setAmpsActive      = false;          // Amps Setting flag.
  long bleWaitMillis             = millis();       // BLE Scan Timer.
  static bool wasTouched         = false;          // Touch Display has detected a press.
  static int  repeatCnt          = 0;              // Threshold counter for held keypress.
  static int  repeatms           = REPEAT_SLOW_MS; // Millisecond delay for held keypress.
  static long arrowMillis        = 0;              // Timer for held Up/Down Arrow key repeat feature.
  static long dbncMillis         = 0;              // Touch Screen debouce timer.
  static long homeMillis         = 0;              // Home Page timer for data refresh.
  static long previousHomeMillis = 0;              // Previous Home Page timer.
  static long setAmpsTimer       = 0;              // Amps setting changed by user timer, for Amps refresh.

  if (eepromActive)
  {
    if (millis() - previousEepMillis >= EEP_DELAY_TIME) {
      eepromActive = false;                       // Init for EEProm write checks.

      if (EEPROM.read(AMP_SET_ADDR) != setAmps) { // Save new Current setting.
        eepromActive = true;                      // New Data is available to write.
        EEPROM.write(AMP_SET_ADDR, setAmps);
        Serial.println("Write E2Prom Addr: " + String(AMP_SET_ADDR) + ", Amp Setting: " + String(setAmps));
      }

      if (EEPROM.read(VOL_SET_ADDR) != spkrVolSwitch) { // Save new Current setting.
        eepromActive = true;                            // New Data is available to write.
        EEPROM.write(VOL_SET_ADDR, spkrVolSwitch);
        Serial.println("Write E2Prom Addr: " + String(VOL_SET_ADDR) + ", Volume: " + String(spkrVolSwitch));
      }

      if (EEPROM.read(PULSE_SW_ADDR) != pulseSwitch) { // Save new Pulse Switch setting.
        eepromActive = true;                           // New Data is available to write.
        EEPROM.write(PULSE_SW_ADDR, pulseSwitch);
        Serial.println("Write E2Prom Addr: " + String(PULSE_SW_ADDR) + ", Pulse Mode: " + String((pulseSwitch == PULSE_ON ? "On" : "Off")));
      }

      if (EEPROM.read(ARC_SW_ADDR) != arcSwitch) { // Save new Arc Switch setting.
        eepromActive = true;                       // New Data is available to write.
        EEPROM.write(ARC_SW_ADDR, arcSwitch);
        Serial.println("Write E2Prom Addr: " + String(ARC_SW_ADDR) + ", Arc Power: " + String((arcSwitch == ARC_ON ? "On" : "Off")));
      }

      if (EEPROM.read(PULSE_FRQ_ADDR) != pulseFreqX10) { // Save new Pulse Frequency setting.
        eepromActive = true;                             // New Data is available to write.
        EEPROM.write(PULSE_FRQ_ADDR, pulseFreqX10);
        Serial.println("Write E2Prom Addr: " + String(PULSE_FRQ_ADDR) + ", Pulse Freq: " + String(PulseFreqHz(), 1) + String(" Hz"));
      }

      if (EEPROM.read(BLE_SW_ADDR) != bleSwitch) { // Save new Bluetooth Switch setting.
        eepromActive = true;                       // New Data is available to write.
        EEPROM.write(BLE_SW_ADDR, bleSwitch);
        Serial.println("Write E2Prom Addr: " + String(BLE_SW_ADDR) + ", Bluetooth: " + String((bleSwitch == PULSE_ON ? "On" : "Off")));
      }

      if (EEPROM.read(PULSE_AMPS_ADDR) != pulseAmpsPc) { // Save new Pulse Current (%) setting.
        eepromActive = true;                             // New Data is available to write.
        EEPROM.write(PULSE_AMPS_ADDR, pulseAmpsPc);
        Serial.println("Write E2Prom Addr: " + String(PULSE_AMPS_ADDR) + ", Pulse Modulation Current: " + String(pulseAmpsPc) + "%");
      }

      if (eepromActive) { // New data available to write. Commit it to the flash.
        eepromActive = false;
        EEPROM.commit();
      }
    }
  }

  if (page == PG_HOME)
  { // Home page.
    homeMillis = millis();

    if (homeMillis - previousHomeMillis >= DATA_REFRESH_TIME) {
      previousHomeMillis = homeMillis;

      // drawBattery(BATTERY_X, BATTERY_Y);
      displayOverTempAlert(); // Display temperature warning if too hot.
      displayAmps(false);
      displayVolts(false);
    }

    if ((setAmpsTimerFlag == true) && (homeMillis >= setAmpsTimer + SET_AMPS_TIME)) {
      setAmpsTimerFlag = false; // Amps setting (by user) timer has expired.
    }

    if (!ts.touched() && wasTouched && (homeMillis > dbncMillis + TOUCH_DBNC)) {
      wasTouched    = false;
      setAmpsActive = false;
    }
    else if (!ts.touched()) {
      repeatCnt = 0;                        // Event counter for when key press is held.
      repeatms  = REPEAT_SLOW_MS;           // Init to slow time delay on held key press, in mS.
    }
    else if (ts.touched() && !wasTouched) { // Debounce the touchscreen to prevents multiple inputs from single touch.
      dbncMillis = millis();

      wasTouched = true;
      getTouchPoints();

      if (((x > ARCBOX_X) && (x < ARCBOX_X + ARCBOX_W)) && ((y > ARCBOX_Y) && (y < ARCBOX_Y + ARCBOX_H)))
      {                                                     // toggle Arc Current
        arcSwitch = arcSwitch == ARC_ON ? ARC_OFF : ARC_ON; // Pseudo Boolean Toggle.
        drawHomePage();

        if (arcSwitch == ARC_ON) {
          Serial.println("Arc Current Turned On (" + String(setAmps) + " Amps)");
          setPotAmps(setAmps, true); // Refresh Digital Pot.

          if (spkrVolSwitch != VOL_OFF) {
            DacAudio.Play(&highBeep, true);
          }
        }
        else {
          Serial.println("Arc Current Turned Off (limited to " + String(SET_AMPS_DISABLE) + " Amps).");
          setPotAmps(SET_AMPS_DISABLE, true); // Set Digital Pot to lowest welding current.

          if (spkrVolSwitch != VOL_OFF) {
            DacAudio.Play(&lowBeep, true);
          }
        }
        previousEepMillis = millis();
        eepromActive      = true; // Request EEProm Write after timer expiry.
      }
      else if (((x > SNDBOX_X) && (x < SNDBOX_X + SNDBOX_W)) && ((y > SNDBOX_Y) && (y < SNDBOX_Y + SNDBOX_H)))
      {                           // Adjust Audio Volume
        if ((spkrVolSwitch >= VOL_OFF) && (spkrVolSwitch < VOL_LOW)) {
          spkrVolSwitch = VOL_LOW;
        }
        else if ((spkrVolSwitch >= VOL_LOW) && (spkrVolSwitch < VOL_MED)) {
          spkrVolSwitch = VOL_MED;
        }
        else if ((spkrVolSwitch >= VOL_MED) && (spkrVolSwitch < VOL_HI)) {
          spkrVolSwitch = VOL_HI;
        }
        else if ((spkrVolSwitch >= VOL_HI) && (spkrVolSwitch < XHI_VOL)) {
          spkrVolSwitch = XHI_VOL;
        }
        else {
          spkrVolSwitch = VOL_OFF;
        }

        if (spkrVolSwitch == VOL_OFF) {
          DacAudio.DacVolume = VOL_LOW; // Termporaily Use soft volume for Audio feedback.
          DacAudio.Play(&lowBeep, true);
          DacAudio.FillBuffer();

          while (lowBeep.TimeLeft) {
            DacAudio.FillBuffer();
          }
          DacAudio.DacVolume = spkrVolSwitch; // Now turn off volume.
          // DacAudio.StopAllSounds();
          Serial.println("Sound Disabled.");
        }
        else {
          DacAudio.DacVolume = spkrVolSwitch;
          DacAudio.Play(&highBeep, true);
          DacAudio.FillBuffer();
          Serial.println("Sound Set to Volume " + String(spkrVolSwitch));
        }
        previousEepMillis = millis();
        eepromActive      = true; // Request EEProm Write after timer expiry.
        updateVolumeIcon();
      }
      else if (((x > INFOBOX_X) && (x < INFOBOX_X + INFOBOX_W)) && ((y > INFOBOX_Y) && (y < INFOBOX_Y + INFOBOX_H)))
      { // Info button pressed
        drawInfoPage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&highBeep, true);
        }
      }
      else if (((x > SETBOX_X) && (x < SETBOX_X + SETBOX_W)) && ((y > SETBOX_Y) && (y < SETBOX_Y + SETBOX_H)))
      { // Settings button pressed
        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.StopAllSounds();
          Sequence.RemoveAllPlayItems();
          DacAudio.Play(&highBeep, false);

          while (highBeep.TimeLeft) {
            DacAudio.FillBuffer();
          }
        }
        drawSettingsPage();
      }

      else if (((x > AUPBOX_X) && (x < AUPBOX_X + AUPBOX_W)) && ((y > AUPBOX_Y) && (y < AUPBOX_Y + AUPBOX_H)))
      {                                 // Increase Amps Setting
        if (arcSwitch == ARC_OFF) {     // Welding current disabled.
          if (spkrVolSwitch != VOL_OFF) {
            DacAudio.Play(&beep, true); // Warn user that button is disabled.
          }
          return;
        }
        setAmpsTimerFlag = true;
        setAmpsTimer     = millis();
        setAmpsActive    = true;
        setAmps++;
        setAmps = constrain(setAmps, MIN_SET_AMPS, MAX_SET_AMPS);
        setPotAmps(setAmps, true); // Refresh Digital Pot.
        displayAmps(true);                       // Refresh displayed value.
        arrowMillis       = millis();
        previousEepMillis = millis();
        eepromActive      = true;                // Request EEProm Write after timer expiry.

        if (spkrVolSwitch != VOL_OFF) {
          if (setAmps < MAX_SET_AMPS) {
            DacAudio.Play(&blip, true);
          }
          else {
            DacAudio.Play(&bloop, true);
          }
        }
      }
      else if (((x > ADNBOX_X) && (x < ADNBOX_X + ADNBOX_W)) && ((y > ADNBOX_Y) && (y < ADNBOX_Y + ADNBOX_H)))
      {                                 // Decrease Amps Setting
        if (arcSwitch == ARC_OFF) {     // Welding current disabled.
          if (spkrVolSwitch != VOL_OFF) {
            DacAudio.Play(&beep, true); // Warn user that button is disabled.
          }
          return;
        }
        setAmpsTimerFlag = true;
        setAmpsTimer     = millis();
        setAmpsActive    = true;
        setAmps--;
        setAmps = constrain(setAmps, MIN_SET_AMPS, MAX_SET_AMPS);
        setPotAmps(setAmps, true); // Refresh Digital Pot.
        displayAmps(true);                       // Refresh displayed value.
        arrowMillis       = millis();
        previousEepMillis = millis();
        eepromActive      = true;                // Request EEProm Write after timer expiry.

        if (spkrVolSwitch != VOL_OFF) {
          if (setAmps > MIN_SET_AMPS) {
            DacAudio.Play(&blip, true);
          }
          else {
            DacAudio.Play(&bloop, true);
          }
        }
      }

      else if (((x > PULSEBOX_X) && (x < PULSEBOX_X + (PULSEBOX_W * 2))) && ((y > PULSEBOX_Y) && (y < PULSEBOX_Y + PULSEBOX_H)))
      {
        pulseSwitch = pulseSwitch == PULSE_ON ? false : true; // Toggle psuedo boolean

        if (pulseSwitch == PULSE_ON) {
          if (spkrVolSwitch != VOL_OFF) {
            DacAudio.Play(&highBeep, true);
          }
          Serial.println("Pulse Mode On: " + String(PulseFreqHz(), 1) + " Hz");
        }
        else {
          if (spkrVolSwitch != VOL_OFF) {
            DacAudio.Play(&lowBeep, true);
          }
          Serial.println("Pulse Mode Off");
        }

        drawPulseIcon();
        displayAmps(true); // Refresh Amps display to update background color.

        if (arcSwitch == ARC_ON) {
          setPotAmps(setAmps, false);
        }
        else {
          setPotAmps(SET_AMPS_DISABLE, false);
        }
        previousEepMillis = millis();
        eepromActive      = true; // Request EEProm Write after timer expiry.
      }
    }

    else if (ts.touched() && wasTouched)
    {
      if (setAmpsActive == true)
      {
        if (((x > AUPBOX_X) && (x < AUPBOX_X + AUPBOX_W)) && ((y > AUPBOX_Y) && (y < AUPBOX_Y + AUPBOX_H)) &&
            (millis() > arrowMillis + repeatms))
        { // Increase Amps Setting while Up Arrow button is held down. Two speeds.
          setAmpsTimerFlag = true;
          setAmpsTimer     = millis();

          if (repeatCnt > REPEAT_CNT_THRS) {
            repeatms = REPEAT_FAST_MS; // millisecond delay, fast.
          }
          else {
            repeatCnt++;
            repeatms = REPEAT_SLOW_MS; // millisecond delay, slow.
          }
          arrowMillis       = millis();
          previousEepMillis = millis();
          eepromActive      = true;                // Request EEProm Write after timer expiry.
          setAmps++;
          setAmps = constrain(setAmps, MIN_SET_AMPS, MAX_SET_AMPS);
          setPotAmps(setAmps, true); // Refresh Digital Pot.
          displayAmps(true);                       // Refresh amps value.

          if (spkrVolSwitch != VOL_OFF) {
            if (repeatCnt == 1) {                  // First Repeated keypress.
              DacAudio.Play(&ding, true);
            }
            else {                                 // Ongoing repeats.
              if (setAmps < MAX_SET_AMPS) {
                DacAudio.Play(&blip, true);
              }
              else {
                DacAudio.Play(&bloop, true);
              }
            }
          }
        }
        else if (((x > ADNBOX_X) && (x < ADNBOX_X + ADNBOX_W)) && ((y > ADNBOX_Y) && (y < ADNBOX_Y + ADNBOX_H)) &&
                 (millis() > arrowMillis + repeatms))
        { // Decrease Amp Setting while Up Arrow button is held down. Two speeds.
          setAmpsTimerFlag = true;
          setAmpsTimer     = millis();

          if (repeatCnt > REPEAT_CNT_THRS) {
            repeatms = REPEAT_FAST_MS; // millisecond delay, fast.
          }
          else {
            repeatCnt++;
            repeatms = REPEAT_SLOW_MS; // millisecond delay, slow.
          }
          arrowMillis       = millis();
          previousEepMillis = millis();
          eepromActive      = true;                // Request EEProm Write after timer expiry.
          setAmps--;
          setAmps = constrain(setAmps, MIN_SET_AMPS, MAX_SET_AMPS);
          setPotAmps(setAmps, true); // Refresh Digital Pot.
          displayAmps(true);                       // Refresh value.

          if (spkrVolSwitch != VOL_OFF) {
            if (repeatCnt == 1) {                  // First Repeated keypress.
              DacAudio.Play(&beep, true);
            }
            else {                                 // Ongoing repeats.
              if (setAmps > MIN_SET_AMPS) {
                DacAudio.Play(&blip, true);
              }
              else {
                DacAudio.Play(&bloop, true);
              }
            }
          }
        }
      }
    }
  }

  else if (page == PG_INFO) // Information page. All display elements are drawn when drawInfoPage() is called.
  {
    if (!ts.touched())
    {
      wasTouched = false;

      if (millis() > infoAbortMillis + MENU_RD_TIME_MS) {
        Serial.println("Main Info page timeout, exit.");
        drawHomePage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
    }
    else if (ts.touched() && !wasTouched) {
      infoAbortMillis = millis();
      wasTouched      = true;
      getTouchPoints();

      if (((x > 0) && (x <= SCREEN_W)) && ((y > 0) && (y <= 50))) // Return button. Return to homepage.
      {
        Serial.println("Exit Info, retured to home page");
        drawHomePage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
      else if (((x > 45) && (x < 260)) && ((y > 70) && (y < 100)))
      {
        infoAbortMillis = millis();
        drawInfoPage6011();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&highBeep, true);
        }
      }
      else if (((x > 45) && (x < 260)) && ((y > 125) && (y < 155)))
      {
        infoAbortMillis = millis();
        drawInfoPage6013();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&highBeep, true);
        }
      }
      else if (((x > 45) && (x < 260)) && ((y > 175) && (y < 205)))
      {
        infoAbortMillis = millis();
        drawInfoPage7018();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&highBeep, true);
        }
      }
    }
  }

  else if (page == PG_INFO_6011) // Information page on 6011 Rod.
  {
    if (!ts.touched())
    {
      wasTouched = false;

      if (millis() > infoAbortMillis + PG_RD_TIME_MS)
      {
        Serial.println("6011 Info page timeout, exit.");
        infoAbortMillis = millis(); // Reset the info page's keypress abort timer.
        drawInfoPage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
    }
    else if (ts.touched() && !wasTouched)
    {
      infoAbortMillis = millis();
      wasTouched      = true;
      getTouchPoints();

      if (((x >= 0) && (x <= SCREEN_W)) && ((y >= 0) && (y <= SCREEN_H))) // Press anywhere on screen to main info page.
      {
        Serial.println("User Exit 6011 Info, retured to main info page");
        infoAbortMillis = millis();
        drawInfoPage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
    }
  }

  else if (page == PG_INFO_6013) // Information page on 6013 Rod.
  {
    if (!ts.touched())
    {
      wasTouched = false;

      if (millis() > infoAbortMillis + PG_RD_TIME_MS)
      {
        Serial.println("6013 Info page timeout, exit.");
        infoAbortMillis = millis(); // Reset the info page's keypress abort timer.
        drawInfoPage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
    }
    else if (ts.touched() && !wasTouched)
    {
      infoAbortMillis = millis();
      wasTouched      = true;
      getTouchPoints();

      if (((x >= 0) && (x <= SCREEN_W)) && ((y >= 0) && (y <= SCREEN_H))) // Press anywhere on screen to main info page.
      {
        Serial.println("User Exit 6013 Info, retured to main info page");
        infoAbortMillis = millis();
        drawInfoPage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
    }
  }

  else if (page == PG_INFO_7018) // Information page on 7018 Rod.
  {
    if (!ts.touched())
    {
      wasTouched = false;

      if (millis() > infoAbortMillis + PG_RD_TIME_MS)
      {
        Serial.println("7018 Info page timeout, exit.");
        infoAbortMillis = millis(); // Reset the info page's keypress abort timer.
        drawInfoPage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
    }
    else if (ts.touched() && !wasTouched)
    {
      infoAbortMillis = millis();
      wasTouched      = true;
      getTouchPoints();

      if (((x >= 0) && (x <= SCREEN_W)) && ((y >= 0) && (y <= SCREEN_H))) // Press anywhere on screen to main info page.
      {
        Serial.println("User Exit 7018 Info, retured to main info page");
        infoAbortMillis = millis();
        drawInfoPage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
    }
  }

  else if (page == PG_SET) // Settings Page
  {
    if (!ts.touched())
    {
      wasTouched = false;

      if (millis() > SettingsAbortMillis + PG_RD_TIME_MS)
      {
        Serial.println("Settings page timeout, exit.");
        SettingsAbortMillis = millis(); // Reset the settings page's keypress abort timer.
        drawHomePage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
    }
    else if (ts.touched() && !wasTouched)
    {
      SettingsAbortMillis = millis();
      wasTouched          = true;
      getTouchPoints();

      if (((x >= 0) && (x <= SCREEN_W)) && ((y >= 0) && (y <= 40))) // Return button. Return to home page.
      {
        Serial.println("User Settings page, retured to Home page");
        infoAbortMillis = millis();
        drawHomePage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
      else if (((x >= SCREEN_W - (PSBOX_W + PSBOX_X)) && (x <= SCREEN_W - PSBOX_X)) && ((y >= PSBOX_Y) && (y <= PSBOX_Y + PSBOX_H)))
      {
        limitHit = adjustPulseFreq(INCR);
        Serial.println("Increased Pulse Freq: " + String(PulseFreqHz(), 1) + " Hz"); // Increase Pulse Freq.

        if (spkrVolSwitch != VOL_OFF)
        {
          if (limitHit) { // End of travel reached.
            DacAudio.Play(&bloop, true);
          }
          else {
            DacAudio.Play(&blip, true);
          }
        }
      }
      else if (((x >= PSBOX_X) && (x <= PSBOX_X + PSBOX_W)) && ((y >= PSBOX_Y) && (y <= PSBOX_Y + PSBOX_H)))
      {
        limitHit = adjustPulseFreq(DECR);
        Serial.println("Decreased Pulse Freq: " + String(PulseFreqHz(), 1) + " Hz"); // Decrease Pulse Freq.

        if (spkrVolSwitch != VOL_OFF)
        {
          if (limitHit) { // End of travel reached.
            DacAudio.Play(&bloop, true);
          }
          else {
            DacAudio.Play(&bleep, true);
          }
        }
      }
      else if (((x >= PCBOX_X) && (x <= PCBOX_X + PCBOX_W)) && ((y >= PCBOX_Y) && (y <= PCBOX_Y + PCBOX_H)))
      {
        limitHit = adjustPulseAmps(DECR);
        Serial.println("Decreased Pulse Current: " + String(pulseAmpsPc) + "%"); // Decrease Pulse Amps (%).

        if (spkrVolSwitch != VOL_OFF)
        {
          if (limitHit) { // End of travel reached.
            DacAudio.Play(&bloop, true);
          }
          else {
            DacAudio.Play(&bleep, true);
          }
        }
      }
      else if (((x >= SCREEN_W - (PCBOX_W + PCBOX_X)) && (x <= SCREEN_W - PCBOX_X)) && ((y >= PCBOX_Y) && (y <= PCBOX_Y + PCBOX_H)))
      {
        limitHit = adjustPulseAmps(INCR);
        Serial.println("Increased Pulse Current: " + String(pulseAmpsPc) + "%"); // Increase Pulse Amps (%).

        if (spkrVolSwitch != VOL_OFF)
        {
          if (limitHit) { // End of travel reached.
            DacAudio.Play(&bloop, true);
          }
          else {
            DacAudio.Play(&blip, true);
          }
        }
      }
      else if (((x >= FBBOX_X + FBBOX_W + 10) && (x <= FBBOX_X + FBBOX_W + BOBOX_W + 5)) && (y >= FBBOX_Y - 5) &&
               (y <= FBBOX_Y + BOBOX_H - 5))
      {
        bleSwitch    = (bleSwitch == BLE_ON ? BLE_OFF : BLE_ON); // Pseudo Boolean toggle.
        eepromActive = true;                                     // Request EEPROM save for new settings.

        if ((bleSwitch == BLE_OFF) && (isBleServerConnected() == true)) {
          stopBle();
        }
        previousEepMillis = millis(); // Set EEPROM write delay timer.
        showBleStatus(BLE_MSG_AUTO);  // Update the Bluetooth On/Off button.
        Serial.println("Bluetooth Mode: " + String(bleSwitch == BLE_ON ? "ON" : "OFF"));

        if (bleSwitch == BLE_ON) {
          DacAudio.Play(&blip, true);
        }
        else {
          DacAudio.Play(&bleep, true);
        }
      }
      else if (((x >= FBBOX_X + 5) && (x <= FBBOX_W + FBBOX_X - 10)) && ((y >= FBBOX_Y - 4) && (y <= FBBOX_Y + FBBOX_H - 10)))
      {
        if (bleSwitch == BLE_OFF)
        {
          DacAudio.Play(&bloop, true);
          Serial.println("Bluetooth Disabled!");
        }
        else if (isBleServerConnected())
        {
          DacAudio.Play(&bleep, true);
          Serial.println("Bluetooth Already Connected!");
          showBleStatus(BLE_MSG_FOUND);
        }
        else
        {
          DacAudio.Play(&blip, true);
          showBleStatus(BLE_MSG_SCAN); // Post "Bluetooth Scanning" message.

          while (blip.TimeLeft) {
            DacAudio.FillBuffer();
          }

          if (isBleDoScan()) {
            Serial.println("User Requested BlueTooth Reconnect.");
            reconnectBlueTooth(BLE_RESCAN_TIME); // Try to Reconnect to lost connection.
          }
          else {
            Serial.println("User Requested Fresh BlueTooth Scan.");
            scanBlueTooth();                                                  // Fresh Scan, never connected before.
          }

          reconnectTimer(true);                                               // Reset Auto-Reconnect Timer.
          bleWaitMillis = millis();

          while (millis() <= bleWaitMillis + 2000 && !isBleServerConnected()) // Wait for advert callback to confirm connect.
          {
            DacAudio.FillBuffer();                                            // Fill the sound buffer with data.
            showHeartbeat();                                                  // Draw Heartbeat icon.
            checkBleConnection();                                             // Check the Bluetooth iTAG FOB Button server connection.
          }

          if (isBleServerConnected()) {
            showBleStatus(BLE_MSG_FOUND); // Post "Found" message.
          }
          else {
            showBleStatus(BLE_MSG_FAIL);  // Post "Not Found"
          }
        }
      }
    }
  }

  else if (page == PG_ERROR) // System Error page.
  {
    if (!ts.touched())
    {
      wasTouched = false;
    }
    else if (ts.touched() && !wasTouched)
    {
      wasTouched = true;
      getTouchPoints();

      if (((x >= 0) && (x <= SCREEN_W)) && ((y >= 0) && (y <= SCREEN_H))) // Press anywhere on screen to main info page.
      {
        Serial.println("User Proceeded to Home Page despite Error Warning.");
        drawHomePage();

        if (spkrVolSwitch != VOL_OFF) {
          DacAudio.Play(&lowBeep, true);
        }
      }
    }
  }
}

// *********************************************************************************************
// Get global x,y touch points and map to screen pixels.
void getTouchPoints(void)
{
  TS_Point p = ts.getPoint();

  //    Serial.println("raw X:" + String(p.x) + " raw Y:" + String(p.y));
  x = map(p.x, TS_MINX, TS_MAXX, SCREEN_W, 0);
  y = map(p.y, TS_MINY, TS_MAXY, SCREEN_H, 0);
  Serial.println("[Touch Coordinates] X: " + String(x) + "  Y:" + String(y));
}

// *********************************************************************************************
// Display Show Amps Setting when idle, otherwise show live Amps Value (when burning rod).
// To prevent screen flash, use forceRefresh=false to only update if the data has changed.
// On entry use forceRefresh=true to refresh the Amps value even if it has not changed.
void displayAmps(bool forceRefresh)
{
  static int   oldAmps    = -1;
  static int   oldsetAmps = -1;
  static int   tglFlag    = false;
  unsigned int color;
  unsigned int dispAmps = 0;

  if (overTempAlert) {
    return;
  }

  if ((Amps >= MIN_DET_AMPS) && (arcSwitch == ARC_ON) && (setAmpsTimerFlag == false))
  {
    oldsetAmps = -1; // Force setting value refresh when rod burning ends.

    if ((oldAmps == Amps) && (forceRefresh == false)) {
      return;
    }
    else {
      oldAmps = Amps;
    }
  }
  else
  {
    oldAmps = -1; // Force burn current refresh on next rod burn.

    if (arcSwitch == ARC_OFF)
    {
      dispAmps = SET_AMPS_DISABLE; // Arc Current disable.
    }
    else if ((oldsetAmps == setAmps) && (forceRefresh == false))
    {
      return;
    }
    else
    {
      oldsetAmps = setAmps;
      dispAmps   = setAmps;
    }
  }

  if (pulseSwitch == PULSE_ON) {
    color = ILI9341_LIGHTGREY; // Highlite Pulsemode.
  }
  else {
    color = ILI9341_WHITE;
  }

  tft.fillRect(AMPBOX_X + 5, AMPBOX_Y + 10, AMPBOX_W, AMPBOX_Y + AMPVAL_H + 5, color); // Erase old value
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setCursor(AMPBOX_X + 5, AMPBOX_Y + AMPVAL_H + 5);
  tft.setTextSize(2);

  if ((Amps >= MIN_DET_AMPS) && (arcSwitch == ARC_ON) && (setAmpsTimerFlag == false))
  { // Burning a rod. Show live current draw.
    tft.setTextColor(ILI9341_RED);
    sprintf(StringBuff, "%3d", Amps);
  }
  else
  { // Idle current (not burning a welding rod) or user has recently changed Amp Setting. Show user's Amps Setting.
    if (arcSwitch == ARC_OFF) {
      tglFlag = !tglFlag;
      color   = tglFlag ? ILI9341_YELLOW : ILI9341_BLACK;
    }
    else if (pulseSwitch == PULSE_ON) {
      color = MED_BLUE;
    }
    else {
      color = ILI9341_GREEN;
    }
    tft.setTextColor(color);
    sprintf(StringBuff, "%3d", dispAmps);
  }
  tft.println(StringBuff);

  // Display the moving amp bar.
  drawAmpBar(AMPBAR_X, AMPBAR_Y, forceRefresh);
}

// *********************************************************************************************
void displaySplash(void)
{
  // Draw Image.
  tft.fillScreen(ILI9341_WHITE);
  tft.drawBitmap(20, 61, sparky, 280, 166, ILI9341_BLACK);

  // Title
  tft.setFont(&FreeMonoBold18pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(100, 25);
  tft.print("SPARKY");

  // Byline
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(20, 50);
  tft.print("Stick Welding Controller");

  // Version
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(70, 215);
  tft.print(VERSION_STR);
}

// *********************************************************************************************
// Display the Over Current Alert Message.
void displayOverTempAlert(void)
{
  static bool heatFlat = false; // Over current detection flag.

  if (heatFlat && !overTempAlert)
  {
    heatFlat = false;
    drawAmpsBox();
    displayAmps(true);
  }
  else if (!heatFlat && overTempAlert)
  {
    heatFlat = true;
    Serial.println("Warning: Over-Temperature has been detected!");

    if (overheat.TimeLeft <= 1) { // Don't interrupt a over-heat alarm message playback.
      DacAudio.Play(&overheat, false);
    }
    tft.fillRoundRect(AMPBOX_X, AMPBOX_Y, SCREEN_W - AMPBOX_X - 2, AMPBOX_H, AMPBOX_R, ILI9341_RED);
    tft.drawRoundRect(AMPBOX_X,     AMPBOX_Y,     SCREEN_W - AMPBOX_X, AMPBOX_H,     AMPBOX_R, ILI9341_BLACK);
    tft.drawRoundRect(AMPBOX_X + 1, AMPBOX_Y + 1, SCREEN_W - AMPBOX_X, AMPBOX_H - 2, AMPBOX_R, ILI9341_BLACK);
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(AMPBOX_X + 50, AMPBOX_Y + 35);
    tft.println("OVER");
    tft.setCursor(AMPBOX_X + 30, AMPBOX_Y + 65);
    tft.println("HEATING");
    tft.setCursor(AMPBOX_X + 35, AMPBOX_Y + 95);
    tft.println("ALARM!");
  }
}

// *********************************************************************************************
// Display Volts Value.
// To prevent screen flash, use forceRefresh=false to only update if the data has changed.
// On entry use forceRefresh=true to refresh the Volts value even if it has not changed.
void displayVolts(bool forceRefresh = false)
{
  static int   oldVolts = -1;
  unsigned int color;

  if ((oldVolts == Volts) && (forceRefresh == false)) {
    return;
  }
  oldVolts = Volts;

  color = arcSwitch == ARC_ON ? ARC_BG_COLOR : ILI9341_BLUE;
  tft.fillRect(VOLTBOX_X + 5, VOLTBOX_Y + 10, VOLTBOX_W, VOLTBOX_H, color); // Erase old value
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextSize(2);
  tft.setCursor(VOLTBOX_X + 5, VOLTBOX_Y + VOLTVAL_H + 5);

  color = Volts <= MIN_VOLTS ? ILI9341_YELLOW : ILI9341_GREEN;
  tft.setTextColor(color);
  sprintf(StringBuff, "%2d", Volts);
  tft.print(StringBuff);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.println('V');

  color = Volts <= MIN_VOLTS ? ILI9341_RED : ILI9341_GREEN;
  tft.drawBitmap(VOLTBOX_X + VOLTBOX_W + 15, VOLTBOX_Y + 20, lightningBitmap, 20, 30, color);
}

// *********************************************************************************************
void drawAmpsBox(void)
{
  tft.fillRoundRect(AMPBOX_X, AMPBOX_Y, SCREEN_W - AMPBOX_X - 2, AMPBOX_H, AMPBOX_R, ILI9341_WHITE);
  tft.drawRoundRect(AMPBOX_X,     AMPBOX_Y,     SCREEN_W - AMPBOX_X, AMPBOX_H,     AMPBOX_R, ILI9341_CYAN);
  tft.drawRoundRect(AMPBOX_X + 1, AMPBOX_Y + 1, SCREEN_W - AMPBOX_X, AMPBOX_H - 2, AMPBOX_R, ILI9341_CYAN);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(AMPBOX_X + 145, AMPBOX_Y + 110);
  tft.println("A");
}

// *********************************************************************************************
// Draw the moving amp bar.
// On entry, forceRefresh==true will redraw the bar.
void drawAmpBar(int x, int y, bool forceRefresh)
{
  unsigned int ampsMapped;
  static int   old_setAmps = -1;

  if (overTempAlert) {
    return;
  }

  if ((setAmps == old_setAmps) && (forceRefresh == false)) {
    return;
  }
  old_setAmps = setAmps;
  ampsMapped  = map(setAmps, 0, MAX_SET_AMPS, 0, AMPBAR_W);

  tft.drawRoundRect(AMPBAR_X - 2, (AMPBAR_Y - 2), (AMPBAR_W + 4), (AMPBAR_H + 4), 3, ILI9341_WHITE);
  tft.fillRect(AMPBAR_X, AMPBAR_Y, AMPBAR_W,   AMPBAR_H, ILI9341_LIGHTGREY);
  tft.fillRect(AMPBAR_X, AMPBAR_Y, ampsMapped, AMPBAR_H, ILI9341_GREEN);
}

// *********************************************************************************************
// Information page.
void drawInfoPage(void)
{
  Serial.println("Main Info Page");
  page            = PG_INFO;
  infoAbortMillis = millis();

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, ILI9341_BLACK);
  tft.drawRect(1, 1, tft.width() - 1, tft.height() - 1, ILI9341_WHITE);
  tft.drawRect(2, 2, tft.width() - 3, tft.height() - 3, ILI9341_WHITE);

  tft.fillRect(2, 2, tft.width() - 1, 40, ILI9341_WHITE);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(55, 32);
  tft.println("ROD INFORMATION");

  tft.drawBitmap(5, 5, returnBitMap, 35, 35, ILI9341_RED);

  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_YELLOW);
  tft.drawRoundRect(43, 60, 234, 50, 8, WHITE);
  tft.drawRoundRect(44, 61, 232, 48, 8, WHITE);
  tft.fillRoundRect(45, 62, 230, 46, 8, 0x2A86);
  tft.setCursor(120, 93);
  tft.println("E-6011");

  tft.drawRoundRect(43, 117, 234, 50, 8, WHITE);
  tft.fillRoundRect(44, 118, 232, 48, 8, 0x2A86);
  tft.setCursor(120, 150);
  tft.println("E-6013");

  tft.drawRoundRect(43, 174, 234, 50, 8, WHITE);
  tft.fillRoundRect(44, 175, 232, 48, 8, 0x2A86);
  tft.setCursor(120, 207);
  tft.println("E-7018");
}

// *********************************************************************************************
// Information page for 6011 rod.
void drawInfoPage6011(void)
{
  Serial.println("Showing Information on 6011 Rod");

  page            = PG_INFO_6011;
  infoAbortMillis = millis();

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, ILI9341_BLACK);
  tft.drawRect(1, 1, tft.width() - 1, tft.height() - 1, ILI9341_WHITE);
  tft.drawRect(2, 2, tft.width() - 3, tft.height() - 2, ILI9341_WHITE);

  tft.fillRect(2, 2, tft.width() - 1, 40, ILI9341_WHITE);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(50, 32);
  tft.println("E-6011 INFORMATION");
  tft.drawBitmap(5, 5, returnBitMap, 35, 35, ILI9341_RED);

  tft.setFont(&FreeSans12pt7b);

  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(15, 70);
  tft.println("3/32\" 2.4mm 40-90A");
  tft.setCursor(15, 100);
  tft.println("1/8\"   3.2mm 75-125A");
  tft.setCursor(15, 130);
  tft.println("5/32\" 4.0mm 110-165A");
  tft.setCursor(15, 160);
  tft.print("DCEP");

  tft.setTextColor(ILI9341_YELLOW);
  tft.print("  Deep Penetration");
  tft.setCursor(15, 190);
  tft.println("High Cellulose Potassium");
  tft.setCursor(15, 220);
  tft.println("All Position, 60K PSI");
}

// *********************************************************************************************
// Information page for 6013 rod.
void drawInfoPage6013(void)
{
  Serial.println("Showing Information on 6013 Rod");

  page            = PG_INFO_6013;
  infoAbortMillis = millis();

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, ILI9341_BLACK);
  tft.drawRect(1, 1, tft.width() - 1, tft.height() - 1, ILI9341_WHITE);
  tft.drawRect(2, 2, tft.width() - 3, tft.height() - 2, ILI9341_WHITE);

  tft.fillRect(2, 2, tft.width() - 1, 40, ILI9341_WHITE);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(50, 32);
  tft.println("E-6013 INFORMATION");
  tft.drawBitmap(5, 5, returnBitMap, 35, 35, ILI9341_RED);

  tft.setFont(&FreeSans12pt7b);

  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(15, 70);
  tft.println("1/16\" 1.6mm 20-45A");
  tft.setCursor(15, 100);
  tft.println("3/32\" 2.4mm 40-90A");
  tft.setCursor(15, 130);
  tft.println("1/8\"   3.2mm 80-130A");
  tft.setCursor(15, 160);
  tft.print("DCEP/DCEN");

  tft.setTextColor(ILI9341_YELLOW);
  tft.println("  Shallow Pen");
  tft.setCursor(15, 190);
  tft.println("High Titania Potassium");
  tft.setCursor(15, 220);
  tft.println("All Position, 60K PSI");
}

// *********************************************************************************************
// Information page for 7018 rod.
void drawInfoPage7018(void)
{
  Serial.println("Showing Information on 7018 Rod");

  page            = PG_INFO_6013;
  infoAbortMillis = millis();

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, ILI9341_BLACK);
  tft.drawRect(1, 1, tft.width() - 1, tft.height() - 1, ILI9341_WHITE);
  tft.drawRect(2, 2, tft.width() - 3, tft.height() - 2, ILI9341_WHITE);

  tft.fillRect(2, 2, tft.width() - 1, 40, ILI9341_WHITE);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(50, 32);
  tft.print("E-7018 INFORMATION");
  tft.drawBitmap(5, 5, returnBitMap, 35, 35, ILI9341_RED);

  tft.setFont(&FreeSans12pt7b);

  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(15, 70);
  tft.print("3/32\" 2.4mm 70-120A");
  tft.setCursor(15, 100);
  tft.print("1/8\"   3.2mm 110-165A");
  tft.setCursor(15, 130);
  tft.print("5/32\" 4.0mm 150-220A");
  tft.setCursor(15, 160);
  tft.print("DCEP");

  tft.setTextColor(ILI9341_YELLOW);
  tft.print("  Shallow Penetration");
  tft.setCursor(15, 190);
  tft.print("Iron Powder Low Hydrogen");
  tft.setCursor(15, 220);
  tft.print("All Position, 70K PSI");
}

// *********************************************************************************************
// Paint the heart icon, state determines color.
void drawCaution(int x, int y, bool state)
{
  int color;

  if (state) {
    color = ILI9341_YELLOW;
  }
  else {
    if (page == PG_HOME) {
      color = arcSwitch == ARC_ON ? ARC_BG_COLOR : ILI9341_BLUE;
    }
    else if ((page == PG_INFO) || (page == PG_INFO_6011) || (page == PG_INFO_6013) || (page == PG_INFO_7018)) {
      color = ILI9341_BLACK;
    }
    else if (page == PG_ERROR) {
      color = ILI9341_RED;
    }
    else {
      color = ILI9341_WHITE;
    }
  }

  tft.drawBitmap(x, y, cautionBitmap, 45, 45, color);
}

// *********************************************************************************************
// Paint the heart icon, state determines color.
// If Bluetooth is on, show bluetooth icon instead of heart.
void drawHeart(int x, int y, bool state)
{
  int color;

  tft.fillRoundRect(x - 1, y - 1, 18, 18, 5, ILI9341_WHITE);

  if (state) {
    color = isBleServerConnected() == true ? ILI9341_BLUE : ILI9341_RED;
  }
  else {
    color = ILI9341_WHITE;
  }

  if (isBleServerConnected()) {
    tft.drawBitmap(x, y, bluetoothBitmap, 16, 16, color);
  }
  else {
    tft.drawBitmap(x, y, heartBitmap, 16, 16, color);
  }
}

// *********************************************************************************************
// Home Page.
void drawErrorPage()
{
  int y = 130;

  page = PG_ERROR;
  tft.fillScreen(ILI9341_YELLOW); // CLS.

  tft.fillRect(0, 0, SCREEN_W, 100, ILI9341_RED);
  tft.setFont(&FreeMonoBold24pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(45, 40);
  tft.print("HARDWARE");
  tft.setCursor(50, 80);
  tft.print("FAILURE!");

  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK);

  if (systemError & ERROR_INA219) {
    tft.setCursor(10, y);
    tft.print("[X] Current Sensor Bad");
    y += 25;
  }

  tft.setCursor(10, y);

  if (systemError & ERROR_DIGPOT) {
    tft.print("[X] Digital POT Bad");
  }

  tft.setCursor(5, 200);
  tft.print("DO NOT USE WELDER");
  tft.setCursor(25, 225);
  tft.print("(Repairs Required)");
}

// *********************************************************************************************
// Home Page.
void drawHomePage()
{
  unsigned int color;

  page = PG_HOME;

  color = arcSwitch == ARC_ON ? ARC_BG_COLOR : ILI9341_BLUE;
  tft.fillScreen(ILI9341_BLACK); // CLS.
  tft.fillRoundRect(0, 0, SCREEN_W, SCREEN_H, 5, color);
  tft.drawRoundRect(0, 0, SCREEN_W,     SCREEN_H,     5, ILI9341_CYAN);
  tft.drawRoundRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, 5, ILI9341_CYAN);

  drawAmpsBox();
  displayOverTempAlert(); // Display temperature warning if too hot.

  // Draw Menu Icons
  tft.fillRoundRect(ARCBOX_X, ARCBOX_Y, ARCBOX_W, ARCBOX_H, ARCBOX_R, BUTTONBACKGROUND);

  if (arcSwitch == ARC_ON) {
    tft.drawBitmap(ARCBOX_X + 1, ARCBOX_Y + 2, arcOnBitmap, 45, 45, ILI9341_WHITE);
  }
  else {
    tft.drawBitmap(ARCBOX_X + 1, ARCBOX_Y + 2, arcOffBitmap, 45, 45, ILI9341_WHITE);
  }

  // Speaker Volume Icon
  updateVolumeIcon();

  // Info Icon
  tft.fillRoundRect(INFOBOX_X, INFOBOX_Y, INFOBOX_W, INFOBOX_H, INFOBOX_R, BUTTONBACKGROUND);
  tft.drawBitmap(INFOBOX_X + 1, INFOBOX_Y + 2, infoBitmap, 45, 45, ILI9341_WHITE);

  // Settings Icon
  tft.fillRoundRect(SETBOX_X, SETBOX_Y, SETBOX_W, SETBOX_H, SETBOX_R, BUTTONBACKGROUND);
  tft.drawBitmap(SETBOX_X + 1, SETBOX_Y + 2, settingsBitmap, 45, 45, ILI9341_WHITE);

  // Arrow Up Icon
  color = arcSwitch == true ? ILI9341_BLACK : ILI9341_LIGHTGREY;
  tft.fillRoundRect(AUPBOX_X, AUPBOX_Y, AUPBOX_W, AUPBOX_H, AUPBOX_R, color);
  tft.drawBitmap(AUPBOX_X + 1, AUPBOX_Y + 8, arrowUpBitmap, 45, 60, ILI9341_WHITE);

  // Arrow Down Icon
  color = arcSwitch == ARC_ON ? ILI9341_BLACK : ILI9341_LIGHTGREY;
  tft.fillRoundRect(ADNBOX_X, ADNBOX_Y, ADNBOX_W, ADNBOX_H, ADNBOX_R, color);
  tft.drawBitmap(ADNBOX_X + 1, ADNBOX_Y + 8, arrowDnBitmap, 45, 60, ILI9341_WHITE);

  drawPulseIcon();

#ifdef DEMO_MODE
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setCursor(SCREEN_W - 145, SCREEN_H - 10);
  tft.println("-DEMO-");
#endif // ifdef DEMO_MODE

  displayAmps(true);
  displayVolts(true);
}

// *********************************************************************************************
void drawPulseIcon(void)
{
  int color; // General purpose color var.

  if (pulseSwitch == PULSE_ON)
  {
    tft.fillRoundRect(PULSEBOX_X,      PULSEBOX_Y, PULSEBOX_W, PULSEBOX_H, PULSEBOX_R, BUTTONBACKGROUND); // Button Box.
    tft.fillRoundRect(PULSEBOX_X + 35, PULSEBOX_Y, PULSEBOX_W, PULSEBOX_H, PULSEBOX_R, BUTTONBACKGROUND); // Pulse Value Box.
    tft.drawBitmap(PULSEBOX_X + 1, PULSEBOX_Y + 2, pulseOnBitmap, 32, 45, LIGHT_BLUE);                    // Icon Image
    drawPulseLightning();                                                                                 // Show lightning bolt.

    tft.setTextColor(LIGHT_BLUE);
    tft.setTextSize(1);

    if (pulseFreqX10 < 10) {
      tft.setFont(&FreeSansBold12pt7b);
      tft.setCursor(PULSEBOX_X + PULSEBOX_W, PULSEBOX_Y + PULSEBOX_H - 18);
      tft.println(String(PulseFreqHz(), 1));
    }
    else {
      tft.setFont(&FreeMonoBold18pt7b);
      tft.setCursor(PULSEBOX_X + PULSEBOX_W - 18, PULSEBOX_Y + PULSEBOX_H - 15);
      tft.println(String(PulseFreqHz(), 0));
    }
  }
  else
  {
    color = arcSwitch == ARC_ON ? ARC_BG_COLOR : ILI9341_BLUE;
    tft.fillRoundRect(PULSEBOX_X + 40, PULSEBOX_Y, PULSEBOX_W, PULSEBOX_H, PULSEBOX_R, color);            // Erase of Pulse Value Box.
    tft.fillRoundRect(PULSEBOX_X,      PULSEBOX_Y, PULSEBOX_W, PULSEBOX_H, PULSEBOX_R, BUTTONBACKGROUND); // Refresh Button Box.
    tft.drawBitmap(PULSEBOX_X + 1, PULSEBOX_Y + 2, pulseOffBitmap, 45, 45, ILI9341_WHITE);                // Icon Image
  }
}

// *********************************************************************************************
// Draw (or erase) the Arc lightning Bolt on Pulse Button.
void drawPulseLightning(void)
{
  if (page == PG_HOME)
  {
    if (pulseState && (Amps >= MIN_PULSE_AMPS)) {
      tft.drawBitmap(PULSEBOX_X + 25, PULSEBOX_Y + 2, arcPulseBitmap, 21, 45, ILI9341_YELLOW);   // Draw Icon Image
    }
    else if (pulseState && (Amps < MIN_PULSE_AMPS)) {
      tft.drawBitmap(PULSEBOX_X + 25, PULSEBOX_Y + 2, arcPulseBitmap, 21, 45, LIGHT_BLUE);       // Draw Icon Image
    }
    else {
      tft.drawBitmap(PULSEBOX_X + 25, PULSEBOX_Y + 2, arcPulseBitmap, 21, 45, BUTTONBACKGROUND); // Erase Icon Image
    }
  }
}

// *********************************************************************************************
// Show Pulse Amps Settings Button Box (Left / Right arrows with pulseFreqX10 value)
void drawPulseAmpsSettings(void)
{
  if (page == PG_SET)
  {
    tft.fillRect(PCBOX_X + 150, PCBOX_Y + 4, 80, PCBOX_H - 6, ILI9341_WHITE); // Erase pulseAmpsPc Data Area for value refresh.
    tft.drawRoundRect(PCBOX_X - 2, PCBOX_Y - 4, SCREEN_W - (PCBOX_X + 15), PCBOX_H + 8, 5, ILI9341_BLACK);
    tft.drawRoundRect(PCBOX_X - 1, PCBOX_Y - 3, SCREEN_W - (PCBOX_X + 17), PCBOX_H + 6, 5, ILI9341_BLACK);
    tft.drawBitmap(PCBOX_X + 5,                        PCBOX_Y + 1, ButtonLtBitmap, 40, 40, ILI9341_BLUE);
    tft.drawBitmap(SCREEN_W - (PCBOX_W + PCBOX_X + 5), PCBOX_Y + 1, ButtonRtBitmap, 40, 40, ILI9341_BLUE);
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(PCBOX_X + PCBOX_W + 27, PCBOX_Y + 30);
    tft.println("BkGnd: " + String(pulseAmpsPc) + "%");
  }
}

// *********************************************************************************************
// Show Pulse Frequency Settings Button Box (Left / Right arrows with pulseFreqX10 value)
void drawPulseHzSettings(void)
{
  if (page == PG_SET)
  {
    tft.fillRect(PSBOX_X + 135, PSBOX_Y + 4, 80, PSBOX_H - 6, ILI9341_WHITE); // Erase PulseFreqHz Data Area for value refresh.
    tft.drawRoundRect(PSBOX_X - 2, PSBOX_Y - 4, SCREEN_W - (PSBOX_X + 15), PSBOX_H + 8, 5, ILI9341_BLACK);
    tft.drawRoundRect(PSBOX_X - 1, PSBOX_Y - 3, SCREEN_W - (PSBOX_X + 17), PSBOX_H + 6, 5, ILI9341_BLACK);
    tft.drawBitmap(PSBOX_X + 5,                        PSBOX_Y + 1, ButtonLtBitmap, 40, 40, ILI9341_BLUE);
    tft.drawBitmap(SCREEN_W - (PSBOX_W + PSBOX_X + 5), PSBOX_Y + 1, ButtonRtBitmap, 40, 40, ILI9341_BLUE);
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_BLACK);
    tft.setCursor(PSBOX_X + PSBOX_W + 23, PSBOX_Y + 30);
    tft.println("Pulse: " + String(PulseFreqHz(), 1) + " Hz");
  }
}

// *********************************************************************************************
void drawSettingsPage()
{
  Serial.println("Settings Page");
  page                = PG_SET;
  SettingsAbortMillis = millis();

  // Prepare background.
  tft.fillScreen(ILI9341_BLACK); // CLS.
  tft.fillRoundRect(0, 0, SCREEN_W, SCREEN_H, 5, ILI9341_WHITE);
  tft.drawRoundRect(0, 0, SCREEN_W,     SCREEN_H,     5, ILI9341_CYAN);
  tft.drawRoundRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, 5, ILI9341_CYAN);

  // Post Title Banner
  tft.fillRect(2, 2, tft.width() - 1, 40, ILI9341_CYAN);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(55, 32);
  tft.println("MACHINE SETTINGS");
  tft.drawBitmap(5, 5, returnBitMap, 35, 35, ILI9341_RED);

  // Show Pulse Settings Buttons (Left / Right arrows)
  drawPulseHzSettings();

  // Pulse Current Settings Button
  drawPulseAmpsSettings();

  // Show the BlueTooth Scan Button.
  tft.drawRoundRect(FBBOX_X - 2, FBBOX_Y - 4, FBBOX_W, FBBOX_H + 8, 5, ILI9341_BLACK);
  tft.drawRoundRect(FBBOX_X - 1, FBBOX_Y - 3, FBBOX_W, FBBOX_H + 6, 5, ILI9341_BLACK);
  showBleStatus(BLE_MSG_AUTO); // Show status message in Bluetooth button box.

  // Show the Bluetooth On/Off Button.
  tft.drawRoundRect(FBBOX_X + FBBOX_W + 10, FBBOX_Y - 4, BOBOX_W, FBBOX_H + 8, 5, ILI9341_BLACK);
  tft.drawRoundRect(FBBOX_X + FBBOX_W + 9,  FBBOX_Y - 3, BOBOX_W, FBBOX_H + 6, 5, ILI9341_BLACK);
}

// *********************************************************************************************
// Show the status text message inside the Bluetooth scan button box.
// Pass msgNumber for message to post.
void showBleStatus(int msgNumber)
{
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.fillRect(FBBOX_X + 2, FBBOX_Y, FBBOX_W - 5, FBBOX_H, ILI9341_WHITE); // Erase Text box area.

  // Post the requested message.
  if (msgNumber == BLE_MSG_AUTO)
  {
    if ((bleSwitch == BLE_ON) && isBleServerConnected())
    {
      tft.setTextColor(ILI9341_GREEN);
      tft.setCursor(FBBOX_X + 35, FBBOX_Y + 30);
      tft.println("Bluetooth On");
    }
    else if (bleSwitch == BLE_OFF)
    {
      tft.setTextColor(ILI9341_BLACK);
      tft.setCursor(FBBOX_X + 30, FBBOX_Y + 30);
      tft.println("Bluetooth Off");
    }
    else if (bleSwitch == BLE_ON)
    {
      tft.setTextColor(ILI9341_BLACK);
      tft.setCursor(FBBOX_X + 20, FBBOX_Y + 30);
      tft.println("Scan Bluetooth");
    }
  }
  else if (msgNumber == BLE_MSG_SCAN)
  {
    tft.setTextColor(ILI9341_BLUE);
    tft.setCursor(FBBOX_X + 50, FBBOX_Y + 30);
    tft.println("Scanning ...");
  }
  else if (msgNumber == BLE_MSG_FAIL)
  {
    tft.setTextColor(ILI9341_RED);
    tft.setCursor(FBBOX_X + 18, FBBOX_Y + 30);
    tft.println("FOB Not Found");
  }
  else if (msgNumber == BLE_MSG_FOUND)
  {
    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(FBBOX_X + 12, FBBOX_Y + 30);
    tft.println("FOB Connected");
  }

  // Update Bluetooth On/Off Switch button
  if (bleSwitch == BLE_OFF) {
    tft.drawBitmap(FBBOX_X + FBBOX_W + 14, FBBOX_Y, PowerSwBitmap, 40, 40, ILI9341_RED);
  }
  else {
    tft.drawBitmap(FBBOX_X + FBBOX_W + 14, FBBOX_Y, PowerSwBitmap, 40, 40, ILI9341_GREEN);
  }
}

// *********************************************************************************************
// Toggle the heartbeat image. Show Caution icon if system error was detected.
void showHeartbeat(void)
{
  static bool ledState       = false; // Toggle flag for onboard LED.
  static bool heartBeat      = true;  // Toggle flag for heartbeat.
  static long previousMillis = 0;     // Saved Previous time for LED flasher.

  if (millis() - previousMillis >= HB_FLASH_TIME)
  {
    previousMillis = millis();
    heartBeat      = !heartBeat;
    ledState       = !ledState;
    digitalWrite(LED_PIN, ledState);

    if (systemError == ERROR_NONE) {
      drawHeart(HEART_X, HEART_Y, heartBeat);
    }
    else {
      drawCaution(CAUTION_X, CAUTION_Y, heartBeat);
    }
  }
}

// *********************************************************************************************
void updateVolumeIcon(void)
{
  tft.fillRoundRect(SNDBOX_X, SNDBOX_Y, SNDBOX_W, SNDBOX_H, SNDBOX_R, BUTTONBACKGROUND);

  if ((spkrVolSwitch >= VOL_OFF) && (spkrVolSwitch < VOL_LOW)) // Audio Off
  {
    tft.drawBitmap(SNDBOX_X + 1, SNDBOX_Y + 2, soundOffBitmap, 45, 45, ILI9341_WHITE);
  }
  else if ((spkrVolSwitch >= VOL_LOW) && (spkrVolSwitch < VOL_MED)) // Audio Low
  {
    tft.drawBitmap(SNDBOX_X + 1, SNDBOX_Y + 2, soundBitmap, 45, 45, ILI9341_WHITE);
  }
  else if ((spkrVolSwitch >= VOL_MED) && (spkrVolSwitch < VOL_HI))           // Audio Med
  {
    tft.drawBitmap(SNDBOX_X + 1, SNDBOX_Y + 2, soundBitmap, 45, 45, ILI9341_WHITE);
    fillArc(SNDBOX_X + 8, SNDBOX_Y + 25, 62, 8, 25, 25, 2, ILI9341_WHITE);   // 1st Short arc
  }
  else if ((spkrVolSwitch >= VOL_HI) && (spkrVolSwitch < XHI_VOL))           // Audio High
  {
    tft.drawBitmap(SNDBOX_X + 1, SNDBOX_Y + 2, soundBitmap, 45, 45, ILI9341_WHITE);
    fillArc(SNDBOX_X + 8,  SNDBOX_Y + 25, 62, 8,  25, 25, 2, ILI9341_WHITE); // 1st Short arc
    fillArc(SNDBOX_X + 18, SNDBOX_Y + 25, 56, 10, 25, 25, 2, ILI9341_WHITE); // 2nd Short arc
  }
  else
  {
    tft.drawBitmap(SNDBOX_X + 1, SNDBOX_Y + 2, soundBitmap, 45, 45, ILI9341_WHITE);
    fillArc(SNDBOX_X + 8,  SNDBOX_Y + 25, 62, 8,  25, 25, 2, ILI9341_WHITE); // 1st Short arc
    fillArc(SNDBOX_X + 14, SNDBOX_Y + 25, 45, 13, 30, 30, 3, ILI9341_WHITE); // Long arc
  }
}

// EOF
