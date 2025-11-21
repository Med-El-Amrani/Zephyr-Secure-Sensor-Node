/**
 * @file mqtt_client.c - FINAL FIX
 * @brief MQTT client using NET_EVENT_IPV4_ADDR_ADD (like working test_wifi)
 */

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

#include "mqtt_client.h"
#include "app_config.h"

LOG_MODULE_REGISTER(app_mqtt, LOG_LEVEL_INF);

/* JSON encoder */
extern int json_encode_sensor_data_with_metadata(const sensor_data_t *data,
                                                 char *buffer,
                                                 size_t buffer_size,
                                                 const char *device_id);

/* MQTT client context */
static struct mqtt_client client;
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[512];

static struct mqtt_utf8 client_id_utf8;
static struct mqtt_utf8 pub_topic_utf8;

static struct sockaddr_storage broker;
static bool mqtt_connected = false;

/* WiFi - TWO SEPARATE CALLBACKS like working test_wifi */
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback ipv4_cb;  // ✅ SEPARATE callback for IPv4

static K_SEM_DEFINE(wifi_connected_sem, 0, 1);
static K_SEM_DEFINE(ipv4_assigned, 0, 1);

/* Forward declarations */
static int wifi_connect(void);

/**
 * @brief Handle IPv4 address assignment - COPIED FROM WORKING test_wifi
 */
static void handle_ipv4_result(struct net_if *iface)
{
    char ip_addr[NET_IPV4_ADDR_LEN];

    for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        struct net_if_addr *if_addr = &iface->config.ip.ipv4->unicast[i];

        if (if_addr->addr_type != NET_ADDR_DHCP || !if_addr->is_used) {
            continue;
        }
        
        if (net_addr_ntop(AF_INET, &if_addr->address.in_addr, ip_addr, sizeof(ip_addr))) {
            LOG_INF("========================================");
            LOG_INF("ESP32-S3 IP Address: %s", ip_addr);
            LOG_INF("========================================");
            k_sem_give(&ipv4_assigned);  // ✅ Signal IP is ready!
            return;
        }
    }
}

/**
 * @brief WiFi event handler
 */
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event,
                                   struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        {
            const struct wifi_status *status = (const struct wifi_status *)cb->info;
            if (status->status) {
                LOG_ERR("WiFi connection failed (status: %d)", status->status);
            } else {
                LOG_INF("WiFi connected successfully");
                k_sem_give(&wifi_connected_sem);
            }
        }
        break;

    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        LOG_WRN("WiFi disconnected");
        mqtt_connected = false;
        k_sem_reset(&wifi_connected_sem);
        k_sem_reset(&ipv4_assigned);
        break;

    default:
        break;
    }
}

/**
 * @brief IPv4 event handler - SEPARATE CALLBACK like working test_wifi
 */
static void ipv4_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                   uint64_t mgmt_event,
                                   struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {  // ✅ This event WORKS!
        handle_ipv4_result(iface);
    }
}

/**
 * @brief Connect to WiFi
 */
static int wifi_connect(void)
{
    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params params = {0};

    if (!iface) {
        LOG_ERR("No default network interface found");
        return -1;
    }

    params.ssid = WIFI_SSID;
    params.ssid_length = strlen(WIFI_SSID);
    params.psk = WIFI_PSK;
    params.psk_length = strlen(WIFI_PSK);
    params.channel = WIFI_CHANNEL_ANY;
    params.security = WIFI_SECURITY_TYPE_PSK;

    LOG_INF("Connecting to WiFi SSID: %s", WIFI_SSID);

    if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params))) {
        LOG_ERR("WiFi connection request failed");
        return -1;
    }

    // Wait for WiFi connection (optional, can skip to IP wait)
    // k_sem_take(&wifi_connected_sem, K_SECONDS(30));

    // ✅ Wait for IP address assignment
    LOG_INF("Waiting for IP address...");
    int ret = k_sem_take(&ipv4_assigned, K_SECONDS(30));
    if (ret != 0) {
        LOG_ERR("DHCP timeout - no IP address assigned");
        return -ETIMEDOUT;
    }

    LOG_INF("✓ WiFi connected with IP assigned!");
    return 0;
}

/* MQTT event handler */
static void mqtt_evt_handler(struct mqtt_client *mqtt,
                             const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result) {
            LOG_ERR("MQTT connect failed: %d", evt->result);
            mqtt_connected = false;
        } else {
            mqtt_connected = true;
            LOG_INF("✓ MQTT connected");
        }
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT disconnected");
        mqtt_connected = false;
        break;

    default:
        break;
    }
}

