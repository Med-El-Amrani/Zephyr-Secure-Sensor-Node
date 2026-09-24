/*
* @file mpu6050_sensor.c
* @brief MPU-6050 accelerometer and gyroscope driver wrapper 
*/

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "app_config.h"

LOG_MODULE_REGISTER(mpu6050_sensor, LOG_LEVEL_INF);

#define MPU6050_NODE DT_NODELABEL(mpu6050)
#define MPU6050_CALIBRATION_SAMPLES 500
#define MPU6050_CALIBRATION_DELAY_MS 10
#define STANDARD_GRAVITY_MS2 9.80665f

#if !DT_NODE_HAS_STATUS(MPU6050_NODE, okay)
#error "MPU-6050 node is missing or disabled in device tree"
#endif

static const struct device *const mpu6050_dev = DEVICE_DT_GET(MPU6050_NODE);

struct mpu6050_bias {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
};

static struct mpu6050_bias bias;
static bool calibration_valid;
static bool filter_initialized;
static float filtered_accel_x;
static float filtered_accel_y;
static float filtered_accel_z;
static float filtered_gyro_x;
static float filtered_gyro_y;
static float filtered_gyro_z;

int mpu6050_sensor_init(void)
{
    if (!device_is_ready(mpu6050_dev)) {
        LOG_ERR("MPU-6050 device not ready");
        return -ENODEV;
    }

    LOG_INF("MPU-6050 initialized: %s", mpu6050_dev->name);
    return 0;
}

