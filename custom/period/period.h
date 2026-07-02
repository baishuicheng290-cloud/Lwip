#ifndef PERIODIC_EVENT_TASK_H
#define PERIODIC_EVENT_TASK_H

#include <stdint.h>

typedef enum {
    EVENT_NONE = 0,
    EVENT_COM_MACHINE,
    EVENT_PERIOD_10MS,
    EVENT_PERIOD_100MS,
    EVENT_PERIOD_500MS,
    NUM_PERIOD_TASKS
} EVENT_IDS;

typedef enum {
    RUN,
    IDLE,
} TASK_STATE;

typedef struct {
    EVENT_IDS    id;
    TASK_STATE   is_running;
    void       (*task_handler)(void);
    uint32_t     period_ms;
    uint32_t     last_run_time_ms;
} period_task_t;

void All_Task_Init(void);
void periodic_event_task_process(void);

#endif