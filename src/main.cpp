#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

IPAddress laptopIP(172, 20, 10, 3);

WiFiUDP udp;



void setup()
{
  Serial.begin(115200);
  WiFi.begin("Pete Phone", "Redsox34");
  udp.begin(3434); 

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
    Serial.println(WiFi.status());
  }

  Serial.println("connected!");
  Serial.println(WiFi.localIP());

  
}

void loop()
{
  

  udp.beginPacket(laptopIP, 4269); 
  udp.write((const uint8_t *)"hello", 5);
  udp.endPacket();

  delay(2000);
}

//Check WireShark