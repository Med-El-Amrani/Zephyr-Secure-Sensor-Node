/**
 * @file power_manager.h
 * @brief Power management for low-power operation
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <zephyr/kernel.h>

/* Power states */
typedef enum {
    POWER_STATE_ACTIVE,
    POWER_STATE_IDLE,
    POWER_STATE_SLEEP,
    POWER_STATE_DEEP_SLEEP
} power_state_t;

/**
 * @brief Initialize power management subsystem
 * @return 0 on success, negative errno on failure
 */
int power_manager_init(void);

/**
 * @brief Enter low-power mode
 * @param state Desired power state
 * @param duration_ms Duration to stay in low-power mode (0 for indefinite)
 * @return 0 on success, negative errno on failure
 */
int power_manager_enter_low_power(power_state_t state, uint32_t duration_ms);

/**
 * @brief Exit low-power mode and return to active state
 */
void power_manager_exit_low_power(void);

/**
 * @brief Get current power state
 * @return Current power state
 */
power_state_t power_manager_get_state(void);

/**
 * @brief Setup watchdog timer
 * @param timeout_ms Watchdog timeout in milliseconds
 * @return 0 on success, negative errno on failure
 */
int power_manager_setup_watchdog(uint32_t timeout_ms);

/**
 * @brief Feed the watchdog timer
 */
void power_manager_feed_watchdog(void);

#endif /* POWER_MANAGER_H */