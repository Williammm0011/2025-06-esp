#include "esp_wifi.h"  // Wi-Fi functions
#include "esp_event.h" // Event loop (required for Wi-Fi)
#include "esp_log.h"   // Logging macros
#include "nvs_flash.h" // NVS storage (required for Wi-Fi credentials)

#include <sys/socket.h> // Core socket functions
#include <netinet/in.h> // sockaddr_in struct and INADDR_ANY
#include <string.h>
#include <arpa/inet.h> // inet_pton and address manipulation

// #include "esp_timer.h"
#include <stdio.h>

void wifi_init_sta()
{
    return;
    esp_netif_init();                    // Initialize TCP/IP network interface (mandatory)
    esp_event_loop_create_default();     // Create default event loop
    esp_netif_create_default_wifi_sta(); // Create default Wi-Fi station interface

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg); // Initialize Wi-Fi with default config

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "your_ssid",        // Replace with actual SSID
            .password = "your_password" // Replace with actual password
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);               // Set station mode
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config); // Apply config to STA interface
    esp_wifi_start();                               // Start Wi-Fi driver
    esp_wifi_connect();                             // Connect to the access point
}

void app_main(void)
{

    wifi_init_sta();

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Create IPv4 UDP socket

    // bind to a port
    struct sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;                // IPv4
    listen_addr.sin_port = htons(12345);             // Host-to-network byte order for port
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on any local IP

    bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)); // Bind socket to port

    // receive UDP broadcast
    char rx_buffer[256];
    struct sockaddr_in source_addr; // Will hold sender's info
    socklen_t socklen = sizeof(source_addr);

    int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                       (struct sockaddr *)&source_addr, &socklen); // Blocking receive

    if (len > 0)
    {
        rx_buffer[len] = 0; // Null-terminate string for safety
        printf("Received: %s\n", rx_buffer);
    }
}
