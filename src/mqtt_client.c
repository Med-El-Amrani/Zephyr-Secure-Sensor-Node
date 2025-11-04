/**
 * @file mqtt_client.c
 * @brief MQTT client with TLS support for sensor data publishing
 */

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include "mqtt_client.h"
#include "app_config.h"

LOG_MODULE_REGISTER(mqtt_cli, LOG_LEVEL_INF);

/* External JSON encoder */
extern int json_encode_sensor_data_with_metadata(const sensor_data_t *data,
                                                 char *buffer,
                                                 size_t buffer_size,
                                                 const char *device_id);

/* MQTT client context */
static struct mqtt_client client_ctx;
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[512];
static struct mqtt_utf8 client_id_utf8;
static struct mqtt_utf8 pub_topic_utf8;

/* Network buffers */
static struct sockaddr_storage broker;
static bool mqtt_connected = false;

/* WiFi management */
static struct net_mgmt_event_callback wifi_cb;
static K_SEM_DEFINE(wifi_connected_sem, 0, 1);

/**
 * @brief WiFi event handler
 */
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint32_t mgmt_event,
                                   struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        LOG_INF("WiFi connected");
        k_sem_give(&wifi_connected_sem);
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        LOG_INF("WiFi disconnected");
        mqtt_connected = false;
        break;
    default:
        break;
    }
}

/**
 * @brief Connect to WiFi
 */
static int wifi_connect(void)
{
    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params params = {0};
    
    params.ssid = WIFI_SSID;
    params.ssid_length = strlen(WIFI_SSID);
    params.psk = WIFI_PSK;
    params.psk_length = strlen(WIFI_PSK);
    params.channel = WIFI_CHANNEL_ANY;
    params.security = WIFI_SECURITY_TYPE_PSK;
    params.band = WIFI_FREQ_BAND_2_4_GHZ;
    params.mfp = WIFI_MFP_OPTIONAL;
    
    LOG_INF("Connecting to WiFi SSID: %s", WIFI_SSID);
    
    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (ret) {
        LOG_ERR("WiFi connection request failed: %d", ret);
        return ret;
    }
    
    /* Wait for connection with timeout */
    ret = k_sem_take(&wifi_connected_sem, K_SECONDS(30));
    if (ret) {
        LOG_ERR("WiFi connection timeout");
        return -ETIMEDOUT;
    }
    
    LOG_INF("WiFi connected successfully");
    return 0;
}

/**
 * @brief MQTT event handler
 */
static void mqtt_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result != 0) {
            LOG_ERR("MQTT connection failed: %d", evt->result);
            mqtt_connected = false;
        } else {
            LOG_INF("MQTT connected");
            mqtt_connected = true;
        }
        break;
        
    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT disconnected: %d", evt->result);
        mqtt_connected = false;
        break;
        
    case MQTT_EVT_PUBACK:
        LOG_DBG("PUBLISH acknowledged");
        break;
        
    default:
        LOG_DBG("MQTT event: %d", evt->type);
        break;
    }
}

/**
 * @brief Initialize MQTT broker address
 */
static int broker_init(void)
{
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
    
    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(MQTT_BROKER_PORT);
    
    /* Resolve broker hostname to IP */
    struct zsock_addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct zsock_addrinfo *result;
    
    int ret = zsock_getaddrinfo(MQTT_BROKER_ADDR, NULL, &hints, &result);
    if (ret != 0) {
        LOG_ERR("Failed to resolve broker address: %d", ret);
        return -EINVAL;
    }
    
    struct sockaddr_in *addr = (struct sockaddr_in *)result->ai_addr;
    broker4->sin_addr = addr->sin_addr;
    
    zsock_freeaddrinfo(result);
    
    LOG_INF("Broker address resolved");
    return 0;
}

