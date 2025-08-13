# ESP32 BLE Temperature & Humidity Sensor

This project implements a **BLE peripheral** on the ESP32 that measures **temperature and humidity** from an SHT40 sensor, optionally reads a thermistor via the ADC, and publishes the readings over Bluetooth Low Energy. 2 compact PCB's were designed to accomodate specific use cases for integration in a sensor-enabled mask.

![Image](https://github.com/user-attachments/assets/af4b6160-2fd7-4741-b419-8729478cb086)

![Image](https://github.com/user-attachments/assets/d3f3787d-2d80-4c20-a191-4d5a04f1b978)

It also supports **remote commands** to enable or disable the sensor via a write-only BLE characteristic.

## Supported Targets

| Supported Targets | ESP32 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- |
| Status            | Support Available    | Support Available*      | Support Available*      |

> \*Requires compatible I²C and ADC pins — verify pin mapping for your board.
---
## Features
- Reads temperature and humidity from an **SHT40** over I²C.
- Optional thermistor measurement on ADC pin 34.
- Publishes `"temperature:humidity"` (ASCII) via a BLE notify characteristic.
- Accepts commands via a BLE write characteristic:
  - `0x01` — disable sensor readings.
  - `0x02` — re-enable sensor readings.
- CRC-8 verification on SHT40 data for robust measurements.
- Automatically restarts advertising when a BLE client disconnects.
- LED indicator on IO25 lights when temperature exceeds a configurable threshold.
---
## Hardware Requirements

- ESP32 development board (e.g., DevKitC, WROOM-based board).
- SHT40 temperature & humidity sensor (I²C, address `0x44`).
- Optional: 10 kΩ NTC thermistor + 10 kΩ fixed resistor (voltage divider for ADC input).
- LED + resistor on IO25 (optional, can use onboard LED if available).

**Default pin mapping:**
| Function      | Pin |
| ------------- | --- |
| I²C SDA       | 21  |
| I²C SCL       | 22  |
| Thermistor ADC| 34  |
| LED output    | 25  |
---
## BLE Services & Characteristics

- **Service UUID:** `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
  - **Temperature/Humidity Characteristic**  
    - UUID: `beb5483e-36e1-4688-b7f5-ea07361b26a8`  
    - Properties: `READ`, `NOTIFY`  
    - Format: ASCII string `"T:RH"` with two decimal places (e.g., `23.56:45.78`).
  - **Command Characteristic**  
    - UUID: `12345678-1234-1234-1234-123456789012`  
    - Properties: `WRITE`  
    - Commands:  
      - `0x01` — disable sensor.  
      - `0x02` — enable sensor.
