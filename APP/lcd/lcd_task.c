#include "lcd_task.h"
#include "lcd.h"

#include <stdio.h>

void lcd_disp(void *argument)
{
    LCD_Init();      // LCD 硬件初始化
    printf("LCD + LVGL Init Done!\r\n");

    for (;;)
    {

    }
}
