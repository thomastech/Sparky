/*
   File: screen.cpp
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Version: 1.3
   Creation: Sep-11-2019
   Revised: Jan-20-2020.
   Public Release: Jan-20-2020
   Revision History: See PulseWelder.cpp
   Project Leader: T. Black (thomastech)
   Contributors: thomastech, hogthrob

   (c) copyright T. Black 2019-2020, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.
*/

#include <Arduino.h>
#include <EEPROM.h>
#include "PulseWelder.h"
#include "screen.h"
#include "digPot.h"
#include "config.h"
#include "speaker.h"

// Touch Screen
extern XPT2046_Touchscreen ts;
extern Adafruit_ILI9341    tft;

// Global Vars
extern int  Amps;             // Live Welding Output Current.
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



// local Scope Vars
static char StringBuff[32];          // General purpose Char Buffer
static bool eepromActive = false;    // EEProm Write Requested.
static int  page = PG_HOME;          // Current Menu Page.
static int  x, y;                    // Screen's Touch coordinates.
static long abortMillis     = 0;     // Info Page Abort Timer, in mS.
static long previousEepMillis   = 0; // Previous Home Page time.

#define COORD(BOXNAME) BOXNAME ## _X , BOXNAME ## _Y , BOXNAME ## _W , BOXNAME ## _H
#define IS_IN_BOX(BOXNAME) (isInBox(x, y, COORD(BOXNAME)))


// *********************************************************************************************
// Change Welder's Pulse Mode amps, Increase or decrement from 10% to 90%.
// Used by processScreen().
// On exit, true returned if end of travel was reached.
bool adjustPulseAmps(bool direction)
{
  bool limitHit = false;

  // Increase or decrease the value. Range is MIN_PULSE_Amps to MAX_PULSE_Amps.
  if ((direction == INCR) && (pulseAmpsPc < MAX_PULSE_AMPS_PC)) {
    pulseAmpsPc += PULSE_AMPS_STEP;
  }
  else if ((direction == DECR) && (pulseAmpsPc > MIN_PULSE_AMPS_PC)) {
    pulseAmpsPc -= PULSE_AMPS_STEP;
  }

  // Check for out of bounds values. Constrain in necessary.
  if (pulseAmpsPc >= MAX_PULSE_AMPS_PC) {
    pulseAmpsPc = MAX_PULSE_AMPS_PC;
    limitHit    = true;
  }
  else if (pulseAmpsPc <= MIN_PULSE_AMPS_PC) {
    pulseAmpsPc = MIN_PULSE_AMPS_PC;
    limitHit    = true;
  }

  // Refresh the Pulse Entry display (if on page PG_SET)
  drawPulseAmpsSettings(true);

  // Let the caller know that we would like to update the EEPROM if the value has changed.
  eepromActive      = true;     // Request EEPROM save for new settings.
  previousEepMillis = millis(); // Set EEPROM write delay timer.

  return limitHit;
}

// *********************************************************************************************
// Change Welder's Pulse Frequency, Increase or decrement from 0.4 to 5.0 Hz.
// Supports 0.4Hz to 0.9Hz (0.1Hz increments) and 1Hz to 5Hz (1Hz increments).
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
  byte constrainedPulseFreqX10 = constrain(pulseFreqX10, MIN_PULSE_FRQ_X10, MAX_PULSE_FRQ_X10);

  limitHit = constrainedPulseFreqX10 != pulseFreqX10;
  // if constrained freq is not equal original frequency we are out of bounds

  pulseFreqX10 = constrainedPulseFreqX10;
  // always use constrained frequency

  // Refresh the Pulse Entry display (if on page PG_SET)
  drawPulseHzSettings(true);

  // Let the caller know that we would like to update the EEPROM if the value has changed.
  eepromActive      = true;     // Request EEPROM save for new settings.
  previousEepMillis = millis(); // Set EEPROM write delay timer.

  return limitHit;
}


