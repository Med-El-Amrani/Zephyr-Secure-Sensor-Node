/**
 * @file power_manager.c
 * @brief Power management for low-power operation
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>
#include "power_manager.h"
#include "app_config.h"

LOG_MODULE_REGISTER(power_mgr, LOG_LEVEL_INF);

/* Watchdog device */
static const struct device *wdt_dev = DEVICE_DT_GET(DT_NODELABEL(wdt0));
static int wdt_channel_id = -1;

/* Power state */
static power_state_t current_state = POWER_STATE_ACTIVE;

int power_manager_init(void)
{
    LOG_INF("Initializing power manager...");
    
#ifdef CONFIG_PM
    /* Enable power management */
    LOG_INF("Power management enabled");
#else
    LOG_WRN("Power management not configured");
#endif
    
    LOG_INF("Power manager initialized");
    return 0;
}

int power_manager_enter_low_power(power_state_t state, uint32_t duration_ms)
{
    if (state == POWER_STATE_ACTIVE) {
        LOG_WRN("Cannot enter active state via low-power function");
        return -EINVAL;
    }
    
    LOG_INF("Entering low-power mode: %d for %u ms", state, duration_ms);
    
    current_state = state;
    
    switch (state) {
    case POWER_STATE_IDLE:
        /* CPU idle, peripherals active */
        if (duration_ms > 0) {
            k_msleep(duration_ms);
        } else {
            k_yield();
        }
        break;
        
    case POWER_STATE_SLEEP:
        /* Light sleep with quick wake-up */
#ifdef CONFIG_PM
        if (duration_ms > 0) {
            k_msleep(duration_ms);  /* PM subsystem handles state */
        }
#else
        k_msleep(duration_ms);
#endif
        break;
        
    case POWER_STATE_DEEP_SLEEP:
        /* Deep sleep, slow wake-up */
#ifdef CONFIG_PM
        LOG_INF("Entering deep sleep for %u ms", duration_ms);
        if (duration_ms > 0) {
            k_msleep(duration_ms);  /* PM subsystem handles deep sleep */
        }
#else
        LOG_WRN("Deep sleep not available without PM");
        k_msleep(duration_ms);
#endif
        break;
        
    default:
        LOG_ERR("Invalid power state: %d", state);
        return -EINVAL;
    }
    
    current_state = POWER_STATE_ACTIVE;
    LOG_DBG("Exited low-power mode");
    
    return 0;
}

void power_manager_exit_low_power(void)
{
    if (current_state != POWER_STATE_ACTIVE) {
        LOG_INF("Forcing exit from low-power mode");
        current_state = POWER_STATE_ACTIVE;
    }
}

power_state_t power_manager_get_state(void)
{
    return current_state;
}

int power_manager_setup_watchdog(uint32_t timeout_ms)
{
#ifdef CONFIG_WATCHDOG
    if (!device_is_ready(wdt_dev)) {
        LOG_ERR("Watchdog device not ready");
        return -ENODEV;
    }
    
    struct wdt_timeout_cfg wdt_config = {
        .flags = WDT_FLAG_RESET_SOC,
        .window.min = 0,
        .window.max = timeout_ms,
        .callback = NULL,
    };
    
    wdt_channel_id = wdt_install_timeout(wdt_dev, &wdt_config);
    if (wdt_channel_id < 0) {
        LOG_ERR("Failed to install watchdog timeout: %d", wdt_channel_id);
        return wdt_channel_id;
    }
    
    int ret = wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
    if (ret < 0) {
        LOG_ERR("Failed to setup watchdog: %d", ret);
        return ret;
    }
    
    LOG_INF("Watchdog configured with %u ms timeout", timeout_ms);
    return 0;
#else
    LOG_WRN("Watchdog not configured in build");
    return -ENOTSUP;
#endif
}

void power_manager_feed_watchdog(void)
{
#ifdef CONFIG_WATCHDOG
    if (wdt_channel_id >= 0) {
        wdt_feed(wdt_dev, wdt_channel_id);
        LOG_DBG("Watchdog fed");
    }
#endif
}