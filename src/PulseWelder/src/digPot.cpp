/*
   File: digPot.cpp
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Target Chips: Microchip MCP45HV51 I2C Digital Pot IC. 5K ohms, 8-Bit.
                 Microchip MCP41HV51 SPI Digital Pot IC. 5K ohms, 8-Bit.
   Version: 1.1
   Creation: Sep-11-2019
   Revised: Dec-29-2019.
   Public Release: Jan-03-2020
   Revision History: See PulseWelder.cpp
   Project Leader: T. Black (thomastech)
   Contributors: thomastech, hogthrob

   (c) copyright T. Black 2019-2020, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty is given.
   This Code was formatted with the uncrustify extension.

   Notes: Beginning with V1.1, support has been added for the MCP41HV51 SPI Digital Pot IC.
          Auto Detection, MCP45HV51 (I2C) or MCP41HV51 (SPI).
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "digPot.h"
#include "PulseWelder.h"
#include "config.h"

extern bool i2cInitComplete; // I2C Port Initialization flag.
extern bool spiInitComplete; // SPI Port Initialization flag.

static uint8_t csPin = 0;
static uint8_t chipAddr = 0;

// *********************************************************************************************
static bool initDigitalPotShared(void)
{
  bool success = true;

  const char *msg = "Initialized POT Ohms, Set Minimum Welding Current.";

  digitalPotWrite(POT_TCON_DEF, POT_TCON_ADDR); // Set Pot TCON register.

  if (digitalPotRead(POT_TCON_ADDR) == POT_TCON_DEF) {
    digitalPotWrite(POT_MIN, POT_WIPER_ADDR);   // Set Pot Wiper

    if (digitalPotRead(POT_WIPER_ADDR) != POT_MIN) {
      success = false;
      msg     = "Ohms Initialization Failed";
    }
  }
  else {
    success = false;
    msg     = "TCON Initialization Failed";
  }

  if (chipAddr != 0) {
    Serial.println("Found MCP45HV51 I2C Digital POT at Addr 0x" + String(chipAddr, HEX) + ": " + msg);
  }
  else if (csPin != 0) {
    Serial.println("Found MCP41HV51 SPI Digital POT at csPin " + String(csPin, DEC) + ": " + msg);
  }
  else {
    Serial.println("MCP4xHV51 Digital POT not found! Check hardware.");
  }
  return success;
}

// *********************************************************************************************
// Initialize the MCP41HV51 SPI Digital Pot.
bool initDigitalPotSPI(uint8_t csPinPot)
{
  csPin    = csPinPot;
  chipAddr = 0; 

  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, HIGH);

  if (spiInitComplete == false) { // Check to see SPI port has already been configured.
    spiInitComplete = true;
    SPI.begin();
  }

  return initDigitalPotShared();
}

// *********************************************************************************************
// Initialize the MCP45HV51 I2C Digital Pot.
bool initDigitalPotI2C(byte chipAddrPot)
{
  bool success = false;

  chipAddr = chipAddrPot;

  if (i2cInitComplete == false) { // Check to see if INA219 has already configured I2C port.
    i2cInitComplete = true;
    Wire.begin();
  }

  Wire.beginTransmission(chipAddr);

  if (Wire.endTransmission()) { // Communication Error!
    success = false;
    Serial.println("Digital POT Failure, Missing at Address 0x" + String(chipAddr, HEX) + ".");
  }
  else {
    success = initDigitalPotShared();
  }

  return success;
}

// *********************************************************************************************
// Initialize the MCP41HV51 SPI Digital Pot.
bool initDigitalPot(byte chipAddrPot, int spi_cs)
{
  bool retval = false;

  retval = initDigitalPotI2C(chipAddrPot);

  if ((retval == false) && (spi_cs != -1)) {
    retval = initDigitalPotSPI(spi_cs);
  }

  return retval;
}

// *********************************************************************************************
// Set the Pot wiper ohms for requested Welding Amps.
// ampVal = Desired Welding Amps.
// verbose = VERBOSE_ON (true) for expanded log messages, else VERBOSE_OFF (false) for less messages.
void setPotAmps(byte ampVal, bool verbose)
{
  byte potVal;
  uint16_t ohms;

  ampVal = constrain(ampVal, MIN_AMPS, MAX_SET_AMPS);
  ohms   = map(ampVal, MIN_AMPS, MAX_AMPS, POT_MIN_OHMS, POT_MAX_OHMS); // Calculate Ohms.
  potVal = map(ampVal, MIN_AMPS, MAX_AMPS, 0x00, 0xff);                 // Calculate Dig Pot Wiper Value.
  digitalPotWrite(potVal, POT_WIPER_ADDR);

  if (verbose == VERBOSE_ON) {
    Serial.println("Set Welding Current to " + String(ampVal) + " Amps. Digital Pot is now " + String(ohms) + " Ohms, Data: 0x" +
                   String(potVal, HEX));
  }
}

// *********************************************************************************************
// Primitive Write value for the MCP4xHV51 digital Pot.
bool digitalPotWrite(byte dataValue, byte memAddr)
{
  bool success = false;

  if (chipAddr != 0) {                // Found I2C Digital Pot.
    Wire.beginTransmission(chipAddr); // I2C Addr
    Wire.write(memAddr | POT_WR_CMD); // Write Command
    Wire.write(dataValue);            // Set Pot to Minimum Welding Current.
    success = Wire.endTransmission() == I2C_ERROR_OK; // Result is true or false bool value.
  }
  else if (csPin != 0) {              // Found SPI Digital Pot.
    digitalWrite(csPin, LOW);
    byte resp1 = SPI.transfer(memAddr | POT_WR_CMD);
    SPI.write(dataValue);
    digitalWrite(csPin, HIGH);

    // Bit 1 of resp1 byte must be 1. If 0, either the MCP4xHVx1 signal has issue or not connected.
    success = ((resp1 & 0x02) == 0x02);  // Result is true or false bool value.
  }

  if (success == false) {
    Serial.println("I/O Error While Writing to the MCP4xHV51 Digital Pot. Check Hardware.");
  }

  return success;
}

// *********************************************************************************************
// Primitive Read value for the MCP4xHV51 digital Pot.
byte digitalPotRead(byte memAddr)
{
  byte dataByte{ 0 };
  bool success = false;

  if (chipAddr != 0) {
    Wire.beginTransmission(chipAddr);                     // Addr
    Wire.write(memAddr | POT_RD_CMD);                     // Read Command
    Wire.requestFrom((uint16_t)(chipAddr), (uint8_t)(2)); // Request two bytes from Dig Pot.
    Wire.read();                                          // Toss out first byte, always zero.
    dataByte = Wire.read();                               // Get Data.
    success  = Wire.endTransmission() == I2C_ERROR_OK;    // Result is true or false bool value.
  }
  else if (csPin != 0) {
    digitalWrite(csPin, LOW);
    byte resp1 = SPI.transfer(memAddr | POT_RD_CMD);
    dataByte = SPI.transfer(0);
    digitalWrite(csPin, HIGH);

    success = ((resp1 & 0x02) == 0x02);                 // Result is true or false bool value.
    // Bit 1 of resp1 byte must be 1. If 0, either the MCP4xHVx1 signal has issue or not connected.
  }

  if (success == false) {
    Serial.println("I/O Error While Reading the MCP4xHV51 Digital Pot. Check Hardware.");
    dataByte = 0;
  }

  return dataByte;
}

// *********************************************************************************************
// EOF
