/**
 * @file main.c
 * @brief Secure Sensor Node - Main Application
 * 
 * This application coordinates sensor sampling, BLE GATT notifications,
 * and MQTT publishing with TLS support on ESP32-S3.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "app_config.h"
#include "sensor_manager.h"
#include "ble_service.h"
#include "mqtt_client.h"
#include "power_manager.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Timing control */
static uint32_t last_ble_notify_ms = 0;
static uint32_t last_mqtt_publish_ms = 0;

/**
 * @brief Sensor data callback - called when new sensor data is available
 */
static void sensor_data_callback(const sensor_data_t *data)
{
    uint32_t now_ms = k_uptime_get_32();
    
    /* Send BLE notification if interval elapsed and client connected */
    if (ble_service_is_connected() && 
        (now_ms - last_ble_notify_ms) >= BLE_NOTIFY_INTERVAL_MS) {
        
        int ret = ble_service_notify(data);
        if (ret == 0) {
            last_ble_notify_ms = now_ms;
            LOG_INF("BLE notification sent");
        }
    }
    
    /* Publish to MQTT if interval elapsed and connected */
    if (mqtt_client_is_connected() &&
        (now_ms - last_mqtt_publish_ms) >= MQTT_PUB_INTERVAL_MS) {
        
        int ret = mqtt_client_publish_sensor_data(data);
        if (ret == 0) {
            last_mqtt_publish_ms = now_ms;
            LOG_INF("MQTT data published");
        }
    }
    
    /* Feed watchdog to prevent reset */
    power_manager_feed_watchdog();
}

/**
 * @brief Initialize all subsystems
 */
static int initialize_subsystems(void)
{
    int ret;
    
    LOG_INF("=== Secure Sensor Node v%d.%d.%d ===",
            APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
    
    /* Initialize power management */
    ret = power_manager_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize power manager: %d", ret);
        return ret;
    }
    
    /* Setup watchdog */
#ifdef CONFIG_WATCHDOG
    ret = power_manager_setup_watchdog(WATCHDOG_TIMEOUT_MS);
    if (ret != 0) {
        LOG_WRN("Watchdog setup failed (continuing anyway): %d", ret);
    }
#endif
    
    /* Initialize sensor manager */
    ret = sensor_manager_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize sensor manager: %d", ret);
        return ret;
    }
    
    /* Register sensor data callback */
    sensor_manager_register_callback(sensor_data_callback);
    
    /* Initialize BLE service */
#ifdef CONFIG_BT
    ret = ble_service_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize BLE service: %d", ret);
        return ret;
    }
    
    ret = ble_service_start_advertising();
    if (ret != 0) {
        LOG_ERR("Failed to start BLE advertising: %d", ret);
        return ret;
    }
#else
    LOG_WRN("BLE not configured");
#endif
    
    /* Initialize MQTT client */
#ifdef CONFIG_MQTT_LIB
    ret = mqtt_client_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize MQTT client: %d", ret);
        /* Continue without MQTT */
    } else {
        ret = mqtt_client_connect();
        if (ret != 0) {
            LOG_ERR("Failed to connect MQTT: %d", ret);
            /* Continue without MQTT */
        }
    }
#else
    LOG_WRN("MQTT not configured");
#endif
    
    LOG_INF("All subsystems initialized successfully");
    return 0;
}

/**
 * @brief Main application entry point
 */
int main(void)
{
    LOG_INF("Starting Secure Sensor Node...");
    
    /* Initialize all subsystems */
    int ret = initialize_subsystems();
    if (ret != 0) {
        LOG_ERR("Initialization failed: %d", ret);
        return ret;
    }
    
    /* Start sensor sampling */
    ret = sensor_manager_start();
    if (ret != 0) {
        LOG_ERR("Failed to start sensor manager: %d", ret);
        return ret;
    }
    
    LOG_INF("Secure Sensor Node is running...");
    LOG_INF("- Sensor sampling interval: %d ms", SENSOR_SAMPLE_INTERVAL_MS);
    LOG_INF("- BLE notification interval: %d ms", BLE_NOTIFY_INTERVAL_MS);
    LOG_INF("- MQTT publish interval: %d ms", MQTT_PUB_INTERVAL_MS);
    
    /* Main loop */
    while (1) {
        /* Process MQTT events */
#ifdef CONFIG_MQTT_LIB
        if (mqtt_client_is_connected()) {
            mqtt_client_process();
        }
#endif
        
        /* Feed watchdog */
        power_manager_feed_watchdog();
        
        /* Sleep to reduce power consumption */
#if ENABLE_LOW_POWER_MODE
        power_manager_enter_low_power(POWER_STATE_IDLE, 1000);
#else
        k_msleep(1000);
#endif
        
        /* Log status periodically */
        static uint32_t last_status_log = 0;
        uint32_t now = k_uptime_get_32();
        if ((now - last_status_log) >= 60000) {  /* Every 60 seconds */
            last_status_log = now;
            LOG_INF("Status: BLE %s, MQTT %s",
                    ble_service_is_connected() ? "connected" : "disconnected",
                    mqtt_client_is_connected() ? "connected" : "disconnected");
        }
    }
    
    return 0;
}