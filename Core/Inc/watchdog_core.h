#ifndef INC_WATCHDOG_CORE_H_
#define INC_WATCHDOG_CORE_H_

/*
 * Hardware-independent part of the task watchdog: it only tracks when each task last
 * checked in and decides which tasks are overdue. No HAL, no FreeRTOS, only <stdint.h>,
 * so it is unit tested on the PC.
 *
 * Time is a free-running 32-bit millisecond counter. Differences are computed with
 * unsigned arithmetic, so the counter wrapping around (every ~49 days) is handled.
 */

#include <stdint.h>

#define WDGCORE_MAX_TASKS 8

typedef struct {
    uint32_t last_ms[WDGCORE_MAX_TASKS];    /* time of the last check-in */
    uint32_t limit_ms[WDGCORE_MAX_TASKS];   /* longest allowed silence */
    uint32_t registered_mask;               /* bit n set: task n is monitored */
} WdgCore;

void WdgCore_Init(WdgCore *w);

/* Start monitoring task 'id'. The silence timer starts at now_ms. */
void WdgCore_Register(WdgCore *w, uint32_t id, uint32_t now_ms, uint32_t limit_ms);

/* Task 'id' is alive. Ignored for unregistered or out-of-range ids. */
void WdgCore_Checkin(WdgCore *w, uint32_t id, uint32_t now_ms);

/* Change the allowed silence of a task (for example when its period changes). */
void WdgCore_SetLimit(WdgCore *w, uint32_t id, uint32_t limit_ms);

/* Bit n is set when task n has been silent for MORE than its limit. 0 means all healthy. */
uint32_t WdgCore_StalledMask(const WdgCore *w, uint32_t now_ms);

#endif /* INC_WATCHDOG_CORE_H_ */
