/**
 * @file sensor_manager.h
 * @brief Sensor data collection and management
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <zephyr/kernel.h>
#include <stdint.h>

/* Sensor data structure */
typedef struct {
    float temperature_c;      /* Temperature in Celsius */
    float accel_x;            /* Accelerometer X-axis (m/s²) */
    float accel_y;            /* Accelerometer Y-axis (m/s²) */
    float accel_z;            /* Accelerometer Z-axis (m/s²) */
    float battery_voltage;    /* Battery voltage (V) */
    uint32_t timestamp_ms;    /* Timestamp in milliseconds */
    bool valid;               /* Data validity flag */
} sensor_data_t;

/**
 * @brief Initialize sensor manager and all sensors
 * @return 0 on success, negative errno on failure
 */
int sensor_manager_init(void);

/**
 * @brief Start sensor sampling thread
 * @return 0 on success, negative errno on failure
 */
int sensor_manager_start(void);

/**
 * @brief Stop sensor sampling thread
 */
void sensor_manager_stop(void);

/**
 * @brief Get latest sensor readings
 * @param data Pointer to sensor_data_t structure to fill
 * @return 0 on success, negative errno on failure
 */
int sensor_manager_get_data(sensor_data_t *data);

/**
 * @brief Register callback for new sensor data
 * @param callback Function to call when new data is available
 */
typedef void (*sensor_data_callback_t)(const sensor_data_t *data);
void sensor_manager_register_callback(sensor_data_callback_t callback);

#endif /* SENSOR_MANAGER_H */