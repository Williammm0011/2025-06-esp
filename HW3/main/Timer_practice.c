// #include <stdio.h>
// #include "driver/timer.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// timer_config_t config = {
//     .divider = 80,
//     .counter_dir = TIMER_COUNT_UP,
//     .counter_en = TIMER_PAUSE,
//     .alarm_en = TIMER_ALARM_EN,
//     .auto_reload = true};

// volatile bool update_flag = false;
// bool IRAM_ATTR timer_isr_callback(void *arg)
// {
//     update_flag = true;
//     return true;
// }

// void time_counting(void *arg)
// {
//     static int cnt = 0;
//     while (1)
//     {
//         if (update_flag)
//         {
//             update_flag = false;
//             printf("cnt = %d\n", cnt++);
//         }
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }

// void app_main(void)
// {
//     timer_init(TIMER_GROUP_0, TIMER_0, &config);
//     timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
//     // timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 50000);
//     timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 1000000);
//     // 1 tick = 1us

//     timer_enable_intr(TIMER_GROUP_0, TIMER_0);
//     timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, timer_isr_callback, NULL, 0);
//     timer_start(TIMER_GROUP_0, TIMER_0);

//     xTaskCreate(time_counting, "time_counting", 2056, NULL, 5, NULL);
// }
