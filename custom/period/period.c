#include "headfile.h"

extern Com_Interface_t com;
extern void periodic_event_task_init();


void comunitcate(){
	Get_Lwip_Interface()->machine();
}

period_task_t period_tasks[] = {
    { EVENT_COM_MACHINE,  RUN,comunitcate, 50, 0 },
};

#define PERIOD_TASKS_COUNT (sizeof(period_tasks) / sizeof(period_tasks[0]))

void All_Task_Init(void)
{
    Get_Com_Interface()->init();
		Get_Lwip_Interface()->init();
		Get_Can1_Interface()->init();
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