#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include "app_config.h"
#include "sensor_manager.h"
#include "power_manager.h"
#include "ble_service.h"
#include "mqtt_client.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/*    INITIALIZATION FUNCTIONS        */

static int init_power_manager(void)
{
    int ret = power_manager_init();
    if (ret != 0) {
        LOG_ERR("Power manager init failed: %d", ret);
    }
    return ret;
}

static int init_mqtt_client(void)
{
    int ret = app_mqtt_client_init();
    if (ret != 0) {
        LOG_ERR("MQTT init failed: %d", ret);
        return ret;
    }
    
    LOG_INF("MQTT initialized - connecting...");
    
    ret = mqtt_client_connect();
    if (ret != 0) {
        LOG_ERR("MQTT connection failed: %d", ret);
        return ret;
    }
    
    LOG_INF(" MQTT connected to broker!");
    return 0;
}

static int init_ble_service(void)
{
    int ret = ble_service_init();
    if (ret != 0) {
        LOG_ERR("BLE service init failed: %d", ret);
        return ret;
    }
    LOG_INF("BLE service initialized!");
    
    ret = ble_service_start_advertising();
    if (ret != 0) {
        LOG_ERR("BLE advertising failed: %d", ret);
        return ret;
    }
    LOG_INF("BLE advertising started - device visible!");
    
    return 0;
}

static int init_sensor_manager(void)
{
    int ret = sensor_manager_init();
    if (ret != 0) {
        LOG_ERR("Sensor manager init failed: %d", ret);
        return ret;
    }
    
    ret = sensor_manager_start();
    if (ret != 0) {
        LOG_ERR("Sensor manager start failed: %d", ret);
        return ret;
    }
    
    return 0;
}

/*      DATA DISPLAY FUNCTIONS           */

static void display_sensor_data(const sensor_data_t *data, int counter)
{
    printk("\n=== Sensor Data [%d] ===\n", counter);
    printk(" Temperature: %.1f °C\n", (double)data->temperature_c);
    printk(" Accelerometer:\n");
    printk("    X: %+6.2f m/s²\n", (double)data->accel_x);
    printk("    Y: %+6.2f m/s²\n", (double)data->accel_y);
    printk("    Z: %+6.2f m/s²\n", (double)data->accel_z);
    printk(" Battery: %.2f V\n", (double)data->battery_voltage);
}

/*     BLE NOTIFICATION HANDLER           */

static void handle_ble_notification(const sensor_data_t *data, int counter)
{
    if (!ble_service_is_connected()) {
        printk(" BLE: Not connected\n");
        return;
    }
    
    if (counter == 0) {
        printk(" BLE: Waiting for MTU negotiation...\n");
        return;
    }
    
    int ret = ble_service_notify(data);
    if (ret == 0) {
        printk("✓ BLE notification sent!\n");
    } else if (ret == -EAGAIN) {
        printk(" BLE: MTU not ready yet\n");
    } else {
        printk(" BLE notification failed: %d\n", ret);
    }
}

/*   MQTT PUBLICATION HANDLER   */

static void handle_mqtt_publication(const sensor_data_t *data)
{
    if (!mqtt_client_is_connected()) {
        printk(" MQTT: Not connected\n");
        return;
    }
    
    int ret = mqtt_client_publish_sensor_data(data);
    if (ret == 0) {
        printk(" MQTT data published!\n");
    } else {
        printk(" MQTT publish failed: %d\n", ret);
    }
}

/*   MAIN LOOP FUNCTIONS       */

static void process_sensor_data(int counter)
{
    sensor_data_t data;
    int ret = sensor_manager_get_data(&data);
    
    if (ret != 0 || !data.valid) {
        printk(" No valid sensor data (ret=%d)\n", ret);
        return;
    }
    
    // Display sensor data
    display_sensor_data(&data, counter);
    
    // Send via BLE
    handle_ble_notification(&data, counter);
    
    // Publish via MQTT
    handle_mqtt_publication(&data);
    
    printk("\n");
}

static void process_maintenance_tasks(int counter)
{
    // Process MQTT events
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
}

static void sleep_cycle(void)
{
    int ret = power_manager_enter_low_power(POWER_STATE_IDLE, 5000);
    if (ret != 0) {
        k_msleep(5000);
    }
}


int main(void)
{
    int ret;
    
    printk("\n\n=== SECURE SENSOR NODE - FULL VERSION ===\n");
    LOG_INF("Starting with BLE + MQTT...");
    
    // Initialize all subsystems
    init_power_manager();
    
    ret = init_mqtt_client();
    if (ret != 0) {
        LOG_WRN("Continuing without MQTT...");
    }
    
    ret = init_ble_service();
    if (ret != 0) {
        return ret;
    }
    
    ret = init_sensor_manager();
    if (ret != 0) {
        return ret;
    }
    
    // Main loop
    int counter = 0;
    
    while (1) {
        process_sensor_data(counter);
        process_maintenance_tasks(counter);
        counter++;
        sleep_cycle();
    }
    
    return 0;
}