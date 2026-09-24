/**
 * @file sensor_manager.c
 * @brief Sensor data collection and management coordinator
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>
#include <float.h>
#include "sensor_manager.h"
#include "app_config.h"

LOG_MODULE_REGISTER(sensor_mgr, LOG_LEVEL_INF);

/* External sensor functions */
extern int mpu6050_sensor_init(void);
extern int mpu6050_sensor_calibrate(void);
extern int mpu6050_sensor_read(float *temperature,
                               float *accel_x, float *accel_y, float *accel_z,
                               float *gyro_x, float *gyro_y, float *gyro_z);
extern int adc_battery_init(void);
extern int adc_battery_read(float *voltage_v);

/* Internal state */
static sensor_data_t latest_data = {0};
static K_MUTEX_DEFINE(data_mutex);
static K_SEM_DEFINE(first_sample_sem, 0, 1);
static sensor_data_callback_t data_callback = NULL;

/* Thread control */
static struct k_thread sensor_thread_data;
static K_THREAD_STACK_DEFINE(sensor_thread_stack, SENSOR_THREAD_STACK_SIZE);
static k_tid_t sensor_thread_tid = NULL;
static bool thread_running = false;

#define STANDARD_GRAVITY_MS2 9.80665f
#define RAD_TO_DEG 57.2957795f

struct acceleration_stats {
    uint32_t count;
    float mean;
    float m2;
    float min;
    float max;
};

static struct acceleration_stats accel_stats;
static float last_stats_mean;
static float last_stats_stddev;
static float last_stats_min;
static float last_stats_max;
static bool orientation_initialized;
static float fused_tilt_y;
static float fused_tilt_z;
static int64_t previous_sample_time_ms;

static void update_acceleration_stats(float value, sensor_data_t *data)
{
    if (accel_stats.count == 0U) {
        accel_stats.min = value;
        accel_stats.max = value;
    } else {
        accel_stats.min = MIN(accel_stats.min, value);
        accel_stats.max = MAX(accel_stats.max, value);
    }

    accel_stats.count++;
    float delta = value - accel_stats.mean;
    accel_stats.mean += delta / accel_stats.count;
    float delta2 = value - accel_stats.mean;
    accel_stats.m2 += delta * delta2;

    if (accel_stats.count >= SENSOR_STATS_WINDOW_SAMPLES) {
        last_stats_mean = accel_stats.mean;
        last_stats_stddev = sqrtf(accel_stats.m2 / accel_stats.count);
        last_stats_min = accel_stats.min;
        last_stats_max = accel_stats.max;
        accel_stats = (struct acceleration_stats){0};
    }

    data->accel_mean = last_stats_mean;
    data->accel_stddev = last_stats_stddev;
    data->accel_min = last_stats_min;
    data->accel_max = last_stats_max;
}

static void update_orientation(sensor_data_t *data, int64_t sample_time_ms)
{
    float accel_tilt_y = atan2f(-data->accel_z, data->accel_x) * RAD_TO_DEG;
    float accel_tilt_z = atan2f(data->accel_y, data->accel_x) * RAD_TO_DEG;

    if (!orientation_initialized) {
        fused_tilt_y = accel_tilt_y;
        fused_tilt_z = accel_tilt_z;
        orientation_initialized = true;
    } else {
        float dt = (sample_time_ms - previous_sample_time_ms) / 1000.0f;
        float accel_weight = data->motion_detected ?
                             0.0f : (1.0f - SENSOR_FUSION_ALPHA);
        float gyro_weight = 1.0f - accel_weight;

        fused_tilt_y = gyro_weight *
                       (fused_tilt_y + data->gyro_y * dt * RAD_TO_DEG) +
                       accel_weight * accel_tilt_y;
        fused_tilt_z = gyro_weight *
                       (fused_tilt_z + data->gyro_z * dt * RAD_TO_DEG) +
                       accel_weight * accel_tilt_z;
    }

    previous_sample_time_ms = sample_time_ms;
    data->tilt_y_deg = fused_tilt_y;
    data->tilt_z_deg = fused_tilt_z;
}

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
        int64_t sample_time_ms = k_uptime_get();
        data.timestamp_ms = (uint32_t)sample_time_ms;
        
        /* Fetch temperature, acceleration, and angular velocity together. */
        int ret = mpu6050_sensor_read(&data.temperature_c,
                                      &data.accel_x, &data.accel_y, &data.accel_z,
                                      &data.gyro_x, &data.gyro_y, &data.gyro_z);
        if (ret != 0) {
            LOG_WRN("Failed to read MPU-6050: %d", ret);
            k_msleep(SENSOR_SAMPLE_INTERVAL_MS);
            continue;
        }

        data.accel_magnitude = sqrtf(data.accel_x * data.accel_x +
                                    data.accel_y * data.accel_y +
                                    data.accel_z * data.accel_z);
        data.motion_detected =
            fabsf(data.accel_magnitude - STANDARD_GRAVITY_MS2) >
            SENSOR_MOTION_THRESHOLD_MS2;
        update_acceleration_stats(data.accel_magnitude, &data);
        update_orientation(&data, sample_time_ms);
        
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
        k_sem_give(&first_sample_sem);
        
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
    
    /* Initialize MPU-6050 sensor */
    int ret = mpu6050_sensor_init();
    if (ret != 0) {
        LOG_ERR("Failed to initialize MPU-6050 sensor: %d", ret);
        return ret;
    }

    /* Estimate constant offsets while the stationary sensor has +X up. */
    ret = mpu6050_sensor_calibrate();
    if (ret != 0) {
        LOG_ERR("Failed to calibrate MPU-6050 sensor: %d", ret);
        return ret;
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
    k_sem_reset(&first_sample_sem);
    
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

    int ret = k_sem_take(&first_sample_sem, K_SECONDS(1));
    if (ret != 0) {
        LOG_ERR("Timed out waiting for the first sensor sample");
        thread_running = false;
        k_thread_join(sensor_thread_tid, K_FOREVER);
        sensor_thread_tid = NULL;
        return ret;
    }

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
