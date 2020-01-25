/*
   File: measure.cpp
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

   Notes:
   1. The INA219 library is custom patched; It has been modified for use with the Sparky project.
   2. The INA219 "High-Side" current sensor is being used in a Low-side configuration. Therefore Bus voltage and
    power measurements are not available.
 */

#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include "INA219.h"
#include "PulseWelder.h"
#include "config.h"

#define I_AVG_SIZE 16                         // Size of Welder Amps data averaging buffer.
#define E_AVG_SIZE 16                         // Size of Welder VDC data averaging buffer.
#define VDC_PIN 36                            // Voltage reading pin.
#define VDC_SCALE ((47000.0 + 1800.0) / 1800) // Resistor Attenuator on Welding VDC signal.
#define VDC_ADC_PORT ADC1_CHANNEL_0
#define DEFAULT_VREF 1100

// External Globals
extern int Amps;
extern unsigned int Volts;
extern INA219 ina219;
extern bool i2cInitComplete;

// Local Scope Vars
static int iAvgBuff[I_AVG_SIZE + 1]; // Amps data averaging buffer.
static int eAvgBuff[E_AVG_SIZE + 1]; // VDC data averaging buffer.
static esp_adc_cal_characteristics_t *adc_chars;

// *********************************************************************************************
// Setup the INA219 Current Sensor.
bool initCurrentSensor(void)
{
  bool success = false;
  uint8_t error;

  ina219.begin(INA219_ADDR);
  Wire.beginTransmission(INA219_ADDR);
  error           = Wire.endTransmission();
  i2cInitComplete = true; // Tell world we have setup the i2c port.

  if (error) {
    Serial.println("INA219 Current Sensor Initialization Failed at Address 0x" + String(INA219_ADDR, HEX) + ".");
    success = false;
  }
  else {
    Serial.print("Initialized INA219 Current Sensor at Address 0x" + String(INA219_ADDR, HEX));
    delay(1);
    ina219.reset();
    #ifdef INA219_AVG_ON
     ina219.configure(BUS_RANGE_16V, PGA_RANGE_160MV, SAMPLE_9BITS, SAMPLE_AVG_32, CONTINUOUS_OP_NO_VDC); // 17mS Acq.
     Serial.println(" (using 32-sample hardware averaging).");
    #else
     ina219.configure(BUS_RANGE_16V, PGA_RANGE_160MV, SAMPLE_9BITS, SAMPLE_12BITS, CONTINUOUS_OP_NO_VDC); // 532uS Acq.
     Serial.println(" (not using hardware averaging).");
    #endif
    ina219.calibrate(SHUNT_OHMS, SHUNT_V_MAX, BUS_V_MAX, MAX_I_EXPECTED);
    success = true;
  }

  return success;
}

// *********************************************************************************************
// Initialize the Volts ADC. We use ADC1_CHANNEL_0, which is Pin 36.
// This MUST be called in setup() before first use of measureVoltage().
void initVdcAdc(void)
{
  // Configure ADC
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(VDC_ADC_PORT, ADC_ATTEN_DB_11);

  // Characterize ADC
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);

  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.println("ADC eFuse provided Factory Stored Vref Calibration.");      // Best Accuracy.
  }
  else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    Serial.println("ADC eFuse provided Factory Stored Two Point Calibration."); // Good Accuracy.
  }
  else {
    Serial.println("ADC eFuse not supported, using Default VRef (1100mV).");    // Low Quality Accuracy.
  }

  /*    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
      printf("ADC eFuse Two Point: Supported\n");
     } else {
      printf("ADC eFuse Two Point: NOT supported\n");
     }

     //Check Vref is burned into eFuse
     if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
       printf("ADC eFuse Vref: Supported\n");
     } else {
       printf("ADC eFuse Vref: NOT supported\n");
     }
   */
}

// *********************************************************************************************
// Measure welder Voltage using data averaging.
// Be sure to call initVdcAdc() in setup();
void measureVoltage(void)
{
  static int avgIndex      = 0; // Index of the current Amps reading
  static uint32_t totalVdc = 0; // Current totalizer for Amps averaging
  uint32_t voltage;
  uint32_t reading;

  reading = adc1_get_raw(VDC_ADC_PORT);
  voltage = esp_adc_cal_raw_to_voltage(reading, adc_chars); // Convert to unscaled mV.

  totalVdc           = totalVdc - eAvgBuff[avgIndex];
  eAvgBuff[avgIndex] = voltage;

  totalVdc  = totalVdc + eAvgBuff[avgIndex];
  avgIndex += 1;
  avgIndex  = avgIndex >= E_AVG_SIZE ? 0 : avgIndex;

  voltage = totalVdc / E_AVG_SIZE;
  Volts   = ((float)(voltage) * VDC_SCALE) / 1000.0f; // Apply Attenuator Scaling, covert from mV to VDC.
  Volts   = constrain(Volts, 0, 99);
  Volts   = Volts < 5 ? 0 : Volts;                    // Zero values under 5V due to ADC behavior at low levels.

  /* // Debug
     static int i = 0;
     if (i++ > 25)
     {
      i = 0;
      Serial.println("total VDC:" + String(totalVdc) + ", Avg Voltage : " + String(voltage) + ", Voltage:" + String(Volts));
     }
   */
}

