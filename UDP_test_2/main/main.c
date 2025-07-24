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

#define WIFI_SSID "hank"
#define WIFI_PASS "23715019"
#define UDP_LISTEN_PORT 12345
#define ACK_PORT 3333
#define CLIENT_ID "ESP32_ABC123"

static const char *TAG = "udp_client";
static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static bool start_processed = false; // ← ignore further starts once first seen

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
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying…");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

//--------------------------------------------------
// Initialize Wi-Fi in STA Mode
//--------------------------------------------------
static void wifi_init_sta(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_conf = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_conf));
    ESP_ERROR_CHECK(esp_wifi_start());
}

//--------------------------------------------------
// Main Task
//--------------------------------------------------
void app_main(void)
{
    // 1) Connect to Wi-Fi
    wifi_init_sta();
    ESP_LOGI(TAG, "Waiting for Wi-Fi…");
    xEventGroupWaitBits(wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        false, true,
                        portMAX_DELAY);
    ESP_LOGI(TAG, "✅ Wi-Fi ready");

    // 2) Create & bind UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        return;
    }
    // allow broadcast
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

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
    ESP_LOGI(TAG, "Listening on UDP port %d", UDP_LISTEN_PORT);

    // 3) Receive loop
    while (true)
    {
        ESP_LOGI(TAG, "Awaiting broadcast…");
        struct sockaddr_in src;
        socklen_t len_src = sizeof(src);
        char buf[256];
        int len = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                           (struct sockaddr *)&src, &len_src);
        if (len < 0)
        {
            ESP_LOGE(TAG, "recvfrom() failed: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        buf[len] = '\0';
        ESP_LOGI(TAG, "From %s:%d → %s",
                 inet_ntoa(src.sin_addr),
                 ntohs(src.sin_port),
                 buf);

        // parse JSON
        cJSON *root = cJSON_Parse(buf);
        if (!root)
        {
            ESP_LOGW(TAG, "Bad JSON");
            continue;
        }
        cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
        if (!cJSON_IsString(cmd_item))
        {
            cJSON_Delete(root);
            continue;
        }
        const char *cmd = cmd_item->valuestring;

        // --- START handling ---
        if (strcmp(cmd, "start") == 0)
        {
            if (!start_processed)
            {
                // parse seq & delay
                cJSON *seq_it = cJSON_GetObjectItem(root, "seq");
                cJSON *delay_it = cJSON_GetObjectItem(root, "delay_ms");
                if (cJSON_IsNumber(seq_it) && cJSON_IsNumber(delay_it))
                {
                    int seq = seq_it->valueint;
                    int delay_ms = delay_it->valueint;
                    ESP_LOGI(TAG, "START[%d] → delay %d ms", seq, delay_ms);

                    // send ACK back
                    int ack_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    if (ack_sock >= 0)
                    {
                        struct sockaddr_in ack_addr = {
                            .sin_family = AF_INET,
                            .sin_port = htons(ACK_PORT),
                            .sin_addr = src.sin_addr};
                        cJSON *ack = cJSON_CreateObject();
                        cJSON_AddStringToObject(ack, "id", CLIENT_ID);
                        cJSON_AddStringToObject(ack, "status", "ack");
                        cJSON_AddNumberToObject(ack, "seq", seq);
                        char *ack_s = cJSON_PrintUnformatted(ack);
                        sendto(ack_sock, ack_s, strlen(ack_s), 0,
                               (struct sockaddr *)&ack_addr, sizeof(ack_addr));
                        ESP_LOGI(TAG, "ACK sent: %s", ack_s);
                        free(ack_s);
                        cJSON_Delete(ack);
                        close(ack_sock);
                    }

                    // countdown with STOP check
                    start_processed = true;
                    int remain = delay_ms;
                    while (remain > 0 && start_processed)
                    {
                        int step = (remain > 100) ? 100 : remain;
                        vTaskDelay(pdMS_TO_TICKS(step));
                        remain -= step;
                    }
                    if (start_processed)
                    {
                        ESP_LOGI(TAG, "=== TASK START ===");
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Countdown aborted by STOP");
                    }
                }
            }
            else
            {
                ESP_LOGI(TAG, "Ignoring extra START");
            }
        }
        // --- STOP handling (always open) ---
        else if (strcmp(cmd, "stop") == 0)
        {
            ESP_LOGW(TAG, "STOP received → cancelling start");
            start_processed = false;
        }

        cJSON_Delete(root);
    }

    // never reached
    close(sock);
}