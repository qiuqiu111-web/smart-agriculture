#ifndef QUEUES_H
#define QUEUES_H

#include "FreeRTOS.h"
#include "queue.h"
#include "manager.h"
#include "frame_codec.h"

extern QueueHandle_t sensorsDataQueue;
extern QueueHandle_t commandQueue;

int QueueInit(void);

#endif // QUEUES_H
