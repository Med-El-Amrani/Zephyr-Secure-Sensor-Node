/**
 * @file ble_service.c
 * @brief BLE GATT service for sensor data notifications
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include "ble_service.h"
#include "app_config.h"

LOG_MODULE_REGISTER(ble_svc, LOG_LEVEL_INF);

/* BLE connection state */
static struct bt_conn *current_conn = NULL;
static bool notify_enabled = false;

/* Sensor data characteristic value (JSON formatted) */
static uint8_t sensor_data_value[JSON_BUFFER_SIZE];
static uint16_t sensor_data_len = 0;

/* External JSON encoder */
extern int json_encode_sensor_data_with_metadata(const sensor_data_t *data,
                                                 char *buffer,
                                                 size_t buffer_size,
                                                 const char *device_id);

/**
 * @brief CCC (Client Characteristic Configuration) changed callback
 */
static void sensor_data_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Sensor data notifications %s", notify_enabled ? "enabled" : "disabled");
}

/**
 * @brief GATT service definition
 */
BT_GATT_SERVICE_DEFINE(sensor_service,
    BT_GATT_PRIMARY_SERVICE(BLE_UUID_SENSOR_SERVICE),
    BT_GATT_CHARACTERISTIC(BLE_UUID_SENSOR_DATA_CHAR,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          NULL, NULL, sensor_data_value),
    BT_GATT_CCC(sensor_data_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/**
 * @brief BLE advertising data
 */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, BLE_DEVICE_NAME, sizeof(BLE_DEVICE_NAME) - 1),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BLE_UUID_SENSOR_SERVICE),
};

/**
 * @brief Connection callback
 */
static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    
    if (err) {
        LOG_ERR("Connection failed: %u", err);
        return;
    }
    
    LOG_INF("Connected: %s", addr);
    current_conn = bt_conn_ref(conn);
}

/**
 * @brief Disconnection callback
 */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    
    LOG_INF("Disconnected: %s (reason %u)", addr, reason);
    
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    
    notify_enabled = false;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int ble_service_init(void)
{
    LOG_INF("Initializing BLE service...");
    
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }
    
    LOG_INF("Bluetooth initialized");
    return 0;
}

int ble_service_start_advertising(void)
{
    int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return err;
    }
    
    LOG_INF("BLE advertising started");
    return 0;
}

int ble_service_stop_advertising(void)
{
    int err = bt_le_adv_stop();
    if (err) {
        LOG_ERR("Failed to stop advertising (err %d)", err);
        return err;
    }
    
    LOG_INF("BLE advertising stopped");
    return 0;
}

int ble_service_notify(const sensor_data_t *data)
{
    if (data == NULL) {
        return -EINVAL;
    }
    
    if (!current_conn || !notify_enabled) {
        LOG_DBG("No connected client or notifications not enabled");
        return -ENOTCONN;
    }
    
    /* Encode sensor data to JSON */
    char json_buf[JSON_BUFFER_SIZE];
    int len = json_encode_sensor_data_with_metadata(data, json_buf, sizeof(json_buf),
                                                    BLE_DEVICE_NAME);
    if (len < 0) {
        LOG_ERR("Failed to encode JSON: %d", len);
        return len;
    }
    
    /* Copy to characteristic value */
    sensor_data_len = MIN(len, sizeof(sensor_data_value));
    memcpy(sensor_data_value, json_buf, sensor_data_len);
    
    /* Send notification */
    int err = bt_gatt_notify(NULL, &sensor_service.attrs[1], 
                            sensor_data_value, sensor_data_len);
    if (err) {
        LOG_ERR("Failed to send notification: %d", err);
        return err;
    }
    
    LOG_DBG("Sent BLE notification (%d bytes)", sensor_data_len);
    return 0;
}

bool ble_service_is_connected(void)
{
    return (current_conn != NULL);
}