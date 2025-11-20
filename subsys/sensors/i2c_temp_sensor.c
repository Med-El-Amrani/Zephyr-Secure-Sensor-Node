/**
 * @file i2c_temp_sensor.c
 * @brief I²C temperature sensor driver (stub for generic I²C temp sensor)
 * 
 * This is a stub implementation. Replace with actual sensor driver
 * (e.g., TMP117, BME280, SHT3x, etc.)
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_temp, LOG_LEVEL_DBG);

/* ★★ Use alias from Devicetree overlay instead of DT_NODELABEL(i2c0) */
#define I2C_DEV_NODE DT_ALIAS(i2c_thermo)

/* Example address */
#define TEMP_SENSOR_ADDR 0x48

#define CONFIG_I2C_TEMP_SENSOR_STUB 1

static const struct device *i2c_dev;


static uint32_t local_rand32(void)
{
    static uint32_t seed = 0;
    
    if (seed == 0) {
        seed = k_uptime_get_32() + 1;
    }
    
    seed = (1103515245 * seed + 12345) & 0x7FFFFFFF;
    return seed;
}

/**
 * @brief Initialize I²C temperature sensor
 */
int i2c_temp_sensor_init(void)
{
    /* ★★ Use alias-based device */
#if DT_NODE_HAS_STATUS(I2C_DEV_NODE, okay)
    i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
#else
#error "i2c-thermo alias not defined or disabled in the Devicetree"
#endif

    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I²C device not ready");
        return -ENODEV;
    }

    LOG_INF("I²C temperature sensor initialized");
    return 0;
}

/**
 * @brief Read temperature from I²C sensor
 * @param temp_c Pointer to store temperature in Celsius
 * @return 0 on success, negative errno on failure
 */
int i2c_temp_sensor_read(float *temp_c)
{
    if (temp_c == NULL) {
        return -EINVAL;
    }


#if defined(CONFIG_I2C_TEMP_SENSOR_STUB)
    /* STUB: Generate simulated temperature */
    *temp_c = 20.0f + (float)(local_rand32() % 1000) / 100.0f;
    LOG_DBG("Temperature (stub): %.2f°C", (double)*temp_c);
    return 0;

#else
    uint8_t data[2];

    int ret = i2c_burst_read(i2c_dev, TEMP_SENSOR_ADDR, 0x00, data, sizeof(data));
    if (ret != 0) {
        LOG_ERR("Failed to read from I²C sensor: %d", ret);
        return ret;
    }

    /* Convert raw data (example conversion) */
    int16_t raw = (data[0] << 8) | data[1];
    *temp_c = (float)raw * 0.0078125f;

    return 0;
#endif
}