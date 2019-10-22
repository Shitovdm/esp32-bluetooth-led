#include "BluetoothSerial.h"
 
BluetoothSerial ESP_BT; // Объект для Bluetooth
 
int incoming;
 
void setup() {
  Serial.begin(9600);
  ESP_BT.begin("ESP32_LED_Control");
  Serial.println("Bluetooth Device is Ready to Pair");
}
void loop() {
 
  if (ESP_BT.available())
  {
    incoming = ESP_BT.read();
    Serial.print("Received:"); Serial.println(incoming);
 
    if (incoming == 49)
        {
        ESP_BT.println("ESP32 says PING");
        ESP_BT.println(incoming);
        }
 
    if (incoming == 48)
        {
        ESP_BT.println("ESP32 says other PING");
        }     
  }
  delay(20);
}
