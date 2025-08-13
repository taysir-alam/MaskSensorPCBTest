#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h> 
#include <Wire.h>
#include <math.h>

#define SHT40_ADDRESS 0x44
#define ADC_PIN 34 // A2/IO34 as ADC input
#define LED_PIN 25 // IO25

BLEServer *pServer = nullptr;
BLECharacteristic *pTempHumidityCharacteristic = nullptr; // temperature and humidity info
BLECharacteristic *pCommandCharacteristic = nullptr; // For receiving commands
bool deviceConnected = false;
bool sensorActive = true; // Sensor is active -default

// BLE service and characteristic UUIDs
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define TEMP_HUMIDITY_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define COMMAND_CHARACTERISTIC_UUID "12345678-1234-1234-1234-123456789012" // UUID for command characteristic

// CRC-8 calculation for SHT40 data (poly 0x31, init 0xFF)
uint8_t sht_crc(uint8_t a, uint8_t b) {
  uint8_t crc = 0xFF;
  auto step = [&](uint8_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  };
  step(a); step(b);
  return crc;
}

// Server connection callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      pServer->getAdvertising()->start(); // Restart advertising on disconnect
    }
};

// Command characteristic callbacks
class CommandCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        if (value[0] == 0x01) {
          Serial.println("Shut off sensor");
          sensorActive = false;
        } else if (value[0] == 0x02) {
          Serial.println("Reactivate sensor");
          sensorActive = true;
        } else {
          Serial.printf("Unknown command: 0x%02X\n", value[0]);
        }
      }
    }
};

// Calculate humidity from SHT40 data (CRC checked)
bool readSHT40(float &temperature, float &humidity) {
  Wire.beginTransmission(SHT40_ADDRESS);
  Wire.write(0xFD); // Start measurement
  if (Wire.endTransmission() != 0) return false;
  delay(12); // Wait for measurement (~9–12 ms for high precision)
  if (Wire.requestFrom(SHT40_ADDRESS, (uint8_t)6) != 6) return false;

  uint8_t data[6];
  for (int i = 0; i < 6; i++) {
    data[i] = Wire.read();
  }

  if (sht_crc(data[0], data[1]) != data[2]) return false;
  if (sht_crc(data[3], data[4]) != data[5]) return false;

  uint16_t tRaw = (data[0] << 8) | data[1];
  uint16_t hRaw = (data[3] << 8) | data[4];

  temperature = -45.0 + 175.0 * ((float)tRaw / 65535.0);
  humidity    =  -6.0 + 125.0 * ((float)hRaw / 65535.0);

  if (humidity < 0) humidity = 0;
  if (humidity > 100) humidity = 100;

  return true;
}

void setup() {
  Serial.begin(9600);
  Wire.begin(21, 22); // SDA, SCL
  BLEDevice::init("Morphace Mask");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(BLE_SERVICE_UUID);
  
  pTempHumidityCharacteristic = pService->createCharacteristic(
                            TEMP_HUMIDITY_CHARACTERISTIC_UUID,
                            BLECharacteristic::PROPERTY_READ |
                            BLECharacteristic::PROPERTY_NOTIFY);
  pTempHumidityCharacteristic->addDescriptor(new BLE2902()); // Enable notifications
  
  // Create a new characteristic for receiving commands
  pCommandCharacteristic = pService->createCharacteristic(
                             COMMAND_CHARACTERISTIC_UUID,
                             BLECharacteristic::PROPERTY_WRITE);

  // Assign the callback to the command characteristic
  pCommandCharacteristic->setCallbacks(new CommandCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  BLEDevice::startAdvertising();
  Serial.println("Waiting for a client connection to notify...");
  
  analogReadResolution(12); // Set ADC resolution to 12 bits (0-4095)
  analogSetPinAttenuation(ADC_PIN, ADC_11db); // Set attenuation for a full scale range of 3.3V

  pinMode(LED_PIN, OUTPUT);
}

// Non-blocking timing
uint32_t lastRead = 0;

void loop() {
  if (millis() - lastRead >= 1000) { // Delay to pace the loop
    lastRead = millis();

    if (deviceConnected && sensorActive) {
      float humidity = 0, temperature = 0;
      if (!readSHT40(temperature, humidity)) {
        Serial.println("Failed to read from SHT40 sensor.");
        return;
      }

      int adcValue = 2000; //temporary until I get thermistor
      //int adcValue = analogRead(ADC_PIN);
      float voltage = adcValue * (3.3 / 4095.0);
      float Rt = 10000 * (3.3 / voltage - 1);
      float tempTherm = -29.13 * log(Rt) + 292.54;

      if (temperature >= 29.00) {
        digitalWrite(LED_PIN, HIGH); 
      } else {
        digitalWrite(LED_PIN, LOW); 
      }
    
      // Format the temperature and humidity data
      char tempHumidityStr[20];
      snprintf(tempHumidityStr, sizeof(tempHumidityStr), "%.2f:%.2f", temperature, humidity);

      // Set the BLE characteristic's value and notify the client
      pTempHumidityCharacteristic->setValue((uint8_t*)tempHumidityStr, strlen(tempHumidityStr));
      pTempHumidityCharacteristic->notify();
    
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" °C, Humidity: ");
      Serial.println(humidity);
    }
    else if (!sensorActive) {
      // If sensor is deactivated based on the command
      digitalWrite(LED_PIN, LOW);
      Serial.println("Sensor is shut off.");
    }
  }
}
