/*
   File: digPot.cpp
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Target Chip: Microchip MCP41HV51 Digital Pot IC. 5K ohms, 8-Bit.
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
#include "PulseWelder.h"
#include "digPot.h"

extern bool i2cInitComplete; // I2C Port Initialization is Complete flag.

// *********************************************************************************************
// Initialize the MCP41HV51 Digital Pot.
bool initDigitalPot(byte chipAddr)
{
  bool success = false;

  if (i2cInitComplete == false) // Check to see if INA219 has already configured I2C port.
  {
    i2cInitComplete = true;
    Wire.begin();
  }

  Wire.beginTransmission(chipAddr);

  if (Wire.endTransmission()) { // Communication Error!
    success = false;
    Serial.println("Digital POT Failure, Missing at Address 0x" + String(chipAddr, HEX) + ".");
  }
  else {
    success = true;

    digitalPotWrite(POT_TCON_DEF, POT_TCON_ADDR, chipAddr); // Set Pot TCON register.

    if (digitalPotRead(POT_TCON_ADDR, chipAddr) == POT_TCON_DEF) {
      digitalPotWrite(POT_MIN, POT_WIPER_ADDR, chipAddr);   // Set Pot Wiper

      if (digitalPotRead(POT_WIPER_ADDR, chipAddr) == POT_MIN) {
        Serial.println("Initialized Digital POT Ohms at Address 0x" + String(chipAddr, HEX) + ". Set Minimum Welding Current.");
      }
      else {
        success = false;
        Serial.println("Digital POT Ohms Initialization Failed at Address 0x" + String(chipAddr, HEX) + ".");
      }
    }
    else {
      success = false;
      Serial.println("Digital POT TCON Initialization Failed at Address 0x" + String(chipAddr, HEX) + ".");
    }
  }

  return success;
}

// *********************************************************************************************
// Set the Pot wiper ohms for requested Welding Amps.
// verbose = true for serial log status message.
void setPotAmps(byte ampVal, byte chipAddr, bool verbose)
{
  byte potVal;
  uint16_t ohms;

  ampVal = constrain(ampVal, MIN_SET_AMPS, MAX_SET_AMPS);
  ohms   = map(ampVal, MIN_AMPS, MAX_AMPS, POT_MIN_OHMS, POT_MAX_OHMS); // Calculate Ohms.
  potVal = map(ampVal, MIN_AMPS, MAX_AMPS, 0x00, 0xff);                 // Calculate Dig Pot Wiper Value.
  digitalPotWrite(potVal, POT_WIPER_ADDR, chipAddr);

  if (verbose) {
    Serial.println("Set Welding Current to " + String(ampVal) + " Amps. Digital Pot is now " + String(ohms) + " Ohms, Data: 0x" +
                   String(potVal, HEX));
  }
}

// *********************************************************************************************
// Primitive Write value for the MCP45HV51 digital Pot.
bool digitalPotWrite(byte dataValue, byte memAddr, byte chipAddr)
{
  Wire.beginTransmission(chipAddr); // I2C Addr
  Wire.write(memAddr | POT_WR_CMD); // Write Command
  Wire.write(dataValue);            // Set Pot to Minimum Welding Current.
  Wire.endTransmission();
  return true;
}

// *********************************************************************************************
// Primitive Read value for the MCP45HV51 digital Pot.
byte digitalPotRead(byte memAddr, byte chipAddr)
{
  byte dataByte;

  Wire.beginTransmission(chipAddr);                     // Addr
  Wire.write(memAddr | POT_RD_CMD);                     // Read Command
  Wire.requestFrom((uint16_t)(chipAddr), (uint8_t)(2)); // Request two bytes from Dig Pot.
  Wire.read();                                          // Toss out first byte, always zero.
  dataByte = Wire.read();                               // Get Data.
  Wire.endTransmission();

  return dataByte;
}

// *********************************************************************************************
// EOF
