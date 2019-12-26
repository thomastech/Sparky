/*
   File: digPot.cpp
   Project: ZX7-200 MMA Stick Welder Controller with Pulse Mode.
   Target Chip: Microchip MCP4xHV51 Digital Pot IC. 5K ohms, 8-Bit. MCP41 = SPI, MCP45 = I2C
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
#include <SPI.h>

#include "PulseWelder.h"
#include "digPot.h"

extern bool i2cInitComplete; // I2C Port Initialization is Complete flag.
bool spiInitComplete; // SPI Port Initialization is Complete flag.

static uint8_t csPin = 0;
static uint8_t chipAddr; 

static bool initDigitalPotShared()
{
  bool success = true;

  const char* msg = "Initialized Digital POT Ohms. Set Minimum Welding Current.";
  digitalPotWrite(POT_TCON_DEF, POT_TCON_ADDR); // Set Pot TCON register.

    if (digitalPotRead(POT_TCON_ADDR) == POT_TCON_DEF) {
      digitalPotWrite(POT_MIN, POT_WIPER_ADDR);   // Set Pot Wiper

      if (digitalPotRead(POT_WIPER_ADDR) != POT_MIN) {
        success = false;
        msg = "Ohms Initialization Failed";
      }
    }
    else {
      success = false;
      msg = "TCON Initialization Failed";
    }

    if (chipAddr != 0) { 
      Serial.println("Digital POT at I2C Address 0x" + String(chipAddr, HEX) + ": " + msg);
    }
    if (csPin != 0) { 
      Serial.println("Digital POT at SPI csPin " + String(csPin, DEC) + ": " + msg);
    }
    return success;
}

// *********************************************************************************************
// Initialize the MCP41HV51 Digital Pot.
bool initDigitalPotSPI(uint8_t csPinPot)
{
  csPin = csPinPot;
  chipAddr = 0;

  pinMode(csPin,OUTPUT);
  digitalWrite(csPin, HIGH);
  
  if (spiInitComplete == false) // Check to see if other functions has already configured I2C port.
  {
    spiInitComplete = true;
    SPI.begin();
  }

  return initDigitalPotShared();
}

// *********************************************************************************************
// Initialize the MCP45HV51 Digital Pot.
bool initDigitalPotI2C(byte chipAddrPot)
{
  chipAddr = chipAddrPot;
  csPin = 0;

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
    success = initDigitalPotShared();
  }

  return success;
}

bool initDigitalPot(byte chipAddrPot, int spi_cs)
{
  bool retval = initDigitalPotI2C(chipAddrPot);
  if (retval == false && spi_cs != -1)
  {
    retval = initDigitalPotSPI(spi_cs);
  }

  return retval;
}
// *********************************************************************************************
// Set the Pot wiper ohms for requested Welding Amps.
// verbose = true for serial log status message.
void setPotAmps(byte ampVal, bool verbose)
{
  byte potVal;
  uint16_t ohms;

  ampVal = constrain(ampVal, MIN_SET_AMPS, MAX_SET_AMPS);
  ohms   = map(ampVal, MIN_AMPS, MAX_AMPS, POT_MIN_OHMS, POT_MAX_OHMS); // Calculate Ohms.
  potVal = map(ampVal, MIN_AMPS, MAX_AMPS, 0x00, 0xff);                 // Calculate Dig Pot Wiper Value.
  digitalPotWrite(potVal, POT_WIPER_ADDR);

  if (verbose) {
    Serial.println("Set Welding Current to " + String(ampVal) + " Amps. Digital Pot is now " + String(ohms) + " Ohms, Data: 0x" +
                   String(potVal, HEX));
  }
}

// *********************************************************************************************
// Primitive Write value for the MCP4xHV51 digital Pot.
bool digitalPotWrite(byte dataValue, byte memAddr)
{
  bool success = false;
  if (chipAddr != 0)
  {
    Wire.beginTransmission(chipAddr); // I2C Addr
    Wire.write(memAddr | POT_WR_CMD); // Write Command
    Wire.write(dataValue);            // Set Pot to Minimum Welding Current.
    success = Wire.endTransmission() == I2C_ERROR_OK;
  }

  if (csPin != 0)
  {
    digitalWrite(csPin, LOW);
    byte resp1 = SPI.transfer(memAddr | POT_WR_CMD);
    SPI.write(dataValue);
    digitalWrite(csPin, HIGH);

    success = ((resp1 & 0x02) == 0x02);
    // we have an issue here. Bit 1 of first response byte must be 1
    // with 0 either the MCP41HVx1 signals a in isse or it is not connected
  }

  return success;
}

// *********************************************************************************************
// Primitive Read value for the MCP4xHV51 digital Pot.
byte digitalPotRead(byte memAddr)
{
  byte dataByte{0};
  bool success = false;

  if (chipAddr != 0)
  {
    Wire.beginTransmission(chipAddr);                     // Addr
    Wire.write(memAddr | POT_RD_CMD);                     // Read Command
    Wire.requestFrom((uint16_t)(chipAddr), (uint8_t)(2)); // Request two bytes from Dig Pot.
    Wire.read();                                          // Toss out first byte, always zero.
    dataByte = Wire.read();                               // Get Data.
    success = Wire.endTransmission() == I2C_ERROR_OK;
  }

  if (csPin != 0)
  {
    digitalWrite(csPin, LOW);
    byte resp1 = SPI.transfer(memAddr | POT_RD_CMD);
    dataByte = SPI.transfer(0);
    digitalWrite(csPin, HIGH);
    
    success = ((resp1 & 0x02) == 0x02 );
    // we have an issue here. Bit 1 of first response byte must be 1
    // with 0 either the MCP4xHVx1 signals a in isse or it is not connected
    
  }

  if (success == false)
  {
      // TODO: implement a better error reporting
      dataByte = 0;
  }
  return dataByte;
}

// *********************************************************************************************
// EOF
