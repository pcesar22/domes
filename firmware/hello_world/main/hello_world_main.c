#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    int count = 0;
    while (1) {
        printf("Hello World! Count: %d\n", count++);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
