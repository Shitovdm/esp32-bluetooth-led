/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleWrite.cpp
    Ported to Arduino ESP32 by Evandro Copercini
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "085ac895-8339-4478-a19a-2160e37e5ffa"
#define CHARACTERISTIC_UUID "8173ba12-78f1-4a17-bd6e-ca2869d89372"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected!");
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("Device disconnected!");
      deviceConnected = false;
    }
};

class MyRuntimeCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      Serial.println("Received new message!");
      if (value.length() > 0) {
        pCharacteristic->setValue("122");
        Serial.print("New value: ");
        for (int i = 0; i < value.length(); i++){
           Serial.print(value[i]);
        }
        Serial.println();
      }
    }
};

void setup() {
  Serial.begin(115200);
  BLEDevice::init("MyESP32");
  // Create the BLE Server
  pServer = BLEDevice::createServer();
  
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
  // Create a BLE Descriptor                  
  pCharacteristic->addDescriptor(new BLE2902());
  
  pCharacteristic->setCallbacks(new MyRuntimeCallbacks());
  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
   // notify changed value
    if (deviceConnected) {
        pCharacteristic->setValue("test");
        pCharacteristic->notify();
        //value++;
        //delay(10); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
    
    delay(5000);
}
