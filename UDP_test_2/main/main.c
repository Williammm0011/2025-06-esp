#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "cJSON.h"

#define WIFI_SSID "hank_EXT"
#define WIFI_PASS "23715019"
#define UDP_LISTEN_PORT 12345
#define ACK_PORT 3333
#define CLIENT_ID "ESP32_ABC123"

static const char *TAG = "udp_client";
static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;

//--------------------------------------------------
// Wi-Fi Event Handler
//--------------------------------------------------
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

//--------------------------------------------------
// Initialize Wi-Fi in Station Mode
//--------------------------------------------------
static void wifi_init_sta(void)
{
    // 1) NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2) TCP/IP, Event loop, default STA
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3) Register events
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &wifi_event_handler,
                                            NULL, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            &wifi_event_handler,
                                            NULL, NULL));

    // 4) Init Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 5) Configure SSID/Password
    wifi_config_t wifi_conf = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_conf));

    // 6) Start
    ESP_ERROR_CHECK(esp_wifi_start());
}

//--------------------------------------------------
// Main Application
//--------------------------------------------------
void app_main(void)
{
    // Connect to Wi-Fi
    wifi_init_sta();
    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    xEventGroupWaitBits(wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        false, true,
                        portMAX_DELAY);
    ESP_LOGI(TAG, "✅ Wi-Fi connected!");

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        return;
    }

    // Enable broadcast reception
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0)
    {
        ESP_LOGE(TAG, "setsockopt SO_BROADCAST failed: errno %d", errno);
        close(sock);
        return;
    }

    // Bind to UDP_LISTEN_PORT
    struct sockaddr_in listen_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(UDP_LISTEN_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)};

    if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
    {
        ESP_LOGE(TAG, "bind() failed: errno %d", errno);
        close(sock);
        return;
    }
    ESP_LOGI(TAG, "Socket bound on port %d", UDP_LISTEN_PORT);

    // Receive loop
    while (true)
    {
        ESP_LOGI(TAG, "Waiting for UDP broadcast...");
        struct sockaddr_in src_addr;
        socklen_t socklen = sizeof(src_addr);
        char rxbuf[256];

        int len = recvfrom(sock, rxbuf, sizeof(rxbuf) - 1,
                           0, (struct sockaddr *)&src_addr, &socklen);
        if (len < 0)
        {
            ESP_LOGE(TAG, "recvfrom() failed: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        rxbuf[len] = '\0';
        ESP_LOGI(TAG, "Received %d bytes: %s", len, rxbuf);

        // Parse JSON
        cJSON *root = cJSON_Parse(rxbuf);
        if (!root)
        {
            ESP_LOGW(TAG, "Invalid JSON");
            continue;
        }
        cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
        if (!cJSON_IsString(cmd))
        {
            cJSON_Delete(root);
            continue;
        }

        // Handle "start"
        if (strcmp(cmd->valuestring, "start") == 0)
        {
            cJSON *seq_item = cJSON_GetObjectItem(root, "seq");
            cJSON *delay_item = cJSON_GetObjectItem(root, "delay_ms");
            if (!cJSON_IsNumber(seq_item) || !cJSON_IsNumber(delay_item))
            {
                ESP_LOGW(TAG, "Missing seq or delay_ms");
                cJSON_Delete(root);
                continue;
            }
            int seq = seq_item->valueint;
            int delay_ms = delay_item->valueint;

            // Send ACK back to server
            int ack_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (ack_sock < 0)
            {
                ESP_LOGE(TAG, "ACK socket() failed: errno %d", errno);
            }
            else
            {
                struct sockaddr_in ack_addr = {
                    .sin_family = AF_INET,
                    .sin_port = htons(ACK_PORT),
                    .sin_addr = src_addr.sin_addr, // server IP
                };
                cJSON *ack = cJSON_CreateObject();
                cJSON_AddStringToObject(ack, "id", CLIENT_ID);
                cJSON_AddStringToObject(ack, "status", "ack");
                cJSON_AddNumberToObject(ack, "seq", seq);
                char *ack_str = cJSON_PrintUnformatted(ack);
                int sent = sendto(ack_sock, ack_str, strlen(ack_str), 0,
                                  (struct sockaddr *)&ack_addr, sizeof(ack_addr));
                if (sent < 0)
                {
                    ESP_LOGE(TAG, "ACK sendto() failed: errno %d", errno);
                }
                else
                {
                    ESP_LOGI(TAG, "ACK sent: %s", ack_str);
                }
                cJSON_Delete(ack);
                free(ack_str);
                close(ack_sock);
            }

            // Wait the instructed delay
            ESP_LOGI(TAG, "Delaying %d ms before start…", delay_ms);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            ESP_LOGI(TAG, "=== TASK START ===");

            // Handle "stop"
        }
        else if (strcmp(cmd->valuestring, "stop") == 0)
        {
            ESP_LOGW(TAG, "Received STOP command — abort countdown");
        }

        cJSON_Delete(root);
    }

    // Cleanup (never reached in this loop example)
    close(sock);
}