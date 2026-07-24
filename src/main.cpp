#include <Arduino.h>
#include <Wire.h>
#include <vector>



void setup()
{
  Serial.begin(115200);
  Serial2.begin(38400,SERIAL_8N1, 16,17); // Testing this baud first because datasheets have been all over the place.
  Serial.println("Connecting....");
  
}

void loop()
{
  if(Serial2.available() > 0){
    int ans = Serial2.read();
    Serial.write(ans);
  }


}


