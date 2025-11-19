/**
 * @file app_config.h
 * @brief Application-wide configuration constants
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <zephyr/kernel.h>

/* Application version */
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0

/* Sensor sampling configuration */
#define SENSOR_SAMPLE_INTERVAL_MS    5000    /* 5 seconds */
#define SENSOR_QUEUE_SIZE            10

/* BLE configuration */
#define BLE_DEVICE_NAME              "SecureSensorNode"
#define BLE_NOTIFY_INTERVAL_MS       10000   /* 10 seconds */

/* MQTT configuration */
#define MQTT_BROKER_ADDR             "172.20.10.7"
#define MQTT_BROKER_PORT             1883    /* TLS port */
#define MQTT_CLIENT_ID               "esp32s3_sensor_node"
#define MQTT_PUB_TOPIC               "sensors/data"
#define MQTT_PUB_INTERVAL_MS         15000   /* 15 seconds */
#define MQTT_KEEPALIVE_SEC           60
#define MQTT_QOS                     1

/* WiFi configuration */
#define WIFI_SSID                    "iPhone"
#define WIFI_PSK                     "Tomas@2001"

/* Power management */
#define ENABLE_LOW_POWER_MODE        1
#define SLEEP_DURATION_MS            30000   /* 30 seconds between cycles */

/* Watchdog configuration */
#define WATCHDOG_TIMEOUT_MS          10000   /* 10 seconds */

/* Buffer sizes */
#define JSON_BUFFER_SIZE             512
#define CBOR_BUFFER_SIZE             256

/* Stack sizes */
#define SENSOR_THREAD_STACK_SIZE     2048
#define BLE_THREAD_STACK_SIZE        2048
#define MQTT_THREAD_STACK_SIZE       4096

/* Thread priorities */
#define SENSOR_THREAD_PRIORITY       5
#define BLE_THREAD_PRIORITY          6
#define MQTT_THREAD_PRIORITY         6

#endif /* APP_CONFIG_H */
/* BLE Service UUIDs */
#define BLE_UUID_SENSOR_SERVICE \
    BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xF0DEBC9A, 0x7856, 0x3412, 0x7856, 0x341234125678))

#define BLE_UUID_SENSOR_DATA_CHAR \
    BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xF0DEBC9B, 0x7856, 0x3412, 0x7856, 0x341234125678))
