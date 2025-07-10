// #include <stdio.h>
// #include "driver/spi_master.h"
// #define PIN_NUM_CLK 18
// #define PIN_NUM_MOSI 23
// #define PIN_NUM_CS 5

// spi_device_handle_t spi_handle;

// spi_bus_config_t buscfg = {
//     .mosi_io_num = PIN_NUM_MOSI,
//     .miso_io_num = -1,
//     .sclk_io_num = PIN_NUM_CLK,
//     .quadwp_io_num = -1,
//     .quadhd_io_num = -1,
//     .max_transfer_sz = 0};

// spi_device_interface_config_t devcfg = {
//     .clock_speed_hz = 1 * 1000 * 1000, // 1 MHz
//     .mode = 0,                         // SPI Mode 0
//     .spics_io_num = PIN_NUM_CS,
//     .queue_size = 1};

// uint8_t pattern[8] = {
//     0b00010000,
//     0b00010000,
//     0b00010000,
//     0b00010000,
//     0b00001000,
//     0b00001000,
//     0b00001000,
//     0b00001000,
// };

// void send_cmd(uint8_t addr, uint8_t data)
// {
//     uint8_t buffer[2] = {addr, data};

//     spi_transaction_t t = {0};
//     t.length = 16; // bits
//     t.tx_buffer = buffer;

//     spi_device_transmit(spi_handle, &t);
// }

// void spi_init()
// {

//     // Initialization sequence (run in app_main):
//     spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);
//     spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);

//     send_cmd(0x09, 0x00);
//     send_cmd(0x0A, 0x08);
//     send_cmd(0x0B, 0x07);
//     send_cmd(0x0C, 0x01);
//     send_cmd(0x0F, 0x00);
// }

// void show_pattern()
// {
//     for (int row = 0; row < 8; row++)
//     {
//         send_cmd(row + 1, pattern[row]);
//     }
// }

// void app_main(void)
// {
//     spi_init();
//     show_pattern();
// }