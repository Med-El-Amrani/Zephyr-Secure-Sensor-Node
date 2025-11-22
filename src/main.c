#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include "app_config.h"
#include "sensor_manager.h"
#include "power_manager.h"
#include "ble_service.h"
#include "mqtt_client.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    printk("\n\n=== SECURE SENSOR NODE - FULL VERSION ===\n");
    LOG_INF("Starting with BLE + MQTT...");
    
    // Initialiser power manager
    int ret = power_manager_init();
    if (ret != 0) {
        LOG_ERR("Power manager init failed: %d", ret);
    }
    
    // ✨ Initialiser MQTT client
    ret = app_mqtt_client_init();
    if (ret != 0) {
        LOG_ERR("MQTT init failed: %d", ret);
        // Continue sans MQTT
    } else {
        LOG_INF("MQTT initialized - connecting...");
        
        // Connecter au broker MQTT
        ret = mqtt_client_connect();
        if (ret != 0) {
            LOG_ERR("MQTT connection failed: %d", ret);
            // Continue sans MQTT
        } else {
            LOG_INF("✓ MQTT connected to broker!");
        }
    }
    
    // Initialiser BLE service
    ret = ble_service_init();
    if (ret != 0) {
        LOG_ERR("BLE service init failed: %d", ret);
        return ret;
    }
    LOG_INF("BLE service initialized!");
    
    // Démarrer l'advertising BLE
    ret = ble_service_start_advertising();
    if (ret != 0) {
        LOG_ERR("BLE advertising failed: %d", ret);
        return ret;
    }
    LOG_INF("BLE advertising started - device visible!");
    
    // Initialiser sensor manager
    ret = sensor_manager_init();
    if (ret != 0) {
        LOG_ERR("Sensor manager init failed: %d", ret);
    }
    
    ret = sensor_manager_start();
    if (ret != 0) {
        LOG_ERR("Sensor manager start failed: %d", ret);
        return ret;
    }
    
    int counter = 0;
    sensor_data_t data;
    
    while (1) {
        ret = sensor_manager_get_data(&data);
        
        if (ret == 0 && data.valid) {
            // ===== Affichage Console =====
            printk("\n=== Sensor Data [%d] ===\n", counter);
            printk("🌡️  Temperature: %.1f °C\n", (double)data.temperature_c);
            printk("📐 Accelerometer:\n");
            printk("    X: %+6.2f m/s²\n", (double)data.accel_x);
            printk("    Y: %+6.2f m/s²\n", (double)data.accel_y);
            printk("    Z: %+6.2f m/s²\n", (double)data.accel_z);
            printk("🔋 Battery: %.2f V\n", (double)data.battery_voltage);
            
            // ===== BLE Notification =====
            if (ble_service_is_connected()) {
                if (counter > 0) {  // Skip première notification
                    ret = ble_service_notify(&data);
                    if (ret == 0) {
                        printk("✓ BLE notification sent!\n");
                    } else if (ret == -EAGAIN) {
                        printk("○ BLE: MTU not ready yet\n");
                    } else {
                        printk("✗ BLE notification failed: %d\n", ret);
                    }
                } else {
                    printk("○ BLE: Waiting for MTU negotiation...\n");
                }
            } else {
                printk("○ BLE: Not connected\n");
            }
            
            // ✨ ===== MQTT Publication =====

            if (mqtt_client_is_connected()) {
                ret = mqtt_client_publish_sensor_data(&data);
                if (ret == 0) {
                    printk("✓ MQTT data published!\n");
                } else {
                    printk("✗ MQTT publish failed: %d\n", ret);
                }
            } else {
                printk("○ MQTT: Not connected\n");
            }
            
            printk("\n");
        } else {
            printk("○ No valid sensor data (ret=%d)\n", ret);
        }
        
        // ✨ Process MQTT events (important!)
        if (mqtt_client_is_connected()) {
            mqtt_client_process();
        }
        
        // Feed watchdog
        power_manager_feed_watchdog();
        
        // Status log
        LOG_INF("Counter: %d | BLE: %s | MQTT: %s", 
                counter,
                ble_service_is_connected() ? "✓" : "✗",
                mqtt_client_is_connected() ? "✓" : "✗");
        
        counter++;
        
        // Sleep
        ret = power_manager_enter_low_power(POWER_STATE_IDLE, 5000);
        if (ret != 0) {
            k_msleep(5000);
        }
    }
    
    return 0;
}