static int mpu6050_sensor_read_raw(float *temperature,
                                   float *accel_x, float *accel_y,
                                   float *accel_z, float *gyro_x,
                                   float *gyro_y, float *gyro_z)
{
    struct sensor_value temperature_value;
    struct sensor_value accel[3];
    struct sensor_value gyro[3];
    int ret;

    if (temperature == NULL ||
        accel_x == NULL || accel_y == NULL || accel_z == NULL ||
        gyro_x == NULL || gyro_y == NULL || gyro_z == NULL) {
        return -EINVAL;
    }

    ret = sensor_sample_fetch(mpu6050_dev);
    if (ret != 0) {
        LOG_ERR("Failed to fetch MPU-6050 sample: %d", ret);
        return ret;
    }

    ret = sensor_channel_get(mpu6050_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
    if (ret != 0) {
        LOG_ERR("Failed to read MPU-6050 acceleration: %d", ret);
        return ret;
    }

    ret = sensor_channel_get(mpu6050_dev, SENSOR_CHAN_GYRO_XYZ, gyro);
    if (ret != 0) {
        LOG_ERR("Failed to read MPU-6050 gyroscope: %d", ret);
        return ret;
    }

    ret = sensor_channel_get(mpu6050_dev, SENSOR_CHAN_DIE_TEMP,
                             &temperature_value);
    if (ret != 0) {
        LOG_ERR("Failed to read MPU-6050 temperature: %d", ret);
        return ret;
    }

    *temperature = (float)sensor_value_to_double(&temperature_value);
    *accel_x = (float)sensor_value_to_double(&accel[0]);
    *accel_y = (float)sensor_value_to_double(&accel[1]);
    *accel_z = (float)sensor_value_to_double(&accel[2]);
    *gyro_x = (float)sensor_value_to_double(&gyro[0]);
    *gyro_y = (float)sensor_value_to_double(&gyro[1]);
    *gyro_z = (float)sensor_value_to_double(&gyro[2]);

    LOG_DBG("MPU-6050: temp=%.2f C, accel=(%.3f, %.3f, %.3f) m/s^2, "
            "gyro=(%.3f, %.3f, %.3f) rad/s",
            (double)*temperature,
            (double)*accel_x, (double)*accel_y, (double)*accel_z,
            (double)*gyro_x, (double)*gyro_y, (double)*gyro_z);

    return 0;
}

int mpu6050_sensor_calibrate(void)
{
    float accel_x_sum = 0.0f;
    float accel_y_sum = 0.0f;
    float accel_z_sum = 0.0f;
    float gyro_x_sum = 0.0f;
    float gyro_y_sum = 0.0f;
    float gyro_z_sum = 0.0f;
    float temperature;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    int ret;

    calibration_valid = false;
    filter_initialized = false;
    LOG_INF("Calibrating MPU-6050: keep it still with +X pointing up for %d ms",
            MPU6050_CALIBRATION_SAMPLES * MPU6050_CALIBRATION_DELAY_MS);

    for (int i = 0; i < MPU6050_CALIBRATION_SAMPLES; i++) {
        ret = mpu6050_sensor_read_raw(&temperature,
                                      &accel_x, &accel_y, &accel_z,
                                      &gyro_x, &gyro_y, &gyro_z);
        if (ret != 0) {
            LOG_ERR("MPU-6050 calibration failed at sample %d: %d", i, ret);
            return ret;
        }

        accel_x_sum += accel_x;
        accel_y_sum += accel_y;
        accel_z_sum += accel_z;
        gyro_x_sum += gyro_x;
        gyro_y_sum += gyro_y;
        gyro_z_sum += gyro_z;
        k_msleep(MPU6050_CALIBRATION_DELAY_MS);
    }

    /* With +X up, gravity is expected only on X during calibration. */
    bias.accel_x = (accel_x_sum / MPU6050_CALIBRATION_SAMPLES) -
                   STANDARD_GRAVITY_MS2;
    bias.accel_y = accel_y_sum / MPU6050_CALIBRATION_SAMPLES;
    bias.accel_z = accel_z_sum / MPU6050_CALIBRATION_SAMPLES;
    bias.gyro_x = gyro_x_sum / MPU6050_CALIBRATION_SAMPLES;
    bias.gyro_y = gyro_y_sum / MPU6050_CALIBRATION_SAMPLES;
    bias.gyro_z = gyro_z_sum / MPU6050_CALIBRATION_SAMPLES;
    calibration_valid = true;

    LOG_INF("Calibration done: accel bias=(%.4f, %.4f, %.4f) m/s^2",
            (double)bias.accel_x, (double)bias.accel_y,
            (double)bias.accel_z);
    LOG_INF("Calibration done: gyro bias=(%.5f, %.5f, %.5f) rad/s",
            (double)bias.gyro_x, (double)bias.gyro_y,
            (double)bias.gyro_z);

    return 0;
}

int mpu6050_sensor_read(float *temperature,
                        float *accel_x, float *accel_y, float *accel_z,
                        float *gyro_x, float *gyro_y, float *gyro_z)
{
    float raw_temperature;
    float raw_accel_x;
    float raw_accel_y;
    float raw_accel_z;
    float raw_gyro_x;
    float raw_gyro_y;
    float raw_gyro_z;
    int ret;

    if (temperature == NULL ||
        accel_x == NULL || accel_y == NULL || accel_z == NULL ||
        gyro_x == NULL || gyro_y == NULL || gyro_z == NULL) {
        return -EINVAL;
    }

    if (!calibration_valid) {
        LOG_ERR("MPU-6050 must be calibrated before reading corrected data");
        return -EAGAIN;
    }

    ret = mpu6050_sensor_read_raw(&raw_temperature,
                                  &raw_accel_x, &raw_accel_y, &raw_accel_z,
                                  &raw_gyro_x, &raw_gyro_y, &raw_gyro_z);
    if (ret != 0) {
        return ret;
    }

    raw_accel_x -= bias.accel_x;
    raw_accel_y -= bias.accel_y;
    raw_accel_z -= bias.accel_z;
    raw_gyro_x -= bias.gyro_x;
    raw_gyro_y -= bias.gyro_y;
    raw_gyro_z -= bias.gyro_z;

    if (!filter_initialized) {
        filtered_accel_x = raw_accel_x;
        filtered_accel_y = raw_accel_y;
        filtered_accel_z = raw_accel_z;
        filtered_gyro_x = raw_gyro_x;
        filtered_gyro_y = raw_gyro_y;
        filtered_gyro_z = raw_gyro_z;
        filter_initialized = true;
    } else { // low pass filter to reduce noise
        filtered_accel_x += SENSOR_FILTER_ALPHA *
                            (raw_accel_x - filtered_accel_x);
        filtered_accel_y += SENSOR_FILTER_ALPHA *
                            (raw_accel_y - filtered_accel_y);
        filtered_accel_z += SENSOR_FILTER_ALPHA *
                            (raw_accel_z - filtered_accel_z);
        filtered_gyro_x += SENSOR_FILTER_ALPHA *
                           (raw_gyro_x - filtered_gyro_x);
        filtered_gyro_y += SENSOR_FILTER_ALPHA *
                           (raw_gyro_y - filtered_gyro_y);
        filtered_gyro_z += SENSOR_FILTER_ALPHA *
                           (raw_gyro_z - filtered_gyro_z);
    }

    *temperature = raw_temperature;
    *accel_x = filtered_accel_x;
    *accel_y = filtered_accel_y;
    *accel_z = filtered_accel_z;
    *gyro_x = filtered_gyro_x;
    *gyro_y = filtered_gyro_y;
    *gyro_z = filtered_gyro_z;

    return 0;
}
