#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "sensor_manager.h"

int app_mqtt_client_init(void);
int mqtt_client_connect(void);
void app_mqtt_disconnect(void);
int mqtt_client_publish_sensor_data(const sensor_data_t *data);
bool mqtt_client_is_connected(void);
void mqtt_client_process(void);

#endif
