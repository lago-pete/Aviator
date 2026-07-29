#include <Arduino.h>
#include <Wire.h>
#include <vector>

struct MpuData
{
  int16_t accelX;
  int16_t accelY;
  int16_t accelZ;
  int16_t temp;
  int16_t gyroX;
  int16_t gyroY;
  int16_t gyroZ;
} mpuData;

const float accelConst = 16384;
const float gyroConst = 131;
const float tempConst = 340;

int16_t combine(uint8_t h, uint8_t l);

float toUnits(int16_t x, float constant);

void setup()
{
  Serial.begin(115200); // This is setting up the channel between the Chips using UART, the number is the BAUD rate
  Wire.begin(21, 22);   // This is the pin location on the ESP32 (pin 21,22) that corrispond with the SDA and SCL on the MPU Chip
  Serial.print("Connecting... :)\n\n");

  Wire.beginTransmission(0x68);
  Wire.write(0x6b);
  Wire.write(0x01);
  int check = Wire.endTransmission();

  if (check != 0)
  {
    Serial.print("Error Recieving transmission!");
  }
  else
  {
    Serial.print("Success!");
  }
}

void loop()
{

  Wire.beginTransmission(0x68);
  Wire.write(0x3b);
  Wire.endTransmission(false);

  int check = Wire.requestFrom(0x68, 14);

  // this needs to be change to accomidate for a struct

  if (check != 14)
  {
    Serial.println("Error Recieving transmission!");
    Serial.print("Actually only recieved : ");
    Serial.print(check);
    Serial.println("/14");
    delay(2000);
    return;
  }
  else
  {
    Serial.print("Success!");
  }

  for (int i = 0; i < 7; i++)
  {
    Serial.println();
    Serial.println();
    Serial.println();    

    uint8_t highByte = Wire.read();
    uint8_t lowByte = Wire.read();

    switch (i)
    {
    case 0:
    {
      mpuData.accelX = combine(highByte, lowByte);
      float gForceX = toUnits(mpuData.accelX, accelConst);
      Serial.print("Acceleration on X: ");
      Serial.println(gForceX);
      break;
    }
    case 1:
    {
      mpuData.accelY = combine(highByte, lowByte);
      float gForceY = toUnits(mpuData.accelY, accelConst);
      Serial.print("Acceleration on Y: ");
      Serial.println(gForceY);
      break;
    }
    case 2:
    {
      mpuData.accelZ = combine(highByte, lowByte);
      float gForceZ = toUnits(mpuData.accelZ, accelConst);
      Serial.print("Acceleration on Z: ");
      Serial.println(gForceZ);
      break;
    }
    case 3:
    {
      mpuData.temp = combine(highByte, lowByte);
      float tempC = toUnits(mpuData.temp, tempConst) + 36.53; // MPU6050 datasheet offset, not part of toUnits
      Serial.print("Temperature: ");
      Serial.println(tempC);
      break;
    }
    case 4:
    {
      mpuData.gyroX = combine(highByte, lowByte);
      float degPerSecX = toUnits(mpuData.gyroX, gyroConst);
      Serial.print("Gyro on X: ");
      Serial.println(degPerSecX);
      break;
    }
    case 5:
    {
      mpuData.gyroY = combine(highByte, lowByte);
      float degPerSecY = toUnits(mpuData.gyroY, gyroConst);
      Serial.print("Gyro on Y: ");
      Serial.println(degPerSecY);
      break;
    }
    case 6:
    {
      mpuData.gyroZ = combine(highByte, lowByte);
      float degPerSecZ = toUnits(mpuData.gyroZ, gyroConst);
      Serial.print("Gyro on Z: ");
      Serial.println(degPerSecZ);
      break;
    }
    }
  }

  
  delay(2000);
}


float toUnits(int16_t x, float constant){
  return x / constant;
}


int16_t combine(uint8_t h, uint8_t l){
  uint16_t temp = (uint16_t)(h << 8) | l;
  return ((int16_t) temp);
}

