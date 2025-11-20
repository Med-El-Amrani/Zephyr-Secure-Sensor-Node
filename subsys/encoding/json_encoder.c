/**
 * @file json_encoder.c
 * @brief JSON encoding for sensor data payloads
 */

#include <zephyr/kernel.h>
#include <zephyr/data/json.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>
#include "sensor_manager.h"
#include "app_config.h"

LOG_MODULE_REGISTER(json_encoder, LOG_LEVEL_DBG);

/* JSON descriptor for sensor data */
static const struct json_obj_descr sensor_data_descr[] = {
    JSON_OBJ_DESCR_PRIM(sensor_data_t, temperature_c, JSON_TOK_FLOAT),
    JSON_OBJ_DESCR_PRIM(sensor_data_t, accel_x, JSON_TOK_FLOAT),
    JSON_OBJ_DESCR_PRIM(sensor_data_t, accel_y, JSON_TOK_FLOAT),
    JSON_OBJ_DESCR_PRIM(sensor_data_t, accel_z, JSON_TOK_FLOAT),
    JSON_OBJ_DESCR_PRIM(sensor_data_t, battery_voltage, JSON_TOK_FLOAT),
    JSON_OBJ_DESCR_PRIM(sensor_data_t, timestamp_ms, JSON_TOK_NUMBER),
};

/**
 * @brief Encode sensor data to JSON string
 * @param data Sensor data to encode
 * @param buffer Output buffer for JSON string
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or negative errno on failure
 */
int json_encode_sensor_data(const sensor_data_t *data, char *buffer, size_t buffer_size)
{
    if (data == NULL || buffer == NULL || buffer_size == 0) {
        return -EINVAL;
    }
    
    if (!data->valid) {
        LOG_WRN("Encoding invalid sensor data");
        return -EINVAL;
    }
    
    /* Use Zephyr's JSON library for encoding */
    int ret = json_obj_encode_buf(sensor_data_descr, ARRAY_SIZE(sensor_data_descr),
                                   data, buffer, buffer_size);
    
    if (ret < 0) {
        LOG_ERR("Failed to encode JSON: %d", ret);
        return ret;
    }
    
    LOG_DBG("Encoded JSON (%d bytes): %s", ret, buffer);
    return ret;
}

/**
 * @brief Encode sensor data to JSON string with metadata
 * @param data Sensor data to encode
 * @param buffer Output buffer for JSON string
 * @param buffer_size Size of output buffer
 * @param device_id Device identifier string
 * @return Number of bytes written, or negative errno on failure
 */
int json_encode_sensor_data_with_metadata(const sensor_data_t *data, 
                                          char *buffer, 
                                          size_t buffer_size,
                                          const char *device_id)
{
    if (data == NULL || buffer == NULL || buffer_size == 0) {
        return -EINVAL;
    }
    
    if (!data->valid) {
        LOG_WRN("Encoding invalid sensor data");
        return -EINVAL;
    }
    
    /* Manual JSON construction with metadata */
    int ret = snprintf(buffer, buffer_size,
        "{"
        "\"device_id\":\"%s\","
        "\"version\":\"%d.%d.%d\","
        "\"timestamp\":%u,"
        "\"sensors\":{"
            "\"temperature\":%.2f,"
            "\"accelerometer\":{"
                "\"x\":%.3f,"
                "\"y\":%.3f,"
                "\"z\":%.3f"
            "},"
            "\"battery\":%.2f"
        "}"
        "}",
        device_id ? device_id : "unknown",
        APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH,
        data->timestamp_ms,
        (double)data->temperature_c,
        (double)data->accel_x,
        (double)data->accel_y,
        (double)data->accel_z,
        (double)data->battery_voltage
    );
    
    if (ret < 0 || ret >= buffer_size) {
        LOG_ERR("Failed to format JSON");
        return -ENOMEM;
    }
    
    LOG_DBG("Encoded JSON with metadata (%d bytes)", ret);
    return ret;
}

/* Extended JSON encoder with metadata */
