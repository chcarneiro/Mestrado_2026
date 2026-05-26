#include <Arduino.h>
#include "iis3dwb_driver.hpp"
#include "iis3dwb_regs.hpp"

void setup()
{
  Serial.begin(115200);
  delay(3000);

  while (!Serial && millis() < 8000)
  {
    delay(10);
  }

  Serial.println("A");
  Serial.println("BOOT,IIS3DWB,Pico RP2040");
  Serial.println("B");

  if (!sensor.begin())
  {
    Serial.println("C");
    Serial.println("ERROR,WHO_AM_I");
    while (true)
    {
      delay(1000);
    }
  }

  Serial.println("D");
  Serial.print("WHO_AM_I=0x");
  Serial.println(sensor.readRegister(REG_WHO_AM_I), HEX);
  Serial.println("E");
  Serial.println("t_ms,x,y,z");
}

void loop()
{
  int16_t x, y, z;
  sensor.readXYZraw(x, y, z);

  Serial.print(millis());
  Serial.print(',');
  Serial.print(x);
  Serial.print(',');
  Serial.print(y);
  Serial.print(',');
  Serial.println(z);

  delay(200);
}