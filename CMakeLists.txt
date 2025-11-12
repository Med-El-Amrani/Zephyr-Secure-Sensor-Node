cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(secure_sensor_node C)

# Include directories
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Main application sources
target_sources(app PRIVATE
    src/main.c
    src/sensor_manager.c
    src/ble_service.c
    src/mqtt_client.c
    src/power_manager.c
)

# Add sensor subsystem
add_subdirectory(subsys/sensors)

# Add encoding subsystem
add_subdirectory(subsys/encoding)

# Link subsystem libraries
target_link_libraries(app PRIVATE
    subsys__sensors
    subsys__encoding
)