/**
 * @file adc_battery.c
 * @brief ADC battery voltage monitor (stub mode - no DT dependency)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(adc_battery, LOG_LEVEL_DBG);

int adc_battery_init(void)
{
    LOG_INF("ADC battery sensor initialized (stub mode)");
    return 0;
}

int adc_battery_read(float *voltage_v)
{
    if (!voltage_v) {
        return -EINVAL;
    }
    
    /* Generate simulated voltage between 3.3V and 4.2V */
    *voltage_v = 3.3f + ((float)(sys_rand32_get() % 90) / 100.0f);
    LOG_DBG("Battery voltage (stub): %.2fV", (double)*voltage_v);
    
    return 0;
}
