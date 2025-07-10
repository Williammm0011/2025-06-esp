#include <stdio.h>
#include "driver/spi_master.h"
#include "driver/timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define PIN_NUM_CLK 18
#define PIN_NUM_MOSI 23
#define PIN_NUM_CS 5

#define INIT_POS_X 3
#define INIT_POS_Y 4
#define END_POS_X 7
#define END_POS_Y 2
#define INIT_VEL_X -1
#define INIT_VEL_Y -1
#define FPS 15

static const char *TAG = "main";
int frameCnt = 0;
bool gameover = false;

uint8_t posX = INIT_POS_X;
uint8_t posY = INIT_POS_Y;
uint8_t velX = INIT_VEL_X;
uint8_t velY = INIT_VEL_Y;

spi_device_handle_t spi_handle;

spi_bus_config_t buscfg = {
    .mosi_io_num = PIN_NUM_MOSI,
    .miso_io_num = -1,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 0};

spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 1 * 1000 * 1000, // 1 MHz
    .mode = 0,                         // SPI Mode 0
    .spics_io_num = PIN_NUM_CS,
    .queue_size = 1};

timer_config_t config = {
    .divider = 80,
    .counter_dir = TIMER_COUNT_UP,
    .counter_en = TIMER_PAUSE,
    .alarm_en = TIMER_ALARM_EN,
    .auto_reload = true};

volatile bool update_flag = false;

bool IRAM_ATTR timer_isr_callback(void *arg)
{
    update_flag = true;
    return true;
}

void send_cmd(uint8_t addr, uint8_t data)
{
    uint8_t buffer[2] = {addr, data};

    spi_transaction_t t = {0};
    t.length = 16; // bits
    t.tx_buffer = buffer;

    spi_device_transmit(spi_handle, &t);
}

void spi_init()
{

    // Initialization sequence (run in app_main):
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);
    spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);

    send_cmd(0x09, 0x00);
    send_cmd(0x0A, 0x08);
    send_cmd(0x0B, 0x07);
    send_cmd(0x0C, 0x01);
    send_cmd(0x0F, 0x00);
}

uint8_t pattern[8] = {
    0b00000001,
    0b00000001,
    0b00000001,
    0b00000001,
    0b00000001,
    0b00000001,
    0b00000001,
    0b00000001,
};

void calculate_pattern(void *arg)
{
    if (posX == 1 || posX == 7)
        velX = -velX;
    if (posY == 0 || posY == 7)
        velY = -velY;

    posX += velX;
    posY += velY;

    for (int row = 0; row < 8; row++)
    {
        if (row == 7 - posY)
        {
            pattern[row] = (0b00000001) + (1 << posX);
        }
        else
        {
            pattern[row] = (0b00000001);
        }
    }
    if (posX == END_POS_X && posY == END_POS_Y)
    {
        gameover = true;
    }

    return;
}

void show_pattern(void *arg)
{
    while (!gameover)
    {
        if (update_flag)
        {
            update_flag = false;
            calculate_pattern(NULL);
            for (int row = 0; row < 8; row++)
            {
                send_cmd(row + 1, pattern[row]);
            }
        }
        ESP_LOGI(TAG, "%d, %d", posX, posY);
        vTaskDelay(pdMS_TO_TICKS(10));
        frameCnt++;
    }
    for (int row = 0; row < 8; row++)
    {
        send_cmd(row + 1, 0);
    }

    ESP_LOGI(TAG, "Frame Count: %d", frameCnt);
    vTaskDelete(NULL);
}

void app_main(void)
{
    spi_init();

    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1000000 / FPS); // 1 tick = 1us
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_isr_callback, NULL, 0);
    timer_start(TIMER_GROUP_0, TIMER_0);

    xTaskCreate(show_pattern, "show_pattern", 2056, NULL, 5, NULL);
}
