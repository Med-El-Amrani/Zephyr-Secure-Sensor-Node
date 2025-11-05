/**
 * @file sensor_manager.c
 * @brief Sensor data collection and management coordinator
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensor_manager.h"
#include "app_config.h"

LOG_MODULE_REGISTER(sensor_mgr, LOG_LEVEL_INF);

/* External sensor functions */
extern int i2c_temp_sensor_init(void);
extern int i2c_temp_sensor_read(float *temp_c);
extern int spi_accel_sensor_init(void);
extern int spi_accel_sensor_read(float *x, float *y, float *z);
extern int adc_battery_init(void);
extern int adc_battery_read(float *voltage_v);

/* Internal state */
static sensor_data_t latest_data = {0};
static K_MUTEX_DEFINE(data_mutex);
static sensor_data_callback_t data_callback = NULL;

/* Thread control */
static struct k_thread sensor_thread_data;
static K_THREAD_STACK_DEFINE(sensor_thread_stack, SENSOR_THREAD_STACK_SIZE);
static k_tid_t sensor_thread_tid = NULL;
static bool thread_running = false;

/**
 * @brief Sensor sampling thread
 */
static void sensor_thread(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);
    
    LOG_INF("Sensor thread started");
    
    while (thread_running) {
        sensor_data_t data = {0};
        data.timestamp_ms = k_uptime_get_32();
        
        /* Read temperature sensor */
        int ret = i2c_temp_sensor_read(&data.temperature_c);
        if (ret != 0) {
            LOG_WRN("Failed to read temperature: %d", ret);
        }
        
        /* Read accelerometer */
        ret = spi_accel_sensor_read(&data.accel_x, &data.accel_y, &data.accel_z);
        if (ret != 0) {
            LOG_WRN("Failed to read accelerometer: %d", ret);
        }
        
        /* Read battery voltage */
        ret = adc_battery_read(&data.battery_voltage);
        if (ret != 0) {
            LOG_WRN("Failed to read battery: %d", ret);
        }
        
        /* Mark data as valid */
        data.valid = true;
        
        /* Update latest data with mutex protection */
        k_mutex_lock(&data_mutex, K_FOREVER);
        memcpy(&latest_data, &data, sizeof(sensor_data_t));
        k_mutex_unlock(&data_mutex);
        
        LOG_INF("Sensor data: T=%.1f°C, Accel=(%.2f,%.2f,%.2f)m/s², Batt=%.2fV",
                (double)data.temperature_c,
                (double)data.accel_x, (double)data.accel_y, (double)data.accel_z,
                (double)data.battery_voltage);
        
        /* Notify callback if registered */
        if (data_callback != NULL) {
            data_callback(&data);
        }
        
        /* Sleep until next sample */
        k_msleep(SENSOR_SAMPLE_INTERVAL_MS);
    }
    
    LOG_INF("Sensor thread stopped");
}

int sensor_manager_init(void)
{
    LOG_INF("Initializing sensor manager...");
    
    /* Initialize I²C temperature sensor */
    int ret = i2c_temp_sensor_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize I²C temp sensor: %d", ret);
        /* Continue anyway for stub mode */
    }
    
    /* Initialize SPI accelerometer */
    ret = spi_accel_sensor_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize SPI accel: %d", ret);
        /* Continue anyway for stub mode */
    }
    
    /* Initialize ADC battery monitor */
    ret = adc_battery_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize ADC battery: %d", ret);
        /* Continue anyway for stub mode */
    }
    
    LOG_INF("Sensor manager initialized successfully");
    return 0;
}

int sensor_manager_start(void)
{
    if (thread_running) {
        LOG_WRN("Sensor thread already running");
        return -EALREADY;
    }
    
    thread_running = true;
    
    sensor_thread_tid = k_thread_create(&sensor_thread_data, sensor_thread_stack,
                                       K_THREAD_STACK_SIZEOF(sensor_thread_stack),
                                       sensor_thread,
                                       NULL, NULL, NULL,
                                       SENSOR_THREAD_PRIORITY, 0, K_NO_WAIT);
    
    if (sensor_thread_tid == NULL) {
        LOG_ERR("Failed to create sensor thread");
        thread_running = false;
        return -ENOMEM;
    }
    
    k_thread_name_set(sensor_thread_tid, "sensor_mgr");
    LOG_INF("Sensor manager started");
    return 0;
}

void sensor_manager_stop(void)
{
    if (!thread_running) {
        return;
    }
    
    thread_running = false;
    
    if (sensor_thread_tid != NULL) {
        k_thread_join(sensor_thread_tid, K_FOREVER);
        sensor_thread_tid = NULL;
    }
    
    LOG_INF("Sensor manager stopped");
}

int sensor_manager_get_data(sensor_data_t *data)
{
    if (data == NULL) {
        return -EINVAL;
    }
    
    k_mutex_lock(&data_mutex, K_FOREVER);
    memcpy(data, &latest_data, sizeof(sensor_data_t));
    k_mutex_unlock(&data_mutex);
    
    return latest_data.valid ? 0 : -ENODATA;
}

void sensor_manager_register_callback(sensor_data_callback_t callback)
{
    data_callback = callback;
    LOG_INF("Sensor data callback registered");
}