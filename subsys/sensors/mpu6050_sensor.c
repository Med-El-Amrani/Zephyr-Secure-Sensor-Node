/*
* @file mpu6050_sensor.c
* @brief MPU-6050 accelerometer and gyroscope driver wrapper 
*/

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mpu6050_sensor, LOG_LEVEL_INF);

#define MPU6050_NODE DT_NODELABEL(mpu6050)

#if !DT_NODE_HAS_STATUS(MPU6050_NODE, okay)
#error "MPU-6050 node is missing or disabled in device tree"
#endif

static const struct device *const mpu6050_dev = DEVICE_DT_GET(MPU6050_NODE);

int mpu6050_sensor_init(void)
{
    if (!device_is_ready(mpu6050_dev)) {
        LOG_ERR("MPU-6050 device not ready");
        return -ENODEV;
    }

    LOG_INF("MPU-6050 initialized: %s", mpu6050_dev->name);
    return 0;
}

int mpu6050_sensor_read(float *temperature,
                        float *accel_x, float *accel_y, float *accel_z,
                        float *gyro_x, float *gyro_y, float *gyro_z)
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
