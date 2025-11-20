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
#include "sensor_manager.h"

LOG_MODULE_REGISTER(ble_svc, LOG_LEVEL_INF);

/* Connexion et état */
static struct bt_conn *current_conn = NULL;
static bool notify_enabled = false;

/* Buffer pour les données JSON */
#define JSON_BUFFER_SIZE 256
static char json_buffer[JSON_BUFFER_SIZE];

/* UUID Service : 12345678-1234-5678-1234-56789abcdef0 */
#define BT_UUID_SENSOR_SERVICE \
    BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0))

/* UUID Characteristic : 12345678-1234-5678-1234-56789abcdef1 */
#define BT_UUID_SENSOR_DATA_CHAR \
    BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1))

/**
 * @brief CCC change handler
 */
static void sensor_data_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Notifications %s", notify_enabled ? "enabled" : "disabled");
}

/**
 * @brief GATT service
 */
BT_GATT_SERVICE_DEFINE(sensor_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_SENSOR_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_SENSOR_DATA_CHAR,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                          BT_GATT_PERM_READ,
                          NULL, NULL, json_buffer),
    BT_GATT_CCC(sensor_data_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/**
 * @brief Advertising data
 */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_NAME_COMPLETE,
        'S', 'e', 'n', 's', 'o', 'r', 'N', 'o', 'd', 'e'),
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

    LOG_INF("✓ Connected: %s", addr);
    current_conn = bt_conn_ref(conn);
}

/**
 * @brief Disconnect callback
 */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("✗ Disconnected: %s (reason %u)", addr, reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    notify_enabled = false;
    
    /* Redémarrer advertising après déconnexion */
    LOG_INF("Restarting advertising...");
    ble_service_start_advertising();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int ble_service_init(void)
{
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }

    /* Afficher l'adresse MAC */
    bt_addr_le_t addr;
    size_t count = 1;
    bt_id_get(&addr, &count);
    
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
    
    LOG_INF("===========================================");
    LOG_INF("*** MAC Address: %s ***", addr_str);
    LOG_INF("===========================================");
    LOG_INF("Bluetooth initialized");
    
    return 0;
}

int ble_service_start_advertising(void)
{
    struct bt_le_adv_param adv_param = {
        .id = BT_ID_DEFAULT,
        .options = BT_LE_ADV_OPT_CONN,  // Utiliser BT_LE_ADV_OPT_CONN au lieu de CONNECTABLE
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
        .peer = NULL,
    };

    int err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Failed to start advertising (err %d)", err);
        return err;
    }

    LOG_INF("✓ Advertising started - Name: SensorNode");
    return 0;
}

int ble_service_stop_advertising(void)
{
    int err = bt_le_adv_stop();
    if (err) {
        LOG_ERR("Failed to stop advertising (err %d)", err);
        return err;
    }

    LOG_INF("Advertising stopped");
    return 0;
}

int ble_service_notify(const sensor_data_t *data)
{
    if (!current_conn || !notify_enabled) {
        return -ENOTCONN;
    }

    /* Créer JSON compact */
    int len = snprintf(json_buffer, sizeof(json_buffer),
        "{\"t\":%.1f,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"b\":%.2f}",
        (double)data->temperature_c,
        (double)data->accel_x,
        (double)data->accel_y,
        (double)data->accel_z,
        (double)data->battery_voltage
    );

    if (len < 0 || len >= sizeof(json_buffer)) {
        LOG_ERR("JSON encode failed");
        return -ENOMEM;
    }
    
    LOG_DBG("Sending: %d bytes", len);
    
    return bt_gatt_notify(current_conn, &sensor_service.attrs[1],
                         json_buffer, len);
}

bool ble_service_is_connected(void)
{
    return (current_conn != NULL && notify_enabled);
}