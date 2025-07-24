#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_wifi.h"  // Wi-Fi functions
#include "esp_log.h"   // Logging macros
#include "nvs_flash.h" // NVS storage (required for Wi-Fi credentials)

#include <sys/socket.h> // Core socket functions
#include <netinet/in.h> // sockaddr_in struct and INADDR_ANY
#include <string.h>
#include <arpa/inet.h> // inet_pton and address manipulation

// #include "esp_timer.h"
#include <stdio.h>

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        printf("Wi-Fi disconnected. Retrying...\n");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        printf("Got IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta()
{

    // Initialize NVS (Non-Volatile Storage) - required for WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init(); // Initialize TCP/IP network interface (mandatory)
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta(); // Create default Wi-Fi station interface
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    esp_wifi_init(&cfg); // Initialize Wi-Fi with default config

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "hank_EXT",    // Replace with actual SSID
            .password = "23715019" // Replace with actual password
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);               // Set station mode
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config); // Apply config to STA interface
    esp_wifi_start();                               // Start Wi-Fi driver
}

void app_main(void)
{
    wifi_init_sta();
    printf("Waiting for Wi-Fi connection...\n");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    printf("✅ Wi-Fi connected successfully!\n");

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Create IPv4 UDP socket
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
    {
        perror("setsockopt failed");
        return;
    }

    // bind to a port
    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;                // IPv4
    listen_addr.sin_port = htons(12345);             // Host-to-network byte order for port
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on any local IP

    // bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)); // Bind socket to port
    if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
    {
        perror("bind failed");
        return;
    }
    else
    {
        printf("Bind succuessfully\n");
    }

    // receive UDP broadcast
    char rx_buffer[256];
    struct sockaddr_in source_addr; // Will hold sender's info
    socklen_t socklen = sizeof(source_addr);
    printf("Start waiting for UDP broadcast...\n");

    int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                       (struct sockaddr *)&source_addr, &socklen); // Blocking receive
    printf("len = %d\n", len);
    if (len > 0)
    {
        rx_buffer[len] = 0; // Null-terminate string for safety
        printf("Received: %s\n", rx_buffer);
    }

    // send ACK

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(3333);                         // Server ACK port
    inet_pton(AF_INET, "192.168.1.100", &dest_addr.sin_addr); // Convert IP string to binary form

    char *ack_msg = "{ \"id\": \"ESP32_A\", \"status\": \"ack\" }";
    sendto(sock, ack_msg, strlen(ack_msg), 0,
           (struct sockaddr *)&dest_addr, sizeof(dest_addr));
}
