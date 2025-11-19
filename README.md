# Secure Sensor Node – Zephyr RTOS (ESP32-S3)

A production-ready embedded sensor node built with **Zephyr RTOS** for **ESP32-S3 DevKitC**. It samples multiple sensors (I²C temperature, SPI accelerometer, ADC battery), encodes data to JSON, and transmits via **BLE GATT** and **MQTT over WiFi** with **TLS support**.

## 🎯 Features

### Sensors
- **I²C Temperature Sensor** - Generic I²C temperature sensor driver (adaptable to TMP117, BME280, SHT3x, etc.)
- **SPI Accelerometer** - Generic SPI accelerometer driver (adaptable to ADXL345, LIS3DH, etc.)
- **ADC Battery Monitor** - Battery voltage monitoring via ADC with voltage divider support

### Communication
- **BLE GATT Service** - Custom GATT service with sensor data characteristic
  - Supports notifications to connected clients
  - JSON-formatted sensor data
  - Auto-reconnect support
- **MQTT Client** - MQTT 3.1.1 over WiFi with optional TLS
  - Configurable broker, topics, and QoS
  - JSON payload publishing
  - Connection management

### Security & Reliability
- **mbedTLS** - TLS/SSL for secure MQTT communication
- **Watchdog Timer** - System reset on hang/freeze
- **Firmware Signing** - MCUboot support for secure boot (optional)

### Power Management
- **Low-Power Modes** - Idle, sleep, and deep sleep states
- **Tickless Idle** - CPU power saving when idle
- **Configurable Intervals** - Optimize power vs. responsiveness

## 📁 Project Structure

```
secure_sensor_node/
├── src/                        # Main application sources
│   ├── main.c                  # Application entry point & orchestration
│   ├── sensor_manager.c        # Sensor coordination & data collection
│   ├── ble_service.c           # BLE GATT service implementation
│   ├── mqtt_client.c           # MQTT client with WiFi
│   └── power_manager.c         # Power management & watchdog
├── subsys/                     # Subsystems
│   ├── sensors/                # Sensor drivers
│   │   ├── i2c_temp_sensor.c   # I²C temperature sensor
│   │   ├── spi_accel_sensor.c  # SPI accelerometer
│   │   └── adc_battery.c       # ADC battery monitor
│   └── encoding/               # Data encoding
│       └── json_encoder.c      # JSON serialization
├── include/                    # Header files
├── boards/                     # Board-specific configurations
│   └── esp32s3_devkitc.overlay # Device tree overlay
├── scripts/                    # Build & utility scripts
│   ├── build.sh                # Build automation
│   └── flash.sh                # Flashing script
├── conf/                       # Configuration fragments
├── CMakeLists.txt              # Build configuration
├── prj.conf                    # Project configuration
└── README.md                   # This file
```

## 🚀 Quick Start

### Prerequisites

1. **Zephyr SDK** (tested with v0.16+)
2. **ESP-IDF** (for ESP32 support)
3. **West** (Zephyr build tool)
4. **Python 3.8+** with pyserial

### Installation

1. **Setup Zephyr environment:**
   ```bash
   # Install Zephyr (if not already installed)
   pip install west
   west init ~/zephyrproject
   cd ~/zephyrproject
   west update
   west zephyr-export
   pip install -r ~/zephyrproject/zephyr/scripts/requirements.txt
   ```

2. **Clone this project:**
   ```bash
   cd ~/zephyrproject
   git clone <your-repo-url> secure_sensor_node
   cd secure_sensor_node
   ```

3. **Configure WiFi credentials:**
   Edit `prj.conf` and set:
   ```
   CONFIG_WIFI_SSID="YourSSID"
   CONFIG_WIFI_PASSWORD="YourPassword"
   ```

4. **Configure MQTT broker:**
   Edit `include/app_config.h`:
   ```c
   #define MQTT_BROKER_ADDR "mqtt.example.com"
   #define MQTT_BROKER_PORT 8883
   ```

### Build

```bash
# Make scripts executable
chmod +x scripts/*.sh

# Build the project
./scripts/build.sh

# Or build with pristine (clean) build
./scripts/build.sh --pristine
```

### Flash

```bash
# Flash to ESP32-S3 (auto-detect port)
./scripts/flash.sh

# Or specify port
./scripts/flash.sh --port /dev/ttyUSB0

# Monitor serial output
west espressif monitor
```

