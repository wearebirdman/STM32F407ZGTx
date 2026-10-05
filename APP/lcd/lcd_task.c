#include "lcd_task.h"
#include "lcd.h"

#include <stdio.h>

void lcd_disp(void *argument)
{
    LCD_Init();   // LCD 硬件初始化
    printf("LCD Init Done!\r\n");
    LCD_ShowString(50, 50, 100, 20, 16, "Hello LCD!", WHITE, BLACK);

    for(;;)
    {
        osDelay(100);
    }
}