// *********************************************************************************************
// Update EEPROM with new data.
// Note: This function does not commit the new data; For flash wear protection the actual data
// write involves a timer, see processScreen().
static bool checkAndUpdateEEPROM(int addr, byte value, const char *label, const char *valueString = NULL)
{
  bool retval = false;
  if (EEPROM.read(addr) != value)
  {                      // Save new Current setting.
    eepromActive = true; // New Data is available to write.
    EEPROM.write(addr, value);
    Serial.println("Write E2Prom Addr: " + String(addr) + ", " + label + ": " + (valueString == NULL?String(value) : valueString));
  }
  return retval;
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
// Check to see if touch coordinates are inside the defined area.
bool isInBox(int x, int y, int bx, int by, int bw, int bh)
{
  return (((x >= bx) && (x <= bx + bw)) && ((y >= by) && (y <= by + bh)));
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
void drawCenteredText(int x, int y, int w, int h, String label, uint32_t bgcolor)
{
  int16_t lx,ly;
  uint16_t lw, lh;

  // calculate text length
  tft.getTextBounds(label, x , y , &lx,  &ly, &lw, &lh);
  int nxd = (w - lw)/2; // calculate offset from left margin to print label centered
  tft.fillRect(x, y, w, h, bgcolor); // Erase Data Area for value refresh.

  tft.setCursor(x + nxd, y + (lh + h)/2 );
  tft.println(label);
}

// *********************************************************************************************
void drawBasicButton(int x, int y, int w, int h, uint color)
{
  tft.drawRoundRect(x - 2, y - 4, w+4, h + 8, 5, color);
  tft.drawRoundRect(x - 1, y - 3, w+2, h + 6, 5, color);
}

// *********************************************************************************************
void drawPlusMinusButtons(int x, int y, int w, int h, String label, bool update_only)
{
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK);
  drawCenteredText(x+45, y, w-90, h, label, ILI9341_WHITE);

  if (update_only == false) {
    drawBasicButton( x, y , w, h, ILI9341_BLACK);
    tft.drawBitmap(x + 5, y , ButtonLtBitmap, 40, 40, ILI9341_BLUE);
    tft.drawBitmap((x + w - 5 - 40), y , ButtonRtBitmap, 40, 40, ILI9341_BLUE);
  }
}

// *********************************************************************************************
void handleRodInfoPage(String rodName, bool& wasTouched)
{
  if (!ts.touched())
  {
    wasTouched = false;

    if (millis() > abortMillis + PG_RD_TIME_MS)
    {
      Serial.println(rodName + " Info page timeout, exit.");
      abortMillis = millis();// Reset the info page's keypress abort timer.
      drawInfoPage();

      spkr.lowBeep();
    }
  }
  else if (ts.touched() && !wasTouched)
  {
    abortMillis = millis();
    wasTouched      = true;
    getTouchPoints();

    if (IS_IN_BOX(SCREEN))// Press anywhere on screen to main info page.
    {
      Serial.println(String("User Exit ") + rodName +  " Info, returned to main info page");
      abortMillis = millis();
      drawInfoPage();

      spkr.lowBeep();
    }
  }
}

// *********************************************************************************************
// Process the TouchScreen actions.
// Display screen pages, get touch inputs, perform actions.
// All screens and touch events are performed here.
void processScreen(void)
{
  bool limitHit                  = false;         // Control reached end of travel.
  static bool setAmpsActive      = false;         // Amps Setting flag.
  long bleWaitMillis             = millis();      // BLE Scan Timer.
  static bool wasTouched         = false;         // Touch Display has detected a press.
  static int  repeatCnt          = 0;             // Threshold counter for held keypress.
  static int  repeatms           = REPEAT_SLOW_MS;// Millisecond delay for held keypress.
  static long arrowMillis        = 0;             // Timer for held Up/Down Arrow key repeat feature.
  static long dbncMillis         = 0;             // Touch Screen debouce timer.
  static long homeMillis         = 0;             // Home Page timer for data refresh.
  static long previousHomeMillis = 0;             // Previous Home Page timer.
  static long setAmpsTimer       = 0;             // Amps setting changed by user timer, for Amps refresh.

  if (eepromActive)
  {
    if (millis() - previousEepMillis >= EEP_DELAY_TIME) {
      eepromActive  = false;// Reset EEProm write check Flag.
      eepromActive |= checkAndUpdateEEPROM(AMP_SET_ADDR, setAmps, "Amp Setting");
      eepromActive |= checkAndUpdateEEPROM(VOL_SET_ADDR, spkrVolSwitch, "Volume");
      eepromActive |= checkAndUpdateEEPROM(PULSE_FRQ_ADDR, pulseFreqX10, "Pulse Freq", (String(PulseFreqHz(), 1) + String(" Hz")).c_str());
      eepromActive |= checkAndUpdateEEPROM(PULSE_AMPS_ADDR, pulseAmpsPc, "Pulse Modulation Current", (String(pulseAmpsPc) + "%").c_str());
      eepromActive |= checkAndUpdateEEPROM(PULSE_SW_ADDR, pulseSwitch, "Pulse Mode", pulseSwitch == PULSE_ON ? "On" : "Off");
      eepromActive |= checkAndUpdateEEPROM(ARC_SW_ADDR, arcSwitch, "Arc Power", arcSwitch == ARC_ON ? "On" : "Off");
      eepromActive |= checkAndUpdateEEPROM(BLE_SW_ADDR, bleSwitch, "Bluetooth", bleSwitch == PULSE_ON ? "On" : "Off");

      if (eepromActive) {// New data available to write. Commit it to the flash.
        eepromActive = false;
        EEPROM.commit();
      }
    }
  }

  if (page == PG_HOME)
  {// Home page.
    homeMillis = millis();

    if (homeMillis - previousHomeMillis >= DATA_REFRESH_TIME) {
      previousHomeMillis = homeMillis;

      // drawBattery(BATTERY_X, BATTERY_Y);
      displayOverTempAlert();// Display temperature warning if too hot.
      displayAmps(false);
      displayVolts(false);
    }

    if ((setAmpsTimerFlag == true) && (homeMillis >= setAmpsTimer + SET_AMPS_TIME)) {
      setAmpsTimerFlag = false;// Amps setting (by user) timer has expired.
    }

    if (!ts.touched() && wasTouched && (homeMillis > dbncMillis + TOUCH_DBNC)) {
      wasTouched    = false;
      setAmpsActive = false;
    }
    else if (!ts.touched()) {
      repeatCnt = 0;                       // Event counter for when key press is held.
      repeatms  = REPEAT_SLOW_MS;          // Init to slow time delay on held key press, in mS.
    }
    else if (ts.touched() && !wasTouched) {// Debounce the touchscreen to prevents multiple inputs from single touch.
      dbncMillis = millis();

      wasTouched = true;
      getTouchPoints();

      if (IS_IN_BOX(ARCBOX))
      {
        if (overTempAlert) {// Alarm state. Do not enable welding current!
          arcSwitch = ARC_OFF;
          Serial.println("Alarm State! Arc Current Cannot be Enabled.");
          spkr.bloop();
        }
        else {
          controlArc(arcSwitch == ARC_ON ? ARC_OFF : ARC_ON, VERBOSE_ON);// Toggle arcSwitch, Update Arc current on/off.
          drawHomePage();

          if (arcSwitch == ARC_ON) {
            spkr.highBeep();
          }
          else {
            spkr.lowBeep();
          }
          previousEepMillis = millis();
          eepromActive      = true;// Request EEProm Write after timer expiry.
        }
      }
      else if (IS_IN_BOX(SNDBOX))
      {// Adjust Audio Volume
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
          spkr.volume(VOL_LOW);// Termporaily Use soft volume for Audio feedback.
          spkr.playToEnd(lowBeep);
          spkr.volume(VOL_OFF);// Now turn off volume.
          // DacAudio.StopAllSounds();
          Serial.println("Sound Disabled.");
        }
        else {
          spkr.volume(spkrVolSwitch);
          spkr.highBeep();
          Serial.println("Sound Set to Volume " + String(spkrVolSwitch));
        }
        previousEepMillis = millis();
        eepromActive      = true;// Request EEProm Write after timer expiry.
        updateVolumeIcon();
      }
      else if (IS_IN_BOX(INFOBOX))
      {// Info button pressed
        drawInfoPage();
        spkr.highBeep();
      }
      else if (IS_IN_BOX(SETBOX))
      {// Settings button pressed
        if (spkrVolSwitch != VOL_OFF) {
          spkr.stopSounds();
          spkr.playToEnd(highBeep);
        }
        drawSettingsPage();
      }

      else if (IS_IN_BOX(AUPBOX))
      {                                               // Increase Amps Setting
        if ((arcSwitch == ARC_OFF) || overTempAlert) {// Welding current disabled.
          Serial.println("Arc Current Off: Amp setting cannot be changed.");
          spkr.bloop();
        }
        else {
          setAmpsTimerFlag = true;
          setAmpsTimer     = millis();
          setAmpsActive    = true;
          setAmps++;
          setAmps = constrain(setAmps, MIN_SET_AMPS, MAX_SET_AMPS);
          setPotAmps(setAmps, VERBOSE_ON);// Refresh Digital Pot.
          displayAmps(true);              // Refresh displayed value.
          arrowMillis       = millis();
          previousEepMillis = millis();
          eepromActive      = true;       // Request EEProm Write after timer expiry.

          if (setAmps < MAX_SET_AMPS) {
            spkr.bleep();
          }
          else {
            spkr.bloop();
          }
        }
      }

      else if (IS_IN_BOX(ADNBOX))
      {                                               // Decrease Amps Setting
        if ((arcSwitch == ARC_OFF) || overTempAlert) {// Welding current disabled.
          Serial.println("Arc Current Off: Amp setting cannot be changed.");
          spkr.bloop();
        }
        else {
          setAmpsTimerFlag = true;
          setAmpsTimer     = millis();
          setAmpsActive    = true;

          if (setAmps > 0) {
            setAmps--;
          }

          setAmps = constrain(setAmps, MIN_SET_AMPS, MAX_SET_AMPS);
          setPotAmps(setAmps, VERBOSE_ON);// Refresh Digital Pot.
          displayAmps(true);              // Refresh displayed value.
          arrowMillis       = millis();
          previousEepMillis = millis();
          eepromActive      = true;       // Request EEProm Write after timer expiry.

          if (setAmps > MIN_SET_AMPS) {
            spkr.bleep();
          }
          else {
            spkr.bloop();
          }
        }
      }
      else if (IS_IN_BOX(PULSEBOX))
      {
        pulseSwitch = pulseSwitch == PULSE_ON ? false : true;// Toggle psuedo boolean

        if (pulseSwitch == PULSE_ON) {
          spkr.highBeep();
          Serial.println("Pulse Mode On: " + String(PulseFreqHz(), 1) + " Hz, " + String(pulseAmpsPc) + "\% Amps");
        }
        else {
          spkr.lowBeep();
          Serial.println("Pulse Mode Off");
        }

        drawPulseIcon();
        displayAmps(true);       // Refresh Amps display to update background color.
        controlArc(arcSwitch, VERBOSE_OFF);
        previousEepMillis = millis();
        eepromActive      = true;// Request EEProm Write after timer expiry.
      }
    }
    else if (ts.touched() && wasTouched)
    {
      if (setAmpsActive == true) {
        if ((millis() > arrowMillis + repeatms)) {
          int valChange = 0;

          if (IS_IN_BOX(AUPBOX)) {
            valChange = 1;
          } else if (IS_IN_BOX(ADNBOX)) {
            valChange = -1;
          }

          if (valChange != 0)
          {
            setAmps         += valChange;
            setAmpsTimerFlag = true;
            setAmpsTimer     = millis();

            if (repeatCnt > REPEAT_CNT_THRS) {
              repeatms = REPEAT_FAST_MS;// millisecond delay, fast.
            }
            else {
              repeatCnt++;
              repeatms = REPEAT_SLOW_MS;// millisecond delay, slow.
            }
            arrowMillis       = millis();
            previousEepMillis = millis();
            eepromActive      = true;       // Request EEProm Write after timer expiry.

            setAmps = constrain(setAmps, MIN_SET_AMPS, MAX_SET_AMPS);
            setPotAmps(setAmps, VERBOSE_ON);// Refresh Digital Pot.
            displayAmps(true);              // Refresh amps value.

            if (repeatCnt == 1) {           // First Repeated keypress.
              spkr.ding();
            }
            else {                          // Ongoing repeats.
              if ((setAmps == MAX_SET_AMPS) || (setAmps == MIN_SET_AMPS)) {
                spkr.bloop();
              }
              else {
                spkr.blip();
              }
            }
          }
        }
      }
    }
  }
  else if (page == PG_INFO)// Information page. All display elements are drawn when drawInfoPage() is called.
  {
    if (!ts.touched())
    {
      wasTouched = false;

      if (millis() > abortMillis + MENU_RD_TIME_MS) {
        Serial.println("Main Info page timeout, exit.");
        drawHomePage();

        spkr.lowBeep();
      }
    }
    else if (ts.touched() && !wasTouched) {
      abortMillis = millis();
      wasTouched      = true;
      getTouchPoints();

      if (IS_IN_BOX(RTNBOX))// Return button. Return to homepage.
      {
        Serial.println("User Exit Info, retured to home page");
        drawHomePage();

        spkr.lowBeep();
      }
      else if (isInBox(x, y, 45, 70, 225, 30))
      {
        abortMillis = millis();
        drawInfoPage6011();

        spkr.highBeep();
      }
      else if (isInBox(x, y, 45, 125, 225, 30))
      {
        abortMillis = millis();
        drawInfoPage6013();

        spkr.highBeep();
      }
      else if (isInBox(x, y, 45, 175, 225, 30))
      {
        abortMillis = millis();
        drawInfoPage7018();

        spkr.highBeep();
      }
    }
  }

  else if (page == PG_INFO_6011)// Information page on 6011 Rod.
  {
    handleRodInfoPage("6011", wasTouched);
  }
  else if (page == PG_INFO_6013)// Information page on 6013 Rod.
  {
    handleRodInfoPage("6013", wasTouched);
  }
  else if (page == PG_INFO_7018)// Information page on 7018 Rod.
  {
    handleRodInfoPage("7018", wasTouched);
  }

  else if (page == PG_SET)// Settings Page
  {
    if (!ts.touched())
    {
      wasTouched = false;

      if (millis() > abortMillis + PG_RD_TIME_MS)
      {
        Serial.println("Machine Settings page timeout, exit.");
        abortMillis = millis();// Reset the settings page's keypress abort timer.
        drawHomePage();

        spkr.lowBeep();
      }
    }
    else if (ts.touched() && !wasTouched)
    {
      abortMillis = millis();
      wasTouched          = true;
      getTouchPoints();

      if (IS_IN_BOX(RTNBOX))// Return button. Return to home page.
      {
        Serial.println("User Exit Machine Settings, returned to Home page");
        abortMillis = millis();
        drawHomePage();

        spkr.lowBeep();
      }
      else if (isInBox(x, y, PSBOX_X  + PSBOX_W - 45, PSBOX_Y, 45, PSBOX_H))
      {
        limitHit = adjustPulseFreq(INCR);
        Serial.println("Increased Pulse Freq: " + String(PulseFreqHz(), 1) + " Hz");// Increase Pulse Freq.

        spkr.limitHit(blip, limitHit);
      }
      else if (isInBox(x, y, PSBOX_X, PSBOX_Y, 45, PSBOX_H))
      {
        limitHit = adjustPulseFreq(DECR);
        Serial.println("Decreased Pulse Freq: " + String(PulseFreqHz(), 1) + " Hz");// Decrease Pulse Freq.

        spkr.limitHit(bleep, limitHit);
      }
      else if (isInBox(x, y, PCBOX_X, PCBOX_Y, 45, PCBOX_H))
      {
        limitHit = adjustPulseAmps(DECR);
        Serial.println("Decreased Pulse Current: " + String(pulseAmpsPc) + "%");// Decrease Pulse Amps (%).

        spkr.limitHit(bleep, limitHit);
      }
      else if (isInBox(x, y, PCBOX_X  + PCBOX_W - 45, PCBOX_Y, 45, PCBOX_H))
      {
        limitHit = adjustPulseAmps(INCR);
        Serial.println("Increased Pulse Current: " + String(pulseAmpsPc) + "%");// Increase Pulse Amps (%).

        spkr.limitHit(blip, limitHit);
      }
      else if (IS_IN_BOX(BOBOX))
      {
        bleSwitch    = (bleSwitch == BLE_ON ? BLE_OFF : BLE_ON);// Pseudo Boolean toggle.
        eepromActive = true;                                    // Request EEPROM save for new settings.

        if ((bleSwitch == BLE_OFF) && (isBleServerConnected() == true)) {
          stopBle();
        }
        previousEepMillis = millis();// Set EEPROM write delay timer.
        showBleStatus(BLE_MSG_AUTO); // Update the Bluetooth On/Off button.
        Serial.println("Bluetooth Mode: " + String(bleSwitch == BLE_ON ? "ON" : "OFF"));

        spkr.play((bleSwitch == BLE_ON) ? blip : bleep);
      }
      else if (isInBox(x, y, FBBOX_X + 5, FBBOX_Y - 4, FBBOX_W - 15, FBBOX_H - 6))
      {
        if (bleSwitch == BLE_OFF)
        {
          spkr.bloop();
          Serial.println("Bluetooth Disabled!");
        }
        else if (isBleServerConnected())
        {
          spkr.bleep();
          Serial.println("Bluetooth Already Connected!");
          showBleStatus(BLE_MSG_FOUND);
        }
        else
        {
          showBleStatus(BLE_MSG_SCAN);// Post "Bluetooth Scanning" message.
          spkr.playToEnd(blip);

          if (isBleDoScan()) {
            Serial.println("User Requested BlueTooth Reconnect.");
            reconnectBlueTooth(BLE_RESCAN_TIME);// Try to Reconnect to lost connection.
          }
          else {
            Serial.println("User Requested Fresh BlueTooth Scan.");
            scanBlueTooth();                                                 // Fresh Scan, never connected before.
          }

          reconnectTimer(true);                                              // Reset Auto-Reconnect Timer.
          bleWaitMillis = millis();

          while (millis() <= bleWaitMillis + 2000 && !isBleServerConnected())// Wait for advert callback to confirm connect.
          {
            spkr.fillBuffer();                                               // Fill the sound buffer with data.
            showHeartbeat();                                                 // Draw Heartbeat icon.
            checkBleConnection();                                            // Check the Bluetooth iTAG FOB Button server connection.
          }

          if (isBleServerConnected()) {
            showBleStatus(BLE_MSG_FOUND);// Post "Found" message.
          }
          else {
            showBleStatus(BLE_MSG_FAIL); // Post "Not Found"
          }
        }
      }
    }
  }

  else if (page == PG_ERROR)// System Error page.
  {
    if (!ts.touched())
    {
      wasTouched = false;
    }
    else if (ts.touched() && !wasTouched)
    {
      wasTouched = true;
      getTouchPoints();

      if (IS_IN_BOX(SCREEN))// Press anywhere on screen to main info page.
      {
        Serial.println("User Proceeded to Home Page despite Error Warning.");
        drawHomePage();

        spkr.lowBeep();
      }
    }
  }
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
    drawOverTempAlert();    // Will Redraw each screen refresh; flashing gets attention.
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
    #ifdef PWM_ARC_CTRL
      dispAmps = 0; // Arc current disabled via PWM Shutdown (fully turned off).
    #else
      dispAmps = ARC_OFF_AMPS; // Arc Current set to minimum via digital pot.
    #endif
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
// Display the Over-Temperature (OC LED) Alert Message.
void displayOverTempAlert(void)
{
  static bool detFlag = false; // Front Panel OC LED detection flag.

  if (detFlag && !overTempAlert)
  {
    detFlag = false;  // Prevent re-entry.
    drawAmpsBox();
    displayAmps(true);
  }
  else if (!detFlag && overTempAlert)
  {
    detFlag = true;
    Serial.println("Warning: Over-Temperature has been detected!");
    drawOverTempAlert();
    spkr.stopSounds();  // Override existing announcement.
    spkr.playToEnd(overHeatMsg);
    // Don't interrupt a over-heat alarm message playback.
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
void drawPageFrame(uint32_t bgColor, uint32_t marginColor) {
  tft.fillScreen(ILI9341_BLACK); // CLS.
  tft.fillRoundRect(0, 0, SCREEN_W, SCREEN_H, 5, bgColor);
  tft.drawRoundRect(0, 0, SCREEN_W,     SCREEN_H,     5, marginColor);
  tft.drawRoundRect(1, 1, SCREEN_W - 2, SCREEN_H - 2, 5, marginColor);
}

// *********************************************************************************************
void drawSubPage(String title, int pg, uint32_t bgColor, uint32_t marginColor) {

  Serial.println(String("Page: ") + title);
  page                = pg;
  abortMillis = millis();

  // Prepare background.
  drawPageFrame(bgColor, marginColor);

  // Post Title Banner
  tft.fillRect(2, 2, tft.width() - 4, 40, marginColor);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_BLACK);
  tft.setCursor(55, 32);
  tft.println("MACHINE SETTINGS");
  tft.drawBitmap(5, 5, returnBitMap, 35, 35, ILI9341_RED);
}

// *********************************************************************************************
// Information page.
void drawInfoPage(void)
{
  drawSubPage("ROD INFORMATION", PG_INFO, ILI9341_BLACK, ILI9341_WHITE);

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
// Information page for XXXX rod.
void drawInfoPageRod(int pageNum, const char* rodName, const char* mainInfo[4], const char* rodInfo[3])
{
  drawSubPage(String(rodName) + " INFORMATION", pageNum, ILI9341_BLACK, ILI9341_WHITE);

  tft.setFont(&FreeSans12pt7b);

  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(15, 70);
  tft.println(mainInfo[0]);
  tft.setCursor(15, 100);
  tft.println(mainInfo[1]);
  tft.setCursor(15, 130);
  tft.println(mainInfo[2]);
  tft.setCursor(15, 160);
  tft.print(mainInfo[3]);

  tft.setTextColor(ILI9341_YELLOW);
  tft.println(rodInfo[0]);
  tft.setCursor(15, 190);
  tft.println(rodInfo[1]);
  tft.setCursor(15, 220);
  tft.println(rodInfo[2]);
}

// *********************************************************************************************
// Information page for 6011 rod.
void drawInfoPage6011(void) {
  const char* mainInfo[4] =                 {
                    "3/32\" 2.4mm 40-90A",
                    "1/8\"   3.2mm 75-125A",
                    "5/32\" 4.0mm 110-165A",
                    "DCEP",
                  };
  const char* rodInfo[3] = {
                    "  Deep Penetration",
                    "High Cellulose Potassium",
                    "All Position, 60K PSI",
                  };

  drawInfoPageRod(PG_INFO_6011, "E-6011", mainInfo, rodInfo);
}

// *********************************************************************************************
// Information page for 6013 rod.
void drawInfoPage6013(void) {
  const char* mainInfo[4] =                 {
                    "1/16\" 1.6mm 20-45A",
                    "3/32\" 2.4mm 40-90A",
                    "1/8\"   3.2mm 80-130A",
                    "DCEP/DCEN"
                  };
  const char* rodInfo[3] = {
                    "  Shallow Pen",
                    "High Titania Potassium",
                    "All Position, 60K PSI"
                  };

  drawInfoPageRod(PG_INFO_6013, "E-6013", mainInfo, rodInfo);
}

// Information page for 7018 rod.
void drawInfoPage7018(void) {
  const char* mainInfo[4] =                 {
                    "3/32\" 2.4mm 70-120A",
                    "1/8\"   3.2mm 110-165A",
                    "5/32\" 4.0mm 150-220A",
                    "DCEP"
                  };
  const char* rodInfo[3] = {
                    "  Shallow Penetration",
                    "Iron Powder Low Hydrogen",
                    "All Position, 70K PSI"
                  };

  drawInfoPageRod(PG_INFO_7018, "E-7018", mainInfo, rodInfo);
}


// *********************************************************************************************
// Paint the Caution icon, state determines color.
void drawCaution(int x, int y, bool state)
{
  unsigned int color;

  if (state) {
    color = ILI9341_YELLOW;
  }
  else {
    switch(page) {
      case PG_HOME:
        color = arcSwitch == ARC_ON ? ARC_BG_COLOR : ILI9341_BLUE;
        break;
      case PG_INFO:
      case PG_INFO_6011:
      case PG_INFO_6013:
      case PG_INFO_7018:
        color = ILI9341_BLACK;
        break;
      case PG_ERROR:
        color = ILI9341_RED;
        break;
      default:
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
  page = PG_HOME;

  unsigned int color = arcSwitch == ARC_ON ? ARC_BG_COLOR : ILI9341_BLUE;
  drawPageFrame(color, ILI9341_CYAN);

  drawAmpsBox();
  displayOverTempAlert(); // Display temperature warning if too hot.

  // Draw Menu Icons
  tft.fillRoundRect(ARCBOX_X, ARCBOX_Y, ARCBOX_W, ARCBOX_H, ARCBOX_R, BUTTONBACKGROUND);
  tft.drawBitmap(ARCBOX_X + 1, ARCBOX_Y + 2, arcSwitch == ARC_ON? arcOnBitmap : arcOffBitmap, 45, 45, ILI9341_WHITE);

  // Speaker Volume Icon
  updateVolumeIcon();

  // Info Icon
  tft.fillRoundRect(INFOBOX_X, INFOBOX_Y, INFOBOX_W, INFOBOX_H, INFOBOX_R, BUTTONBACKGROUND);
  tft.drawBitmap(INFOBOX_X + 1, INFOBOX_Y + 2, infoBitmap, 45, 45, ILI9341_WHITE);

  // Settings Icon
  tft.fillRoundRect(SETBOX_X, SETBOX_Y, SETBOX_W, SETBOX_H, SETBOX_R, BUTTONBACKGROUND);
  tft.drawBitmap(SETBOX_X + 1, SETBOX_Y + 2, settingsBitmap, 45, 45, ILI9341_WHITE);

  unsigned int arrowColor;
  if(overTempAlert) {
      arrowColor = ILI9341_LIGHTGREY;
  }
  else {
   arrowColor = arcSwitch == ARC_ON ? ILI9341_BLACK : ILI9341_LIGHTGREY;
  }

  // Arrow Up Icon
  tft.fillRoundRect(AUPBOX_X, AUPBOX_Y, AUPBOX_W, AUPBOX_H, AUPBOX_R, arrowColor);
  tft.drawBitmap(AUPBOX_X + 1, AUPBOX_Y + 8, arrowUpBitmap, 45, 60, ILI9341_WHITE);

  // Arrow Down Icon
  tft.fillRoundRect(ADNBOX_X, ADNBOX_Y, ADNBOX_W, ADNBOX_H, ADNBOX_R, arrowColor);
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
    unsigned int color = arcSwitch == ARC_ON ? ARC_BG_COLOR : ILI9341_BLUE;
    tft.fillRoundRect(PULSEBOX_X + 40, PULSEBOX_Y, PULSEBOX_W, PULSEBOX_H, PULSEBOX_R, color);            // Erase of Pulse Value Box.
    tft.fillRoundRect(PULSEBOX_X,      PULSEBOX_Y, PULSEBOX_W, PULSEBOX_H, PULSEBOX_R, BUTTONBACKGROUND); // Refresh Button Box.
    tft.drawBitmap(PULSEBOX_X + 1, PULSEBOX_Y + 2, pulseOffBitmap, 45, 45, ILI9341_WHITE);                // Icon Image
  }
}

// *********************************************************************************************
// Draw Over Temperature Alert in Amps display area.
void drawOverTempAlert(void) {
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

// *********************************************************************************************
// Draw (or erase) the Arc lightning Bolt on Pulse Button.
void drawPulseLightning(void)
{
  if (page == PG_HOME)
  {
    unsigned int color;
    if (pulseState) {
      color = Amps >= PULSE_AMPS_THRS ? ILI9341_YELLOW : LIGHT_BLUE;
    }
    else {
      color = BUTTONBACKGROUND; // Erase Icon Image
    }
    tft.drawBitmap(PULSEBOX_X + 25, PULSEBOX_Y + 2, arcPulseBitmap, 21, 45, color);
  }
}

// *********************************************************************************************
// Show Pulse Amps Settings Button Box (Left / Right arrows with pulseFreqX10 value)
void drawPulseAmpsSettings(bool update_only)
{
    if (page == PG_SET) {
      drawPlusMinusButtons(COORD(PCBOX), "BkGnd: " + String(pulseAmpsPc) + "%", update_only);
    }
}

// *********************************************************************************************
// Show Pulse Frequency Settings Button Box (Left / Right arrows with pulseFreqX10 value)
void drawPulseHzSettings(bool update_only)
{
  if (page == PG_SET) {
    drawPlusMinusButtons(COORD(PSBOX), "Pulse: " + String(PulseFreqHz(), 1) + " Hz", update_only);
  }
}

// *********************************************************************************************
void drawSettingsPage()
{
  drawSubPage("MACHINE SETTINGS", PG_SET, ILI9341_WHITE, ILI9341_CYAN);

  // Show Pulse Settings Buttons (Left / Right arrows)
  drawPulseHzSettings(false);

  // Pulse Current Settings Button
  drawPulseAmpsSettings(false);

  // Show the BlueTooth Scan Button.
  drawBasicButton(COORD(FBBOX),ILI9341_BLACK);

  showBleStatus(BLE_MSG_AUTO); // Show status message in Bluetooth button box.

  // Show the Bluetooth On/Off Button.
  drawBasicButton(FBBOX_X + FBBOX_W + 12,FBBOX_Y, BOBOX_W, FBBOX_H, ILI9341_BLACK);

}


// *********************************************************************************************
// Show the status text message inside the Bluetooth scan button box.
// Pass msgNumber for message to post.
void showBleStatus(int msgNumber)
{
  String label;
  uint32_t color = ILI9341_BLACK;

  // Post the requested message.
  if (msgNumber == BLE_MSG_AUTO)
  {
    if ((bleSwitch == BLE_ON) && isBleServerConnected())
    {
      color = ILI9341_GREEN;
      label = "Bluetooth On";
    }
    else if (bleSwitch == BLE_OFF)
    {
      label = "Bluetooth Off";
    }
    else if (bleSwitch == BLE_ON)
    {
      label = "Scan Bluetooth";
    }
  }
  else if (msgNumber == BLE_MSG_SCAN)
  {
    color = ILI9341_BLUE;
    label = "Scanning ...";
  }
  else if (msgNumber == BLE_MSG_FAIL)
  {
    color = ILI9341_RED;
    label = "FOB Not Found";
  }
  else if (msgNumber == BLE_MSG_FOUND)
  {
    color = ILI9341_GREEN;
    label = "FOB Connected";
  }

  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);
  tft.setTextColor(color);
  drawCenteredText(COORD(FBBOX), label, ILI9341_WHITE);

  // Update Bluetooth On/Off Switch button
  tft.drawBitmap(FBBOX_X + FBBOX_W + 17, FBBOX_Y, PowerSwBitmap, 40, 40, bleSwitch == BLE_OFF ? ILI9341_RED : ILI9341_GREEN);

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

  if ((spkrVolSwitch >= VOL_OFF) && (spkrVolSwitch < VOL_LOW)) { // Audio Off
    tft.drawBitmap(SNDBOX_X + 1, SNDBOX_Y + 2, soundOffBitmap, 45, 45, ILI9341_WHITE);
  }
  else if (spkrVolSwitch >= VOL_LOW) {
    tft.drawBitmap(SNDBOX_X + 1, SNDBOX_Y + 2, soundBitmap, 45, 45, ILI9341_WHITE);

    if (spkrVolSwitch >= VOL_MED) {
      fillArc(SNDBOX_X + 8, SNDBOX_Y + 25, 62, 8, 25, 25, 2, ILI9341_WHITE);   // 1st Short arc

      if ((spkrVolSwitch >= VOL_HI) && (spkrVolSwitch < XHI_VOL)) {          // Audio High
        fillArc(SNDBOX_X + 18, SNDBOX_Y + 25, 56, 10, 25, 25, 2, ILI9341_WHITE); // 2nd Short arc
      }
      else if (spkrVolSwitch >= XHI_VOL) { // Audio Extra High
        fillArc(SNDBOX_X + 14, SNDBOX_Y + 25, 45, 13, 30, 30, 3, ILI9341_WHITE); // Long arc
      }
    }
  }
}

// EOF
