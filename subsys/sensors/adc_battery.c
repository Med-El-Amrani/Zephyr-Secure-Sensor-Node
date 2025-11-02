/**
 * @file adc_battery.c
 * @brief ADC-based battery voltage monitoring
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(adc_battery, LOG_LEVEL_DBG);

/* ADC configuration */
#define ADC_DEV_NODE DT_NODELABEL(adc0)
#define ADC_CHANNEL 0
#define ADC_RESOLUTION 12
#define ADC_GAIN ADC_GAIN_1
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT

/* Voltage divider configuration (adjust for your hardware) */
#define VOLTAGE_DIVIDER_RATIO 2.0f  /* R1/(R1+R2) if using divider */
#define ADC_REF_VOLTAGE 3.3f        /* Reference voltage in volts */

static const struct device *adc_dev;
static struct adc_channel_cfg channel_cfg = {
    .gain = ADC_GAIN,
    .reference = ADC_REFERENCE,
    .acquisition_time = ADC_ACQUISITION_TIME,
    .channel_id = ADC_CHANNEL,
};

static int16_t adc_sample_buffer;
static struct adc_sequence sequence = {
    .channels = BIT(ADC_CHANNEL),
    .buffer = &adc_sample_buffer,
    .buffer_size = sizeof(adc_sample_buffer),
    .resolution = ADC_RESOLUTION,
};

/**
 * @brief Initialize ADC for battery voltage monitoring
 */
int adc_battery_init(void)
{
    adc_dev = DEVICE_DT_GET(ADC_DEV_NODE);
    
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }
    
    int ret = adc_channel_setup(adc_dev, &channel_cfg);
    if (ret != 0) {
        LOG_ERR("Failed to setup ADC channel: %d", ret);
        return ret;
    }
    
    LOG_INF("ADC battery monitor initialized");
    return 0;
}

/**
 * @brief Read battery voltage from ADC
 * @param voltage_v Pointer to store battery voltage in volts
 * @return 0 on success, negative errno on failure
 */
int adc_battery_read(float *voltage_v)
{
    if (voltage_v == NULL) {
        return -EINVAL;
    }
    
    /* STUB: Simulate battery voltage reading */
    #if defined(CONFIG_ADC_BATTERY_STUB)
    /* Generate simulated voltage between 3.3V and 4.2V */
    *voltage_v = 3.3f + ((float)(sys_rand32_get() % 90) / 100.0f);
    LOG_DBG("Battery voltage (stub): %.2fV", (double)*voltage_v);
    #else
    int ret = adc_read(adc_dev, &sequence);
    if (ret != 0) {
        LOG_ERR("Failed to read ADC: %d", ret);
        return ret;
    }
    
    /* Convert ADC value to voltage */
    int32_t raw_value = adc_sample_buffer;
    float adc_voltage = (float)raw_value * ADC_REF_VOLTAGE / ((1 << ADC_RESOLUTION) - 1);
    
    /* Apply voltage divider ratio if used */
    *voltage_v = adc_voltage * VOLTAGE_DIVIDER_RATIO;
    
    LOG_DBG("Battery voltage: %.2fV (raw: %d)", (double)*voltage_v, raw_value);
    #endif
    
    return 0;
}