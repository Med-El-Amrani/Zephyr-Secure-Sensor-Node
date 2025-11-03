# Sensor subsystem CMakeLists

zephyr_library()

zephyr_library_sources(
    i2c_temp_sensor.c
    spi_accel_sensor.c
    adc_battery.c
)

zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)