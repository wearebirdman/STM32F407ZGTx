#ifndef __CALIB_TASK_H
#define __CALIB_TASK_H

#include "main.h"
#include "cmsis_os.h"

/* 校准触发信号量（定义于 freertos.c）：
 * 二进制、初值 0；key_task 等触发方 osSemaphoreRelease，
 * calib_task 阻塞在 osSemaphoreAcquire 上，零轮询开销。 */
extern osSemaphoreId_t sem_calibHandle;

void calib_proc(void *argument);

#endif /* __CALIB_TASK_H */
