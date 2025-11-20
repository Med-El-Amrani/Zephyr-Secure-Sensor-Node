/**
 * @file spi_accel_sensor.c
 * @brief SPI accelerometer sensor driver (stub for generic SPI accelerometer)
 * 
 * This is a stub implementation. Replace with actual sensor driver
 * (e.g., ADXL345, MPU6050 with SPI, LIS3DH, etc.)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>  // Pour sys_rand32_get()

LOG_MODULE_REGISTER(spi_accel, LOG_LEVEL_DBG);

/* ★★ Use DT alias instead of DT_NODELABEL(spi2) */
#define SPI_DEV_NODE DT_ALIAS(spi_accel)

#if !DT_NODE_HAS_STATUS(SPI_DEV_NODE, okay)
#error "spi-accel alias not defined or device disabled in Devicetree"
#endif

static const struct device *spi_dev;

static struct spi_config spi_cfg = {
    .frequency = 1000000,              /* 1 MHz */
    .operation = SPI_WORD_SET(8) |
                 SPI_TRANSFER_MSB |
                 SPI_OP_MODE_MASTER,
    .slave = 0,
};

/**
 * @brief Initialize SPI accelerometer sensor
 */
int spi_accel_sensor_init(void)
{
    spi_dev = DEVICE_DT_GET(SPI_DEV_NODE);

    if (!device_is_ready(spi_dev)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }

    LOG_INF("SPI accelerometer sensor initialized");
    return 0;
}

/**
 * @brief Read accelerometer data (stub or real)
 */
int spi_accel_sensor_read(float *accel_x, float *accel_y, float *accel_z)
{
    if (!accel_x || !accel_y || !accel_z) {
        return -EINVAL;
    }

#ifndef USE_REAL_SPI_SENSOR
    /* STUB simulation */
    *accel_x = ((float)(sys_rand32_get() % 200) - 100) / 100.0f;
    *accel_y = ((float)(sys_rand32_get() % 200) - 100) / 100.0f;
    *accel_z = 9.81f + ((float)(sys_rand32_get() % 100) - 50) / 100.0f;

    LOG_DBG("Accel (stub): X=%.2f Y=%.2f Z=%.2f m/s²",
            (double)*accel_x, (double)*accel_y, (double)*accel_z);
    return 0;

#else
    /* REAL SPI SENSOR example (ADXL345-like) */
    uint8_t tx_buf[7] = {0x80 | 0x32, 0, 0, 0, 0, 0, 0};
    uint8_t rx_buf[7] = {0};

    const struct spi_buf tx = {.buf = tx_buf, .len = sizeof(tx_buf)};
    const struct spi_buf rx = {.buf = rx_buf, .len = sizeof(rx_buf)};
    const struct spi_buf_set tx_set = {.buffers = &tx, .count = 1};
    const struct spi_buf_set rx_set = {.buffers = &rx, .count = 1};

    int ret = spi_transceive(spi_dev, &spi_cfg, &tx_set, &rx_set);
    if (ret < 0) {
        LOG_ERR("SPI read failed: %d", ret);
        return ret;
    }

    int16_t raw_x = (rx_buf[2] << 8) | rx_buf[1];
    int16_t raw_y = (rx_buf[4] << 8) | rx_buf[3];
    int16_t raw_z = (rx_buf[6] << 8) | rx_buf[5];

    *accel_x = raw_x * 0.0039f * 9.81f;
    *accel_y = raw_y * 0.0039f * 9.81f;
    *accel_z = raw_z * 0.0039f * 9.81f;

    return 0;
#endif
}
