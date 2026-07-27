#include <Arduino.h>
#include <Wire.h>
#include <vector>



void setup()
{
  Serial.begin(115200);
  Wire.begin(21,22); 
  Serial.println("Connecting....");

  for(int i = 1; i < 127; i++){
    Wire.beginTransmission(i);
    int check = Wire.endTransmission();
    if(check == 0){
      Serial.print("Connected at:");
      Serial.println(i,HEX);
    }

  }

}

void loop()
{
  
      
}
