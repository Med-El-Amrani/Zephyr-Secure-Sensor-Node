/**
 * @file ble_service.h
 * @brief BLE GATT service for sensor data notifications
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <zephyr/kernel.h>
#include "sensor_manager.h"

/* Custom BLE Service UUID: 12345678-1234-5678-1234-56789abcdef0 */
#define BLE_UUID_SENSOR_SERVICE \
    BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0))

/* Sensor Data Characteristic UUID: 12345678-1234-5678-1234-56789abcdef1 */
#define BLE_UUID_SENSOR_DATA_CHAR \
    BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1))

/**
 * @brief Initialize BLE stack and GATT service
 * @return 0 on success, negative errno on failure
 */
int ble_service_init(void);

/**
 * @brief Start BLE advertising
 * @return 0 on success, negative errno on failure
 */
int ble_service_start_advertising(void);

/**
 * @brief Stop BLE advertising
 * @return 0 on success, negative errno on failure
 */
int ble_service_stop_advertising(void);

/**
 * @brief Send sensor data notification to connected clients
 * @param data Sensor data to send
 * @return 0 on success, negative errno on failure
 */
int ble_service_notify(const sensor_data_t *data);

/**
 * @brief Check if BLE client is connected
 * @return true if connected, false otherwise
 */
bool ble_service_is_connected(void);

#endif /* BLE_SERVICE_H */