int mqtt_client_init(void)
{
    LOG_INF("Initializing MQTT client...");
    
    /* Register WiFi event handler */
    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
                                NET_EVENT_WIFI_CONNECT_RESULT |
                                NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);
    
    /* Connect to WiFi */
    int ret = wifi_connect();
    if (ret != 0) {
        LOG_ERR("Failed to connect to WiFi");
        return ret;
    }
    
    /* Initialize MQTT client */
    mqtt_client_init(&client_ctx);
    
    /* Configure client ID */
    static char client_id[32];
    snprintf(client_id, sizeof(client_id), "%s_%08x", 
            MQTT_CLIENT_ID, sys_rand32_get());
    client_id_utf8.utf8 = (uint8_t *)client_id;
    client_id_utf8.size = strlen(client_id);
    
    /* Configure publish topic */
    pub_topic_utf8.utf8 = (uint8_t *)MQTT_PUB_TOPIC;
    pub_topic_utf8.size = strlen(MQTT_PUB_TOPIC);
    
    /* Setup MQTT client */
    client_ctx.broker = &broker;
    client_ctx.evt_cb = mqtt_evt_handler;
    client_ctx.client_id = client_id_utf8;
    client_ctx.password = NULL;
    client_ctx.user_name = NULL;
    client_ctx.protocol_version = MQTT_VERSION_3_1_1;
    client_ctx.rx_buf = rx_buffer;
    client_ctx.rx_buf_size = sizeof(rx_buffer);
    client_ctx.tx_buf = tx_buffer;
    client_ctx.tx_buf_size = sizeof(tx_buffer);
    client_ctx.keepalive = MQTT_KEEPALIVE_SEC;
    
    /* Initialize broker address */
    ret = broker_init();
    if (ret != 0) {
        return ret;
    }
    
    LOG_INF("MQTT client initialized");
    return 0;
}

int mqtt_client_connect(void)
{
    if (mqtt_connected) {
        LOG_WRN("MQTT already connected");
        return 0;
    }
    
    LOG_INF("Connecting to MQTT broker %s:%d", MQTT_BROKER_ADDR, MQTT_BROKER_PORT);
    
    int ret = mqtt_connect(&client_ctx);
    if (ret) {
        LOG_ERR("MQTT connect failed: %d", ret);
        return ret;
    }
    
    /* Wait for connection */
    for (int i = 0; i < 50 && !mqtt_connected; i++) {
        mqtt_input(&client_ctx);
        k_msleep(100);
    }
    
    if (!mqtt_connected) {
        LOG_ERR("MQTT connection timeout");
        return -ETIMEDOUT;
    }
    
    return 0;
}

void mqtt_client_disconnect(void)
{
    if (mqtt_connected) {
        mqtt_disconnect(&client_ctx);
        mqtt_connected = false;
        LOG_INF("MQTT disconnected");
    }
}

int mqtt_client_publish_sensor_data(const sensor_data_t *data)
{
    if (!mqtt_connected) {
        LOG_WRN("MQTT not connected");
        return -ENOTCONN;
    }
    
    if (data == NULL) {
        return -EINVAL;
    }
    
    /* Encode sensor data to JSON */
    char payload[JSON_BUFFER_SIZE];
    int len = json_encode_sensor_data_with_metadata(data, payload, sizeof(payload),
                                                    MQTT_CLIENT_ID);
    if (len < 0) {
        LOG_ERR("Failed to encode JSON: %d", len);
        return len;
    }
    
    /* Prepare MQTT publish message */
    struct mqtt_publish_param param = {
        .message.topic = pub_topic_utf8,
        .message.payload.data = payload,
        .message.payload.len = len,
        .message_id = sys_rand32_get() & 0xFFFF,
        .dup_flag = 0,
        .retain_flag = 0,
        .qos = MQTT_QOS,
    };
    
    LOG_INF("Publishing to MQTT: %s", MQTT_PUB_TOPIC);
    
    int ret = mqtt_publish(&client_ctx, &param);
    if (ret) {
        LOG_ERR("MQTT publish failed: %d", ret);
        return ret;
    }
    
    LOG_DBG("Published %d bytes to MQTT", len);
    return 0;
}

bool mqtt_client_is_connected(void)
{
    return mqtt_connected;
}

int mqtt_client_process(void)
{
    if (mqtt_connected) {
        return mqtt_input(&client_ctx);
    }
    return 0;
}