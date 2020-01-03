/**********************************************
* INA219 library example
* 10 May 2012 by johngineer
*
* this code is public domain.
**********************************************/


#include <Wire.h>
#include <INA219.h>

INA219 monitor;


void setup()
{
  Serial.begin(9600);
  monitor.begin(64);
  monitor.configure(0, 3, 3, 7);
  
  // test shunt = 115mm of 22AWG solid copper = 0.3 Ohms
  monitor.calibrate(0.3, 0.2, 6, 0.25);
  

}

void loop()
{

  Serial.println("******************");
  
  Serial.print("raw shunt voltage: ");
  Serial.println(monitor.shuntVoltageRaw());
  
  Serial.print("raw bus voltage:   ");
  Serial.println(monitor.busVoltageRaw());
  
  Serial.println("--");
  
  Serial.print("shunt voltage: ");
  Serial.print(monitor.shuntVoltage() * 1000, 4);
  Serial.println(" mV");
  
  Serial.print("shunt current: ");
  Serial.print(monitor.shuntCurrent() * 1000, 4);
  Serial.println(" mA");
  
  Serial.print("bus voltage:   ");
  Serial.print(monitor.busVoltage(), 4);
  Serial.println(" V");
  
  Serial.print("bus power:     ");
  Serial.print(monitor.busPower() * 1000, 4);
  Serial.println(" mW");
  
  Serial.println(" ");
  Serial.println(" ");
  
  delay(10000);

}


