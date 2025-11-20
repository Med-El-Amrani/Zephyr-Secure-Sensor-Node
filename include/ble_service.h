#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <stdbool.h>
#include "sensor_manager.h"

int ble_service_init(void);
int ble_service_start_advertising(void);
int ble_service_stop_advertising(void);
int ble_service_notify(const sensor_data_t *data);
bool ble_service_is_connected(void);

#endif
