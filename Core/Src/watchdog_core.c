#include "watchdog_core.h"

void WdgCore_Init(WdgCore *w)
{
    for (uint32_t i = 0; i < WDGCORE_MAX_TASKS; i++) {
        w->last_ms[i] = 0;
        w->limit_ms[i] = 0;
    }
    w->registered_mask = 0;
}

void WdgCore_Register(WdgCore *w, uint32_t id, uint32_t now_ms, uint32_t limit_ms)
{
    if (id >= WDGCORE_MAX_TASKS) {
        return;
    }
    w->last_ms[id] = now_ms;
    w->limit_ms[id] = limit_ms;
    w->registered_mask |= (1u << id);
}

void WdgCore_Checkin(WdgCore *w, uint32_t id, uint32_t now_ms)
{
    if (id >= WDGCORE_MAX_TASKS || (w->registered_mask & (1u << id)) == 0) {
        return;
    }
    w->last_ms[id] = now_ms;
}

void WdgCore_SetLimit(WdgCore *w, uint32_t id, uint32_t limit_ms)
{
    if (id >= WDGCORE_MAX_TASKS) {
        return;
    }
    w->limit_ms[id] = limit_ms;
}

uint32_t WdgCore_StalledMask(const WdgCore *w, uint32_t now_ms)
{
    uint32_t stalled = 0;

    for (uint32_t i = 0; i < WDGCORE_MAX_TASKS; i++) {
        if ((w->registered_mask & (1u << i)) == 0) {
            continue;
        }
        if ((uint32_t)(now_ms - w->last_ms[i]) > w->limit_ms[i]) {
            stalled |= (1u << i);
        }
    }
    return stalled;
}
