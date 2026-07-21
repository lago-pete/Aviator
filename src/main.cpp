#include <Arduino.h>
#include <Wire.h>
#include <vector>

void setup() {
  Serial.begin(115200); // This is setting up the channel between the Chips using UART, the number is the BAUD rate
  Wire.begin(21,22); // This is the pin location on the ESP32 (pin 21,22) that corrispond with the SDA and SCL on the MPU Chip
  Serial.print("Connecting... :)\n\n");

}

void loop() {
  int count = 0;
  for(int i = 1; i < 127; i++){  // I2c Protocol address space is 7 bits (2^7 = 128). This means that the range for addresses is 1->128 or 0->127. However 0 and 127 are reservered by I2C. 0: General call/Broadcast address. 127: reserved.
    Wire.beginTransmission(i);
    int check = Wire.endTransmission();
    if(check == 0){
      if(count == 0)
      {
        Serial.println("Here are the found devices:");
      }
      Serial.print("0x");
      if(i < 16){  // this is because single variable 1-9 and a-f so thats the first 16 digits. 
        Serial.print("0");
      }
      Serial.println(i, HEX);
      count++;
    }
  }
  if(count == 0){
    Serial.print("No Devices were found");
  }

  delay(5000);
}
