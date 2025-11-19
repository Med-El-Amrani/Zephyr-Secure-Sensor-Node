cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(secure_sensor_node C)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Application sources
target_sources(app PRIVATE
    src/main.c
    src/sensor_manager.c
    src/ble_service.c
    src/mqtt_client.c
    src/power_manager.c

    subsys/sensors/i2c_temp_sensor.c
    subsys/sensors/spi_accel_sensor.c
    subsys/sensors/adc_battery.c

    subsys/encoding/json_encoder.c
)
