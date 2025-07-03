#include <stdio.h>
#include <esp_log.h>
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG = "HW2";
int M1[4][4] = {{1, 2, 3, 4}, {1, 0, 0, 1}, {1, 2, 0, 0}, {3, 2, 1, 0}};
int M2[4][4] = {{1, 2, 4, 3}, {1, 1, 0, 1}, {1, 0, 2, 0}, {0, 1, 1, 0}};

int M3[4][4] = {0};

SemaphoreHandle_t sum_mutex;
SemaphoreHandle_t mul_done;

int sum;

void task_A()
{
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                M3[i][j] += M1[i][k] * M2[k][j];

    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 4; j++)
        {
            xSemaphoreTake(sum_mutex, portMAX_DELAY);
            ESP_LOGI(TAG, "A start");
            sum += M3[i][j];
            ESP_LOGI(TAG, "A end");
            xSemaphoreGive(sum_mutex);
        }
    ESP_LOGI(TAG, "After task A, the sum is =%d", sum);
    vTaskDelete(NULL);
}

void task_B()
{
    for (int i = 2; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                M3[i][j] += M1[i][k] * M2[k][j];
    for (int i = 2; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            xSemaphoreTake(sum_mutex, portMAX_DELAY);
            ESP_LOGI(TAG, "B start");
            sum += M3[i][j];
            ESP_LOGI(TAG, "B end");
            xSemaphoreGive(sum_mutex);
        }
    ESP_LOGI(TAG, "After task B, the sum is =%d", sum);
    vTaskDelete(NULL);
}

void app_main(void)
{
    sum_mutex = xSemaphoreCreateMutex();
    xTaskCreate(task_A, "task_A", 2048, NULL, 5, NULL);
    xTaskCreate(task_B, "task_B", 2048, NULL, 5, NULL);
}