/* Resolve broker hostname */
static int broker_init(void)
{
    struct sockaddr_in *b = (struct sockaddr_in *)&broker;

    b->sin_family = AF_INET;
    b->sin_port = htons(MQTT_BROKER_PORT);

    struct zsock_addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct zsock_addrinfo *result;

    int ret = zsock_getaddrinfo(MQTT_BROKER_ADDR, NULL, &hints, &result);
    if (ret != 0) {
        LOG_ERR("DNS lookup failed: %d", ret);
        return -EINVAL;
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)result->ai_addr;
    b->sin_addr = addr->sin_addr;

    zsock_freeaddrinfo(result);
    return 0;
}

/* Reconnection thread */
static K_THREAD_STACK_DEFINE(reconnect_stack, 2048);
static struct k_thread reconnect_thread;

static void reconnect_thread_func(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);
    
    while (1) {
        k_sleep(K_SECONDS(30));
        
        if (!mqtt_connected) {
            LOG_WRN("MQTT disconnected, attempting reconnection...");
            
            int ret = wifi_connect();
            if (ret == 0) {
                ret = mqtt_client_connect();
                if (ret == 0) {
                    LOG_INF("✓ Reconnected to MQTT broker");
                }
            }
        }
    }
}

/**
 * @brief Initialize MQTT client
 */
int app_mqtt_client_init(void)
{
    LOG_INF("Initializing MQTT client...");

    /* ✅ Register WiFi events callback */
    net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
                                NET_EVENT_WIFI_CONNECT_RESULT |
                                NET_EVENT_WIFI_DISCONNECT_RESULT);
    
    /* ✅ Register IPv4 events callback - SEPARATE! */
    net_mgmt_init_event_callback(&ipv4_cb, ipv4_mgmt_event_handler,
                                NET_EVENT_IPV4_ADDR_ADD);  // ✅ This works!

    net_mgmt_add_event_callback(&wifi_cb);
    net_mgmt_add_event_callback(&ipv4_cb);

    /* Give system time to initialize */
    k_sleep(K_SECONDS(1));

    /* Connect to WiFi */
    int ret = wifi_connect();
    if (ret < 0) {
        return ret;
    }

    /* Setup broker address */
    ret = broker_init();
    if (ret < 0) return ret;

    /* Prepare client */
    mqtt_client_init(&client);

    client.broker = &broker;
    client.evt_cb = mqtt_evt_handler;
    client.protocol_version = MQTT_VERSION_3_1_1;

    client.tx_buf = tx_buffer;
    client.tx_buf_size = sizeof(tx_buffer);
    client.rx_buf = rx_buffer;
    client.rx_buf_size = sizeof(rx_buffer);

    /* Client ID */
    static char cid[32];
    snprintf(cid, sizeof(cid), "%s_%08x", MQTT_CLIENT_ID, sys_rand32_get());
    client_id_utf8.utf8 = (uint8_t *)cid;
    client_id_utf8.size = strlen(cid);
    client.client_id = client_id_utf8;

    /* Publish topic */
    pub_topic_utf8.utf8 = (uint8_t *)MQTT_PUB_TOPIC;
    pub_topic_utf8.size = strlen(MQTT_PUB_TOPIC);

    /* Start reconnection thread */
    k_thread_create(&reconnect_thread, reconnect_stack,
                    K_THREAD_STACK_SIZEOF(reconnect_stack),
                    reconnect_thread_func,
                    NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&reconnect_thread, "mqtt_reconnect");

    return 0;
}

int mqtt_client_connect(void)
{
    int ret = mqtt_connect(&client);
    if (ret < 0) {
        LOG_ERR("mqtt_connect failed: %d", ret);
        return ret;
    }

    /* Wait for connection */
    for (int i = 0; i < 50 && !mqtt_connected; i++) {
        mqtt_input(&client);
        k_msleep(100);
    }

    if (!mqtt_connected) {
        LOG_ERR("MQTT connection timeout");
        return -ETIMEDOUT;
    }

    return 0;
}

void app_mqtt_disconnect(void)
{
    if (mqtt_connected) {
        struct mqtt_disconnect_param disc_param = {0};
        mqtt_disconnect(&client, &disc_param);
        mqtt_connected = false;
        LOG_INF("MQTT disconnected");
    }
}

int mqtt_client_publish_sensor_data(const sensor_data_t *data)
{
    char payload[JSON_BUFFER_SIZE];
    int len = json_encode_sensor_data_with_metadata(data,
                                                    payload,
                                                    sizeof(payload),
                                                    MQTT_CLIENT_ID);
    if (len < 0) return len;

    struct mqtt_publish_param param = {
        .message.topic = pub_topic_utf8,
        .message.payload.data = payload,
        .message.payload.len = len,
        .message_id = sys_rand32_get() & 0xFFFF,
        .dup_flag = 0,
        .retain_flag = 0,
    };

    return mqtt_publish(&client, &param);
}

bool mqtt_client_is_connected(void)
{
    return mqtt_connected;
}

void mqtt_client_process(void)
{
    if (mqtt_connected) {
        mqtt_input(&client);
        mqtt_live(&client);
    }
}