// *********************************************************************************************
// Measure welder current using data averaging.
void measureCurrent(void)
{
  static int avgIndex          = 0; // Index of the current Amps reading
  static int totalAmps         = 0; // Current totalizer for Amps averaging
  float inaAmps                = 0; // INA219 Amps.
  static unsigned int delayCnt = 0; // Delay counter for INA219 error message.


#ifdef DEMO_MODE
  Amps = 0; // Zero out welding current when in Demo Mode.
  return;
#endif // ifdef DEMO_MODE

  inaAmps = ina219.shuntCurrent(); // Get Current measurement.
  inaAmps = constrain(inaAmps, -220.0, +220.0);
  inaAmps = -inaAmps;              // Invert polarity.

  //    Serial.println("Shunt current:" + String(inaAmps) + "A");
  //    Serial.println("Raw shunt vdc:" + String(ina219.shuntVoltageRaw()));
  //    Serial.println("Shunt vdc:" + String(ina219.shuntVoltage() * 1000) + "mV");

  if ((inaAmps > -3.0) && (inaAmps < 3.0)) { // Noise.
    inaAmps = 0.0;
  }
  else if (inaAmps < 0.0) {                  // Negative amps? The INA219 is missing the shunt resistor or it is wired "backwards."
    inaAmps  = 0.0;
    Amps     = 999;                          // Show "Error" value to alert user.
    delayCnt = 0;

    if (delayCnt++ > 50) {                   // Periodically echo error message.
      delayCnt = 0;
      Serial.println("WARNING: INA219 sensor wiring error!");
    }
    return;
  }

  totalAmps          = totalAmps - iAvgBuff[avgIndex];
  iAvgBuff[avgIndex] = (int)(inaAmps); // Store in buffer.
  totalAmps          = totalAmps + iAvgBuff[avgIndex];
  avgIndex          += 1;
  avgIndex           = avgIndex >= I_AVG_SIZE ? 0 : avgIndex;
  Amps               = totalAmps / I_AVG_SIZE;

  //  Serial.println("totalAmps:" + String(totalAmps) + ", Avg Amps : " + String(Amps));
}

// *********************************************************************************************
// Clear out the Welding Amps Averaging Buffer.
void resetCurrentBuffer(void)
{
  int i;

  for (i = 0; i < I_AVG_SIZE; i++) {
    iAvgBuff[i] = 0; // Zero out the entire Amps averaging buffer.
  }
}

// *********************************************************************************************
// Clear out the Welding Volts Averaging Buffer.
void resetVdcBuffer(void)
{
  int i;

  for (i = 0; i < E_AVG_SIZE; i++) {
    eAvgBuff[i] = 0; // Zero out the entire VDC averaging buffer.
  }
}

ArcState arcState = ARC_UNKNOWN;
int prevArcState = 0;
bool arcStateChanged = false;
unsigned long arcStateChangeTime = 0;
// 1 = Open Circuit, No Arc
// 2 = Short Circuit, No Arc
// 3 = Short Circuit, No Arc, Low Energy
// 4 = Arc?

void detectArcState()
{
  const int ampLimit = 20;
  ArcState newArcState = ARC_UNKNOWN;
  static uint32_t shortTimer = 0;

  if (Volts >= 1 && Amps < ampLimit) {
    newArcState = OPEN_NO_ARC;
  } 
  else if (Volts < 1 && Amps >= ampLimit) {
    if (arcState != SHORT) {
      uint32_t now = millis();
      if (shortTimer < now - 1000) {
        shortTimer = now;
      } 
      else if (shortTimer < now - 250) {
        newArcState = SHORT;
      }
    } 
  }
  else if (Volts < 1 && Amps >= 0 && Amps < ampLimit) {
    newArcState = SHORT_LOW;
  }
  else {
    newArcState = ARC;
  }

  if (newArcState != ARC_UNKNOWN && newArcState != SHORT) {
    shortTimer = 0; // detection of any other valid state resets short timer
  } 

  if (newArcState != arcState && newArcState != ARC_UNKNOWN)
  {
    unsigned long now = millis();
    unsigned long delta = now - arcStateChangeTime;
    arcStateChangeTime = now;
    arcStateChanged = true;
    Serial.println("Arc State Change from " + String(arcState) + " to " + String(newArcState) + " after " + String(delta) + "ms" );
    Serial.println("Volts " + String(Volts) + " Amps " + String(Amps));
    prevArcState = arcState;
    arcState = newArcState;
  }  
}

// EOF
