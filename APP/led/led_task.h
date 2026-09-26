#ifndef __LED_TASK_H
#define __LED_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

void led_proc(void *argument);

#endif
