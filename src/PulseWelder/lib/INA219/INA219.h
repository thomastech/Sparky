/******************************************************************************
* TI INA219 hi-side i2c current/power monitor Library
*
* http://www.ti.com/product/ina219
*
* 6 May 2012 by John De Cristofaro
*
*
* Tested at standard i2c 100kbps signaling rate.
*
* This library does not handle triggered conversion modes. It uses the INA219
* in continuous conversion mode. All reads are from continous conversions.
*
* A note about the gain (PGA) setting:
*	The gain of the ADC pre-amplifier is programmable in the INA219, and can
*	be set between 1/8x (default) and unity. This allows a shunt voltage
*	range of +/-320mV to +/-40mV respectively. Something to keep in mind,
*	however, is that this change in gain DOES NOT affect the resolution
*	of the ADC, which is fixed at 1uV. What it does do is increase noise
*	immunity by exploiting the integrative nature of the delta-sigma ADC.
*	For the best possible reading, you should set the gain to the range
*	of voltages that you expect to see in your particular circuit. See
*	page 15 in the datasheet for more info about the PGA.
*
* Known bugs:
*     * may return unreliable values if not connected to a bus or at
*	bus currents below 10uA.
*
* Arduino 1.0 compatible as of 6/6/2012
*
* Dependencies:
*    * Arduino Wire library
*
* MIT license
******************************************************************************/

#ifndef ina219_h
#define ina219_h


#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

#include <Wire.h>

#define INA219_DEBUG 0

// INA219 memory registers
#define CONFIG_R		0x00	// configuration register
#define V_SHUNT_R		0x01	// differential shunt voltage
#define V_BUS_R			0x02	// bus voltage (wrt to system/chip GND)
#define P_BUS_R			0x03	// system power draw (= V_BUS * I_SHUNT)
#define I_SHUNT_R		0x04	// shunt current
#define CAL_R			0x05	// calibration register

#define INA_RESET		0xFFFF	// send to CONFIG_R to reset unit

#define CONFIG_DEFAULT		0x399F

// config. register bit labels
#define RST	15
#define BRNG	13
#define PG1	12
#define PG0	11
#define BADC4	10
#define BADC3	9
#define BADC2	8
#define BADC1	7
#define SADC4	6
#define SADC3	5
#define SADC2	4
#define SADC1	3
#define MODE3	2
#define MODE2	1
#define MODE1	0

// default values
#define D_I2C_ADDRESS	0x40 // (64)
#define D_RANGE			1
#define D_GAIN			3
#define D_SHUNT_ADC		3
#define D_BUS_ADC		3
#define D_MODE			7
#define D_SHUNT			0.25
#define D_V_BUS_MAX		6
#define D_V_SHUNT_MAX		0.3
#define D_I_MAX_EXPECTED	1


class INA219
{
  public:
	INA219();

	// by default uses addr = 0x40 (both a-pins tied low)
	void begin(uint8_t addr = D_I2C_ADDRESS);

	void calibrate(float r_shunt = D_SHUNT, float v_shunt_max = D_V_SHUNT_MAX, float v_bus_max = D_V_BUS_MAX, float i_max_expected = D_I_MAX_EXPECTED);

	void configure(uint8_t range = D_RANGE, uint8_t gain = D_GAIN, uint8_t bus_adc = D_BUS_ADC, uint8_t shunt_adc = D_SHUNT_ADC, uint8_t mode = D_MODE);

	void reset();

	int16_t shuntVoltageRaw();
	int16_t busVoltageRaw();
	float shuntVoltage();
	float busVoltage();
	float shuntCurrent();
	float busPower();


  private:
	uint8_t i2c_address;
	float r_shunt, current_lsb, power_lsb;
	uint16_t config, cal, gain;

	int16_t read16(uint8_t addr);
	void write16(uint8_t addr, uint16_t data);

	int last_addr;

};

#endif