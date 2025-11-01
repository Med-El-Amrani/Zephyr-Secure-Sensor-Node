/**
 * @file mqtt_client.h
 * @brief MQTT client with TLS support for sensor data publishing
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <zephyr/kernel.h>
#include "sensor_manager.h"

/**
 * @brief Initialize MQTT client
 * @return 0 on success, negative errno on failure
 */
int mqtt_client_init(void);

/**
 * @brief Connect to MQTT broker
 * @return 0 on success, negative errno on failure
 */
int mqtt_client_connect(void);

/**
 * @brief Disconnect from MQTT broker
 */
void mqtt_client_disconnect(void);

/**
 * @brief Publish sensor data to MQTT topic
 * @param data Sensor data to publish
 * @return 0 on success, negative errno on failure
 */
int mqtt_client_publish_sensor_data(const sensor_data_t *data);

/**
 * @brief Check if MQTT client is connected
 * @return true if connected, false otherwise
 */
bool mqtt_client_is_connected(void);

/**
 * @brief Process MQTT events (should be called periodically)
 * @return 0 on success, negative errno on failure
 */
int mqtt_client_process(void);

#endif /* MQTT_CLIENT_H */