# Secure Sensor Node – Zephyr RTOS (ESP32-S3)

Embedded sensor node built in **C (C99)** on **Zephyr RTOS** for ESP32-S3 DevKitC.
It samples sensors (I²C / SPI / ADC), encodes payloads (JSON/CBOR), and publishes via **BLE GATT** and **MQTT (Wi-Fi)**, with **TLS**, **watchdog**, and **low-power** support.

## Features
- Sensors: I²C temperature, SPI accelerometer, ADC battery voltage (stubs included)
- Comms: BLE GATT (notify), MQTT client (optionally TLS)
- Security: mbedTLS, firmware signing via MCUboot (optional), watchdog
- Power: sleep modes, tickless idle
- Tooling: west + CMake, logs over UART (RTT optional)

## Hardware
- Board: **ESP32-S3 DevKitC**  
- Zephyr target: `esp32s3_devkitc/esp32s3/procpu`