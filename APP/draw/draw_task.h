#ifndef __DRAW_TASK_H
#define __DRAW_TASK_H

#include "main.h"
#include "cmsis_os.h"

/* 画板会话触发信号量（定义于 freertos.c）：home 让屏后 release */
extern osSemaphoreId_t sem_drawHandle;

void draw_proc(void *argument);

#endif /* __DRAW_TASK_H */
