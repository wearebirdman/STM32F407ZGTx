#ifndef __TOUCH_TASK_H
#define __TOUCH_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

void touch_proc(void *argument);

#endif /* __TOUCH_TASK_H */
