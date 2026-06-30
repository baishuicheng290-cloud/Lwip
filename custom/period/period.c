#include "headfile.h"

extern Com_Interface_t com;
extern void periodic_event_task_init();


void comunitcate(){
	com.machine();
}

period_task_t period_tasks[] = {
    { EVENT_COM_MACHINE,  RUN,comunitcate, 50, 0 },
};

#define PERIOD_TASKS_COUNT (sizeof(period_tasks) / sizeof(period_tasks[0]))

void All_Task_Init(void)
{
    __HAL_ETH_DMA_ENABLE_IT(&heth, ETH_DMA_IT_NIS | ETH_DMA_IT_R);
    com.init();
		periodic_event_task_init();
}


void periodic_event_task_init(){
	uint32_t current_time = HAL_GetTick();
  for (int i = 0; i < PERIOD_TASKS_COUNT; i++) {
       period_tasks[i].last_run_time_ms = current_time;
  }
}

void periodic_event_task_process(void)
{
    uint32_t current_time = HAL_GetTick();

    for (int i = 0; i < PERIOD_TASKS_COUNT; i++) {
        period_task_t *task = &period_tasks[i];

        if (task->is_running == RUN && task->task_handler != NULL) {
            if (current_time - task->last_run_time_ms >= task->period_ms) {
                task->task_handler();
                task->last_run_time_ms = current_time;
            }
        }
    }
}

void enable_periodic_task(EVENT_IDS event_id)
{
    for (int i = 0; i < PERIOD_TASKS_COUNT; i++) {
        if (period_tasks[i].id == event_id) {
            period_tasks[i].is_running = RUN;
            period_tasks[i].last_run_time_ms = HAL_GetTick();
            break;
        }
    }
}

void disable_periodic_task(EVENT_IDS event_id)
{
    for (int i = 0; i < PERIOD_TASKS_COUNT; i++) {
        if (period_tasks[i].id == event_id) {
            period_tasks[i].is_running = IDLE;
            break;
        }
    }
}