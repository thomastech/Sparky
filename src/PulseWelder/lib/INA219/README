# TI INA219 hi-side i2c current monitor Library
http://www.ti.com/product/ina219  
06 May 2012 by John De Cristofaro  
19 Sep 2019 Modified by T. Black  
  
### Important Notes:
This library has been modified for use with the Sparky Welder Project.  
Tested at standard i2c 100kbps signaling rate.  
This library does not handle triggered conversion modes. It uses the INA219
in continuous conversion mode. All reads are from continous conversions.  
  
A note about the gain (PGA) setting:  
	The gain of the ADC pre-amplifier is programmable in the INA219, and can
	be set between 1/8x (default) and unity. This allows a shunt voltage
	range of +/-320mV to +/-40mV respectively. Something to keep in mind,
	however, is that this change in gain DOES NOT affect the resolution
	of the ADC, which is fixed at 1uV. What it does do is increase noise
	immunity by exploiting the integrative nature of the delta-sigma ADC.
	For the best possible reading, you should set the gain to the range
	of voltages that you expect to see in your particular circuit. See
	page 15 in the datasheet for more info about the PGA.  
  
### Known bugs:
    May return unreliable values if not connected to a bus or at
	bus currents below 10uA.  

### Dependencies:
    Arduino Wire library

### MIT license
