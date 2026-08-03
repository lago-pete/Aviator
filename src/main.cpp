#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>



void setup(){
  Serial.begin(115200);
  WiFi.begin("Pete's", "Redsox34");

  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(1000);
    Serial.println(WiFi.status());
  }
        

  Serial.println("connected!");
  Serial.println(WiFi.localIP());
}

void loop()
{
  
}